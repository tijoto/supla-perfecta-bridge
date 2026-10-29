package org.suplaforum.perfectalan;

import org.json.JSONArray;
import org.json.JSONObject;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/** Runs under Android app_process. All HTTP action requests use in-memory fixtures. */
public final class AndroidChecks {
    static int checks,calls;static String response;static Fake last;static int status=200;
    static void check(boolean value,String text){checks++;if(!value)throw new AssertionError(text);}
    static final class Fake extends HttpURLConnection{
        final ByteArrayOutputStream output=new ByteArrayOutputStream();
        Fake(URL url){super(url);}
        public void connect(){}public boolean usingProxy(){return false;}public void disconnect(){}
        @Override public void setRequestMethod(String method){this.method=method;}
        public int getResponseCode(){return status;}
        public InputStream getInputStream(){return new ByteArrayInputStream(response.getBytes(java.nio.charset.StandardCharsets.UTF_8));}
        public java.io.OutputStream getOutputStream(){return output;}
    }
    static JSONArray fixture()throws Exception{
        JSONArray rows=new JSONArray();for(int n=0;n<13;n++)rows.put(new JSONObject().put("id",10000+n).put("channelNumber",n).put("iodeviceId",9001)
            .put("caption",n>=4&&n<12?(n+5)+": Sensor":"Channel "+n).put("connected",true)
            .put("function",new JSONObject().put("id",n==12?130:n<2?236:n<4?1020:1010).put("name","KNOWN"))
            .put("state",new JSONObject().put("connected",true).put("connectedCode","CONNECTED").put(n==12?"on":"hi",false)));
        return rows;
    }
    public static void main(String[] args)throws Exception{
        HttpURLConnection actual=(HttpURLConnection)new URL("https://cloud.example.invalid").openConnection();
        actual.setRequestMethod("PATCH");check(actual.getRequestMethod().equals("PATCH"),"Android URLConnection accepts PATCH without connecting");actual.disconnect();
        JSONArray rows=fixture();response=rows.toString();JSONObject decoded=SuplaState.decode(response,9001,"https://cloud.example");
        check(decoded.getBoolean("status_fresh"),"complete binary states are fresh");
        check(decoded.getJSONArray("partitions").getJSONObject(1).getInt("alarm")==0,"partition2 quiet");
        check(!decoded.getBoolean("rf_fresh")&&!decoded.getBoolean("diagnostics_fresh"),"do not invent RF or battery");
        check(!decoded.getJSONArray("inputs").getJSONObject(0).getBoolean("status_available"),"unpublished inputs are unavailable");
        check(decoded.getJSONArray("inputs").getJSONObject(8).getBoolean("status_available"),"input9 state available");
        check(!decoded.getJSONArray("inputs").getJSONObject(8).getBoolean("can_assign"),"remote metadata cannot allow assignments");
        rows.getJSONObject(3).getJSONObject("state").put("hi",true);
        decoded=SuplaState.decode(rows.toString(),9001,"https://cloud.example");
        check(decoded.getJSONArray("partitions").getJSONObject(1).getInt("alarm")==68,"partition2 alarm mapping");
        check(decoded.getJSONArray("partitions").getJSONObject(1).getInt("source")==0,"source must remain unknown");
        AlarmLatch latch=new AlarmLatch(0,0);check(latch.accept(decoded.getBoolean("status_fresh"),new int[]{0,68})==2,"same phone alarm latch handles cloud alarm");
        rows.getJSONObject(3).put("state",new JSONObject());decoded=SuplaState.decode(rows.toString(),9001,"https://cloud.example");
        check(!decoded.getBoolean("status_fresh"),"missing alarm state cannot be treated as normal");
        rows=fixture();rows.getJSONObject(12).getJSONObject("function").put("id",140);
        try{SuplaState.decode(rows.toString(),9001,"https://cloud.example");throw new AssertionError("wrong actuator accepted");}catch(IllegalArgumentException expected){checks++;}
        response=fixture().toString();
        CloudWire wire=new CloudWire(CloudEndpoint.parse("https://cloud.example"),"fixture-test-token",url->{calls++;last=new Fake(url);return last;});
        PanelApi api=new PanelApi(new EspApi(LanEndpoint.parse("127.0.0.2"),"test","unused"),wire,"https://cloud.example","supla",9001);
        api.get("/api/state");check(calls==1,"one filtered cloud state request");
        String token=new JSONObject(api.get("/api/session")).getString("token");
        try{api.configureInput(10,false,false,1,token);throw new AssertionError("remote input write accepted");}catch(IllegalStateException expected){checks++;}
        check(calls==1,"blocked remote input edit sends no request");
        token=new JSONObject(api.get("/api/session")).getString("token");
        try{api.configureZ7(true,1,token);throw new AssertionError("remote Z7 write accepted");}catch(IllegalStateException expected){checks++;}
        check(calls==1,"blocked remote Z7 edit sends no request");
        try{api.get("/api/supla");throw new AssertionError("remote configuration path accepted");}catch(IllegalStateException expected){checks++;}
        token=new JSONObject(api.get("/api/session")).getString("token");api.control("arm",token);
        check(calls==2&&last.getRequestMethod().equals("PATCH"),"one explicit action request");
        check(last.output.toString("UTF-8").equals("{\"action\":\"TURN_ON\"}"),"explicit ON action, never toggle");
        check(last.getURL().toString().equals("https://cloud.example/api/v3/channels/10012"),"only verified actuator channel");
        try{api.control("arm",token);throw new AssertionError("action token replay accepted");}catch(IllegalStateException expected){checks++;}
        check(calls==2,"replay does not send another action");
        token=new JSONObject(api.get("/api/session")).getString("token");status=403;
        try{api.control("disarm",token);throw new AssertionError("permission denied lost");}catch(CloudWire.ApiException expected){check(expected.status==403,"control permission error retained");}
        check(calls==3,"failed action is not retried or sent through LAN");
        check(last.output.toString("UTF-8").equals("{\"action\":\"TURN_OFF\"}"),"explicit OFF action");
        System.out.println("PASS "+checks+" Android cloud checks; no real action requests or panel changes");
    }
}
