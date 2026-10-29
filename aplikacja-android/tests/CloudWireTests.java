package org.suplaforum.perfectalan;
import java.net.URL;
import java.net.HttpURLConnection;
import java.io.ByteArrayOutputStream;
import java.io.ByteArrayInputStream;
public final class CloudWireTests {
    static int checks,calls,status=200;static Fake last;
    static void check(boolean v,String why){checks++;if(!v)throw new AssertionError(why);}
    static class Fake extends HttpURLConnection{
        ByteArrayOutputStream out=new ByteArrayOutputStream();boolean closed;
        Fake(URL u){super(u);}public void connect(){}public void disconnect(){closed=true;}public boolean usingProxy(){return false;}
        public void setRequestMethod(String m){method=m;}public int getResponseCode(){return status;}
        public java.io.OutputStream getOutputStream(){return out;}public java.io.InputStream getInputStream(){return new ByteArrayInputStream("[]".getBytes());}
    }
    public static void main(String[] args)throws Exception{
        check(CloudEndpoint.parse("cloud.example").base.equals("https://cloud.example"),"HTTPS default");
        for(String bad:new String[]{"http://cloud.example","https://a:b@cloud.example","https://cloud.example/path","https://cloud.example?token=x","https://cloud.example#x","https://cloud.example:0"})
            try{CloudEndpoint.parse(bad);throw new AssertionError("Unsafe origin");}catch(IllegalArgumentException e){checks++;}
        CloudWire wire=new CloudWire(CloudEndpoint.parse("https://cloud.example"),"fixture-token-only",u->{calls++;last=new Fake(u);return last;});
        check(wire.channels(9001).equals("[]"),"filtered read");check(last.getURL().toString().contains("iodeviceIds=9001"),"single device filter");
        check(!last.getInstanceFollowRedirects(),"no cross-host credential redirects");
        check(last.getRequestProperty("Authorization").equals("Bearer fixture-token-only"),"Bearer token header");
        wire.control(10012,"arm");check(last.getRequestMethod().equals("PATCH"),"PATCH");check(last.out.toString("UTF-8").equals("{\"action\":\"TURN_ON\"}"),"explicit ON");
        check(last.closed,"connection closed");status=403;
        try{wire.control(10012,"disarm");throw new AssertionError("403 lost");}catch(CloudWire.ApiException e){check(e.status==403,"permission error");}
        check(calls==3,"no automatic retries");
        try{wire.control(10012,"toggle");throw new AssertionError("toggle accepted");}catch(IllegalArgumentException e){checks++;}
        check(calls==3,"only explicit on/off");
        System.out.println("PASS "+checks+" cloud transport checks");
    }
}
