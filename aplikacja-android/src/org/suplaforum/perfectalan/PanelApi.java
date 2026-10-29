package org.suplaforum.perfectalan;

import android.os.SystemClock;
import org.json.JSONObject;
import java.io.IOException;
import java.util.UUID;

/** Read fallback only. A write is pinned to the transport of its fresh state. */
public final class PanelApi {
    public final LanEndpoint endpoint;public final String cloudEndpoint;
    private final EspApi lan;private final CloudWire cloud;private final String mode;private final int device;
    private boolean remote;private long retryLanAt,stateAt,sessionAt;private int controlChannel;private String session="";
    PanelApi(CredentialStore store)throws Exception{
        endpoint=LanEndpoint.parse(store.endpoint());lan=new EspApi(endpoint,store.username(),store.password());
        mode=store.preferences.getString("connection_mode","lan");device=store.preferences.getInt("supla_device",0);
        cloudEndpoint=store.preferences.getString("supla_endpoint","");
        cloud=store.cloudConfigured()?new CloudWire(CloudEndpoint.parse(cloudEndpoint),store.cloudToken()):null;
    }
    PanelApi(EspApi lan,CloudWire cloud,String cloudEndpoint,String mode,int device){
        this.lan=lan;this.cloud=cloud;this.endpoint=lan.endpoint;this.cloudEndpoint=cloudEndpoint;this.mode=mode;this.device=device;
    }
    public synchronized String get(String path)throws Exception{
        if(path.equals("/api/state")){
            long now=SystemClock.elapsedRealtime();
            if(!mode.equals("supla")&&(!remote||now>=retryLanAt||cloud==null)){
                try{String text=mode.equals("auto")&&cloud!=null?lan.getQuickState():lan.get(path);remote=false;stateAt=SystemClock.elapsedRealtime();return text;}
                catch(IOException e){if(!mode.equals("auto")||cloud==null)throw e;retryLanAt=SystemClock.elapsedRealtime()+30000;}
            }
            if(cloud==null)throw new IllegalStateException("Zapisz token SUPLA w ustawieniach połączenia.");
            remote=true;
            try{JSONObject state=SuplaState.decode(cloud.channels(device),device,cloudEndpoint);controlChannel=state.getInt("remote_control_channel");stateAt=SystemClock.elapsedRealtime();return state.toString();}
            catch(CloudWire.ApiException e){throw e;}catch(IOException e){throw new IllegalStateException("Brak połączenia z SUPLA. Sprawdź internet w telefonie.",e);}
        }
        if(path.equals("/api/session")){
            if(SystemClock.elapsedRealtime()-stateAt>=4000)throw new IllegalStateException("Odczytaj ponownie stan centrali.");
            session=(remote?"supla:":"lan:")+(remote?UUID.randomUUID():new JSONObject(lan.get(path)).getString("token"));sessionAt=SystemClock.elapsedRealtime();return new JSONObject().put("token",session).toString();
        }
        if(remote)throw new IllegalStateException("Ta funkcja wymaga połączenia lokalnego z ESP32.");
        return lan.get(path);
    }
    public synchronized void control(String action,String token)throws Exception{
        checkSession(token);
        if(remote)cloud.control(controlChannel,action);else lan.control(action,token.substring(4));
    }
    private void checkSession(String token){
        if(!session.equals(token)||SystemClock.elapsedRealtime()-sessionAt>=4000||!token.startsWith(remote?"supla:":"lan:"))throw new IllegalStateException("Stan lub połączenie zmieniły się; odczytaj ponownie dane.");
        session=""; // A request is consumed before transmission; it cannot be replayed.
    }
    public synchronized String configureZ7(boolean enabled,long revision,String token)throws Exception{checkSession(token);if(remote)throw new IllegalStateException("Przypisania zmieniaj przez LAN.");return lan.configureZ7(enabled,revision,token.substring(4));}
    public synchronized String configureInput(int n,boolean p1,boolean p2,long revision,String token)throws Exception{checkSession(token);if(remote)throw new IllegalStateException("Przypisania zmieniaj przez LAN.");return lan.configureInput(n,p1,p2,revision,token.substring(4));}
    public synchronized boolean isRemote(){return remote;}
}
