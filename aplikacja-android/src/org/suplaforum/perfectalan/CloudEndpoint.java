package org.suplaforum.perfectalan;

import java.net.URI;
import java.util.Locale;

/** Separate HTTPS origin for the explicitly configured SUPLA Cloud. */
public final class CloudEndpoint {
    public final String base;
    private CloudEndpoint(String base){this.base=base;}
    public static CloudEndpoint parse(String input){
        try{
            String value=input.trim();if(!value.contains("://"))value="https://"+value;
            URI u=new URI(value);String host=u.getHost();int port=u.getPort();
            if(!"https".equalsIgnoreCase(u.getScheme())||host==null||u.getRawUserInfo()!=null||u.getRawQuery()!=null||u.getRawFragment()!=null||
                !(u.getRawPath()==null||u.getRawPath().isEmpty()||u.getRawPath().equals("/"))||port==0||port>65535||port< -1)throw new IllegalArgumentException();
            return new CloudEndpoint("https://"+host.toLowerCase(Locale.ROOT)+(port<0||port==443?"":":"+port));
        }catch(Exception e){throw new IllegalArgumentException("Podaj adres HTTPS SUPLA Cloud, zgodny z Twoim kontem");}
    }
}
