package org.suplaforum.perfectalan;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public final class BootReceiver extends BroadcastReceiver {
    @Override public void onReceive(Context context,Intent intent) {
        CredentialStore store=new CredentialStore(context);
        if(Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())&&store.configured()&&store.preferences.getBoolean("monitor_enabled",true)) {
            try{context.startForegroundService(new Intent(context,MonitorService.class));}
            catch(RuntimeException e){MonitorService.serviceProblem="Otwórz aplikację, aby wznowić monitorowanie po restarcie.";}
        }
    }
}
