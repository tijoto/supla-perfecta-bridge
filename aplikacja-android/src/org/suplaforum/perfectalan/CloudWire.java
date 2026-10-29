package org.suplaforum.perfectalan;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.Proxy;
import java.net.URL;
import java.nio.charset.StandardCharsets;

/** One HTTPS request; no redirects, custom certificates or action retries. */
public final class CloudWire {
    interface Factory { HttpURLConnection open(URL url)throws IOException; }
    private final CloudEndpoint endpoint;private final String token;private final Factory factory;
    public static final class ApiException extends IOException {
        public final int status;
        ApiException(int status){super(status==401?"Nieprawidłowy lub wygasły token SUPLA":status==403?"Token SUPLA nie ma wymaganych uprawnień":"SUPLA: błąd HTTP "+status);this.status=status;}
    }
    public CloudWire(CloudEndpoint endpoint,String token){this(endpoint,token,url->(HttpURLConnection)url.openConnection(Proxy.NO_PROXY));}
    CloudWire(CloudEndpoint endpoint,String token,Factory factory){
        if(token==null||token.length()<16||token.length()>4096||token.matches("(?s).*\\s.*"))throw new IllegalArgumentException("Podaj poprawny token SUPLA");
        this.endpoint=endpoint;this.token=token;this.factory=factory;
    }
    public String channels(int device)throws IOException{if(device<=0)throw new IllegalArgumentException();return request("/api/v3/channels?include=state,connected&iodeviceIds="+device,"GET",null);}
    public void control(int channel,String action)throws IOException{
        if(channel<=0||!("arm".equals(action)||"disarm".equals(action)))throw new IllegalArgumentException("Nieprawidłowe polecenie SUPLA");
        request("/api/v3/channels/"+channel,"PATCH","{\"action\":\""+(action.equals("arm")?"TURN_ON":"TURN_OFF")+"\"}");
    }
    private String request(String path,String method,String body)throws IOException{
        HttpURLConnection c=factory.open(new URL(endpoint.base+path));
        c.setConnectTimeout(3500);c.setReadTimeout(4500);c.setInstanceFollowRedirects(false);
        c.setRequestMethod(method);c.setRequestProperty("Authorization","Bearer "+token);c.setRequestProperty("Accept","application/json");c.setRequestProperty("Cache-Control","no-store");
        try{
            if(body!=null){byte[] data=body.getBytes(StandardCharsets.UTF_8);c.setRequestProperty("Content-Type","application/json; charset=UTF-8");c.setDoOutput(true);c.setFixedLengthStreamingMode(data.length);try(java.io.OutputStream out=c.getOutputStream()){out.write(data);}}
            int status=c.getResponseCode();if(status<200||status>=300)throw new ApiException(status);
            InputStream input=c.getInputStream();if(input==null)return "";
            try(InputStream stream=input;ByteArrayOutputStream out=new ByteArrayOutputStream()){
                byte[] bytes=new byte[4096];int n;while((n=stream.read(bytes))!=-1){if(out.size()+n>1048576)throw new IOException("Zbyt duża odpowiedź SUPLA");out.write(bytes,0,n);}
                return new String(out.toByteArray(),StandardCharsets.UTF_8);
            }
        }finally{c.disconnect();}
    }
}
