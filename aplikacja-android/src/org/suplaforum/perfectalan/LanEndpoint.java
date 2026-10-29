package org.suplaforum.perfectalan;

import java.net.InetAddress;
import java.net.URI;
import java.net.UnknownHostException;
import java.util.Locale;

/** An endpoint is deliberately limited to LAN hosts; credentials cannot be embedded in its URL. */
public final class LanEndpoint {
    public final String base;
    public final String host;

    private LanEndpoint(String base, String host) { this.base = base; this.host = host; }

    public static LanEndpoint parse(String input) {
        try {
            String raw = input.trim();
            if (!raw.contains("://")) raw = "http://" + raw;
            URI uri = new URI(raw);
            String scheme = uri.getScheme().toLowerCase(Locale.ROOT);
            String host = uri.getHost();
            if (!(scheme.equals("http") || scheme.equals("https")) || host == null ||
                uri.getRawUserInfo() != null || uri.getRawQuery() != null || uri.getRawFragment() != null ||
                !(uri.getRawPath() == null || uri.getRawPath().isEmpty() || uri.getRawPath().equals("/")))
                throw new IllegalArgumentException();
            host = host.toLowerCase(Locale.ROOT);
            int port = uri.getPort();
            if (port == 0 || port > 65535 || port < -1) throw new IllegalArgumentException();
            if (host.matches("[0-9.]+")) {
                String[] parts = host.split("\\.", -1);
                if (parts.length != 4) throw new IllegalArgumentException();
                int[] ip = new int[4];
                for (int i = 0; i < 4; i++) {
                    if (parts[i].isEmpty() || (parts[i].length() > 1 && parts[i].startsWith("0"))) throw new IllegalArgumentException();
                    ip[i] = Integer.parseInt(parts[i]);
                    if (ip[i] > 255 || ip[i] < 0) throw new IllegalArgumentException();
                }
                if (!(ip[0] == 10 || ip[0] == 127 || (ip[0] == 192 && ip[1] == 168) ||
                      (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) || (ip[0] == 169 && ip[1] == 254)))
                    throw new IllegalArgumentException();
            } else if (!(host.matches("[a-z0-9][a-z0-9-]*") || host.matches("[a-z0-9][a-z0-9.-]*\\.local"))) {
                throw new IllegalArgumentException();
            }
            String suffix = port == -1 || (scheme.equals("http") && port == 80) ||
                (scheme.equals("https") && port == 443) ? "" : ":" + port;
            return new LanEndpoint(scheme + "://" + host + suffix, host);
        } catch (Exception e) { throw new IllegalArgumentException("Podaj lokalny adres ESP32, np. http://perfecta-esp.local"); }
    }

    public void requireLocalAddress() throws UnknownHostException {
        InetAddress[] addresses = InetAddress.getAllByName(host);
        for (InetAddress address : addresses)
            if (!(address.isSiteLocalAddress() || address.isLinkLocalAddress() || address.isLoopbackAddress()))
                throw new UnknownHostException("Adres urządzenia nie należy do sieci lokalnej");
    }
}
