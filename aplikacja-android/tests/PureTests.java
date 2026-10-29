package org.suplaforum.perfectalan;

import com.sun.net.httpserver.HttpServer;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicInteger;

public final class PureTests {
    static int checks;
    static void check(boolean condition,String message) { checks++;if(!condition)throw new AssertionError(message); }
    static void invalid(String input) {
        try { LanEndpoint.parse(input); throw new AssertionError("Accepted: "+input); }
        catch(IllegalArgumentException expected){checks++;}
    }
    public static void main(String[] args) throws Exception {
        // Desktop JDK restricts Origin by default; Android's HTTP stack accepts it.
        System.setProperty("sun.net.http.allowRestrictedHeaders", "true");
        check(LanEndpoint.parse("127.0.0.2").base.equals("http://127.0.0.2"),"default scheme");
        check(LanEndpoint.parse("http://127.0.0.2:80/").base.equals("http://127.0.0.2"),"canonical origin");
        check(LanEndpoint.parse("https://perfecta-esp.local:443").base.equals("https://perfecta-esp.local"),"local TLS");
        for(String value:new String[]{"http://8.8.8.8","https://example.com","http://admin:secret@127.0.0.2",
            "http://127.0.0.2/update","http://127.0.0.2/?action=arm","http://127.0.0.2/#password",
            "file:///etc/passwd","http://127.0.0.2:0","http://127.0.0.2:65536","http://127.0.0.999",
            "http://127.0.00.2","http://172.32.0.1","http://172.15.0.1","http://169.253.1.2"})invalid(value);
        for(String value:new String[]{"10.0.0.2","172.16.0.1","172.31.255.255","169.254.1.2","127.0.0.1","perfecta-esp.local"})
            check(LanEndpoint.parse(value).host.equals(value),"allowed LAN endpoint");
        int[] known={0,1,5};
        for(int a:known)for(int b:known)check(PanelPolicy.mayControl(true,0,true,new int[]{a,b},false,0,0,false),"valid state");
        for(int s=-1;s<=255;s++)if(s!=0&&s!=1&&s!=5)
            check(!PanelPolicy.mayControl(true,0,true,new int[]{0,s},false,0,0,false),"unknown partition state");
        check(!PanelPolicy.mayControl(false,0,true,new int[]{0,0},false,0,0,false),"offline");
        check(!PanelPolicy.mayControl(true,4000,true,new int[]{0,0},false,0,0,false),"expired read");
        check(!PanelPolicy.mayControl(true,-1,true,new int[]{0,0},false,0,0,false),"invalid age");
        check(!PanelPolicy.mayControl(true,0,false,new int[]{0,0},false,0,0,false),"stale panel");
        check(!PanelPolicy.mayControl(true,0,true,new int[]{0,0},true,0,0,false),"Soft bridge");
        check(!PanelPolicy.mayControl(true,0,true,new int[]{0,0},false,25,0,false),"active pulse");
        check(!PanelPolicy.mayControl(true,0,true,new int[]{0,0},false,0,1,false),"cooldown");
        check(!PanelPolicy.mayControl(true,0,true,new int[]{0,0},false,0,0,true),"command in progress");
        check(!PanelPolicy.mayControl(true,0,true,new int[]{0},false,0,0,false),"incomplete status");
        AlarmLatch latch=new AlarmLatch(0,0);
        check(latch.accept(false,new int[]{68,68})==0&&!latch.active(),"stale alarm cannot ring");
        check(latch.accept(true,new int[]{0,68})==2&&latch.latchedMask==2,"partition 2 alarm");
        check(latch.accept(true,new int[]{0,0})==0&&latch.active(),"phone keeps ringing after panel reset");
        latch.acknowledge();check(!latch.active(),"manual silence");
        check(latch.accept(true,new int[]{68,0})==1,"new partition 1 alarm");
        latch.acknowledge();
        check(latch.accept(true,new int[]{68,0})==0&&!latch.active(),"acknowledged episode does not ring again");
        latch.accept(false,new int[]{0,0});check(latch.observedMask==1,"offline must not invent alarm end");
        latch.accept(true,new int[]{255,0});check(latch.observedMask==1,"unknown alarm code must not invent alarm end");
        latch.accept(true,new int[]{0,0});check(latch.accept(true,new int[]{68,68})==3,"new alarm after explicit restore");
        for(int code=1;code<256;code++)if(code!=68){AlarmLatch empty=new AlarmLatch(0,0);check(empty.accept(true,new int[]{code,code})==0&&!empty.active(),"unknown code cannot trigger audio");}
        AlarmLatch restored=new AlarmLatch(2,2);check(restored.active(),"latched alarm survives service restart");
        restored.acknowledge();check(restored.accept(true,new int[]{0,68})==0&&!restored.active(),"silenced persisted episode is not replayed");
        AtomicInteger posts=new AtomicInteger(), gets=new AtomicInteger(),configPosts=new AtomicInteger(),sensorPosts=new AtomicInteger();
        HttpServer server=HttpServer.create(new InetSocketAddress("127.0.0.1",0),0);
        server.createContext("/api/state",ex->{
            gets.incrementAndGet();byte[] body="{\"status_fresh\":true}".getBytes(StandardCharsets.UTF_8);
            if(ex.getRequestHeaders().getFirst("Authorization")==null){ex.sendResponseHeaders(401,-1);ex.close();return;}
            ex.sendResponseHeaders(200,body.length);ex.getResponseBody().write(body);ex.close();
        });
        server.createContext("/api/control",ex->{
            posts.incrementAndGet();String body=new String(ex.getRequestBody().readAllBytes(),StandardCharsets.UTF_8);
            boolean valid=ex.getRequestMethod().equals("POST")&&body.equals("action=arm")&&
                "test-session-token".equals(ex.getRequestHeaders().getFirst("X-Perfecta-Token"))&&
                ("http://127.0.0.1:"+server.getAddress().getPort()).equals(ex.getRequestHeaders().getFirst("Origin"));
            byte[] result=(valid?"{\"sent\":true}":"{\"error\":\"bad headers\"}").getBytes(StandardCharsets.UTF_8);
            ex.sendResponseHeaders(valid?200:403,result.length);ex.getResponseBody().write(result);ex.close();
        });
        server.createContext("/api/input7/partition1",ex->{
            configPosts.incrementAndGet();String body=new String(ex.getRequestBody().readAllBytes(),StandardCharsets.UTF_8);
            boolean valid=body.equals("enabled=1&revision=4294967295")&&"test-session-token".equals(ex.getRequestHeaders().getFirst("X-Perfecta-Token"));
            byte[] result=(valid?"{\"busy\":true}":"{\"error\":\"bad config request\"}").getBytes(StandardCharsets.UTF_8);
            ex.sendResponseHeaders(valid?202:400,result.length);ex.getResponseBody().write(result);ex.close();
        });
        server.createContext("/api/inputs/partitions",ex->{
            sensorPosts.incrementAndGet();String body=new String(ex.getRequestBody().readAllBytes(),StandardCharsets.UTF_8);
            boolean valid=ex.getRequestMethod().equals("POST")&&
                (body.equals("number=10&partition1=1&partition2=1&revision=4294967295")||body.equals("number=10&partition1=0&partition2=0&revision=4294967295"))&&
                "test-session-token".equals(ex.getRequestHeaders().getFirst("X-Perfecta-Token"))&&
                ("http://127.0.0.1:"+server.getAddress().getPort()).equals(ex.getRequestHeaders().getFirst("Origin"));
            byte[] result=(valid?"{\"busy\":true}":"{\"error\":\"conflict\"}").getBytes(StandardCharsets.UTF_8);
            ex.sendResponseHeaders(valid?202:409,result.length);ex.getResponseBody().write(result);ex.close();
        });
        server.start();
        try {
            EspApi api=new EspApi(LanEndpoint.parse("http://127.0.0.1:"+server.getAddress().getPort()),"test","password");
            check(posts.get()==0,"construction must not actuate");
            for(int i=0;i<5;i++)check(api.get("/api/state").contains("status_fresh"),"GET state");
            check(gets.get()==5&&posts.get()==0,"polling must not actuate");
            try{api.get("/update");throw new AssertionError("arbitrary path");}catch(IllegalArgumentException expected){checks++;}
            try{api.control("restart","test-session-token");throw new AssertionError("arbitrary action");}catch(IllegalArgumentException expected){checks++;}
            try{api.control("arm","short");throw new AssertionError("missing session");}catch(IllegalArgumentException expected){checks++;}
            check(posts.get()==0,"invalid action must not reach ESP");
            check(api.control("arm","test-session-token").contains("true"),"POST token/body/origin");
            check(posts.get()==1,"single actuator request");
            try{api.control("arm","wrong-session-token");throw new AssertionError("expected 403");}
            catch(EspApi.ApiException expected){check(expected.status==403,"API error status");}
            check(posts.get()==2,"failed actuator request is not retried");
            check(api.configureZ7(true,0xffffffffL,"test-session-token").contains("true"),"config enabled and unsigned revision");
            check(configPosts.get()==1,"one explicit config POST");
            try{api.configureZ7(true,-1,"test-session-token");throw new AssertionError("invalid config revision");}catch(IllegalArgumentException expected){checks++;}
            check(configPosts.get()==1,"invalid configuration does not reach server");
            check(api.configureInput(10,true,true,0xffffffffL,"test-session-token").contains("true"),"sensor assignment body, CSRF and origin");
            check(sensorPosts.get()==1,"one explicit sensor assignment POST");
            try{api.configureInput(10,true,true,1,"test-session-token");throw new AssertionError("expected conflict");}
            catch(EspApi.ApiException expected){check(expected.status==409,"sensor conflict propagated");}
            check(sensorPosts.get()==2,"sensor conflict not retried");
            check(api.configureInput(10,false,false,0xffffffffL,"test-session-token").contains("true"),"both memberships can be cleared explicitly");
            check(sensorPosts.get()==3,"clear-both assignment is one explicit POST");
            for(int n:new int[]{0,17})try{api.configureInput(n,true,true,1,"test-session-token");throw new AssertionError("invalid sensor");}catch(IllegalArgumentException expected){checks++;}
            try{api.configureInput(10,true,false,-1,"test-session-token");throw new AssertionError("invalid revision");}catch(IllegalArgumentException expected){checks++;}
            check(sensorPosts.get()==3,"invalid assignments have no side effect");
        } finally {server.stop(0);}
        System.out.println("PASS "+checks+" checks: local endpoints, stale data guards, bridge lock, CSRF and no automatic control POSTs");
    }
}
