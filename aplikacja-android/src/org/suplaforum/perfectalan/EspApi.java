package org.suplaforum.perfectalan;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.Proxy;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.Base64;

/** Single-request API. Actuator POSTs are never automatically retried. */
public final class EspApi {
    public final LanEndpoint endpoint;
    private final String authorization;

    public static final class ApiException extends IOException {
        public final int status;
        public final String body;
        ApiException(int status, String body) { super("HTTP " + status); this.status = status; this.body = body; }
    }

    public EspApi(LanEndpoint endpoint, String username, String password) {
        this.endpoint = endpoint;
        authorization = "Basic " + Base64.getEncoder().encodeToString((username + ":" + password).getBytes(StandardCharsets.UTF_8));
    }

    public String get(String path) throws IOException {
        if (!(path.equals("/api/state") || path.equals("/api/session") || path.equals("/api/supla")))
            throw new IllegalArgumentException("Niedozwolona ścieżka API");
        return request(path, null, null);
    }
    String getQuickState()throws IOException{return request("/api/state",null,null,700,1200);}

    public String control(String action, String token) throws IOException {
        if (!(action.equals("arm") || action.equals("disarm"))) throw new IllegalArgumentException("Nieprawidłowe polecenie");
        if (token == null || token.length() < 8 || token.length() > 128 || token.contains("\r") || token.contains("\n"))
            throw new IllegalArgumentException("Nieprawidłowa sesja");
        return request("/api/control", "action=" + URLEncoder.encode(action, "UTF-8"), token);
    }
    public String configureZ7(boolean enabled,long revision,String token) throws IOException {
        if(revision<0||revision>0xffffffffL||token==null||token.length()<8||token.length()>128||token.contains("\r")||token.contains("\n"))
            throw new IllegalArgumentException("Nieprawidłowa sesja lub wersja ustawień Z7");
        return request("/api/input7/partition1","enabled="+(enabled?"1":"0")+"&revision="+revision,token);
    }
    public String configureInput(int number,boolean p1,boolean p2,long revision,String token) throws IOException {
        if(number<1||number>16||revision<0||revision>0xffffffffL||token==null||token.length()<8||token.length()>128||token.contains("\r")||token.contains("\n"))
            throw new IllegalArgumentException("Nieprawidłowe przypisanie czujki lub sesja");
        return request("/api/inputs/partitions","number="+number+"&partition1="+(p1?"1":"0")+"&partition2="+(p2?"1":"0")+"&revision="+revision,token);
    }

    private String request(String path, String body, String token) throws IOException {
        return request(path,body,token,2500,3500);
    }
    private String request(String path,String body,String token,int connectTimeout,int readTimeout)throws IOException{
        endpoint.requireLocalAddress();
        HttpURLConnection connection = (HttpURLConnection) new URL(endpoint.base + path).openConnection(Proxy.NO_PROXY);
        connection.setConnectTimeout(connectTimeout);
        connection.setReadTimeout(readTimeout);
        connection.setInstanceFollowRedirects(false);
        connection.setRequestProperty("Authorization", authorization);
        connection.setRequestProperty("Accept", "application/json");
        connection.setRequestProperty("Cache-Control", "no-store");
        connection.setRequestProperty("Connection", "close");
        try {
            if (body != null) {
                connection.setRequestMethod("POST");
                connection.setRequestProperty("X-Perfecta-Token", token);
                connection.setRequestProperty("Origin", endpoint.base);
                connection.setRequestProperty("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
                connection.setDoOutput(true);
                byte[] bytes = body.getBytes(StandardCharsets.UTF_8);
                connection.setFixedLengthStreamingMode(bytes.length);
                try (java.io.OutputStream output = connection.getOutputStream()) { output.write(bytes); }
            }
            int status = connection.getResponseCode();
            InputStream stream = status >= 400 ? connection.getErrorStream() : connection.getInputStream();
            String result = readBounded(stream);
            if (status < 200 || status >= 300) throw new ApiException(status, result);
            return result;
        } finally { connection.disconnect(); }
    }

    private static String readBounded(InputStream input) throws IOException {
        if (input == null) return "";
        try (InputStream stream = input; ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[4096];
            int count;
            while ((count = stream.read(buffer)) != -1) {
                if (output.size() + count > 262144) throw new IOException("Zbyt duża odpowiedź ESP32");
                output.write(buffer, 0, count);
            }
            return new String(output.toByteArray(), StandardCharsets.UTF_8);
        }
    }
}
