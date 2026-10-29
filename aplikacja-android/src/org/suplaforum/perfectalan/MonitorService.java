package org.suplaforum.perfectalan;

import android.Manifest;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.content.res.AssetFileDescriptor;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.PowerManager;
import android.os.SystemClock;
import android.os.VibrationEffect;
import android.os.Vibrator;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

public final class MonitorService extends Service {
    public static final String SILENCE="org.suplaforum.perfectalan.SILENCE",TEST="org.suplaforum.perfectalan.TEST",RELOAD="org.suplaforum.perfectalan.RELOAD";
    private static final String MONITOR_CHANNEL="monitor",ALARM_CHANNEL="alarm";
    public static volatile boolean running,online,sounding,testing;
    public static volatile long lastSuccess,pollCount;
    public static volatile JSONObject latest;
    public static volatile String connectionError="",serviceProblem="",alarmText="";
    public static volatile int latchedMask;
    private final Handler main=new Handler(Looper.getMainLooper());
    private final ScheduledExecutorService worker=Executors.newSingleThreadScheduledExecutor();
    private volatile PanelApi api;
    private volatile int generation;
    private CredentialStore store;
    private AlarmLatch alarm;
    private NotificationManager notifications;
    private PowerManager.WakeLock wakeLock;
    private AudioManager audio;
    private AudioFocusRequest focus;
    private MediaPlayer player;
    private Vibrator vibrator;
    private String notificationText="";
    private long retrySoundAt;
    private int testGeneration;

    @Override public void onCreate() {
        super.onCreate();store=new CredentialStore(this);
        alarm=new AlarmLatch(store.preferences.getInt("alarm_seen_mask",0),store.preferences.getInt("alarm_latched_mask",0));
        latchedMask=alarm.latchedMask;alarmText=store.preferences.getString("alarm_text","");
        notifications=getSystemService(NotificationManager.class);audio=getSystemService(AudioManager.class);vibrator=getSystemService(Vibrator.class);
        NotificationChannel monitoring=new NotificationChannel(MONITOR_CHANNEL,"Monitorowanie centrali",NotificationManager.IMPORTANCE_LOW);
        monitoring.setDescription("Stały odczyt stanu PERFECTY przez LAN lub SUPLA.");monitoring.setSound(null,null);monitoring.enableVibration(false);
        NotificationChannel alarms=new NotificationChannel(ALARM_CHANNEL,"Alarm centrali",NotificationManager.IMPORTANCE_HIGH);
        alarms.setDescription("Wykrycie alarmu strefy; zapętlony sygnał korzysta z głośności alarmów telefonu.");alarms.setSound(null,null);
        notifications.createNotificationChannel(monitoring);notifications.createNotificationChannel(alarms);
        Notification notification=monitorNotification("Łączenie z ESP32…");
        if(Build.VERSION.SDK_INT>=29)startForeground(1,notification,ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE);else startForeground(1,notification);
        PowerManager power=getSystemService(PowerManager.class);
        wakeLock=power.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,"PerfectaLAN:Monitor");wakeLock.setReferenceCounted(false);wakeLock.acquire();
        running=true;serviceProblem="";loadConnection();
        worker.scheduleWithFixedDelay(this::poll,0,1000,TimeUnit.MILLISECONDS);
        if(alarm.active()){notifyAlarm();startSound();}
    }
    @Override public int onStartCommand(Intent intent,int flags,int startId) {
        if(!store.preferences.getBoolean("monitor_enabled",true)){silence();stopSelf();return START_NOT_STICKY;}
        if(intent!=null) {
            if(SILENCE.equals(intent.getAction()))silence();
            else if(RELOAD.equals(intent.getAction()))loadConnection();
            else if(TEST.equals(intent.getAction()))testSound();
        }
        return START_STICKY;
    }
    private void loadConnection() {
        generation++;online=false;latest=null;
        try{api=new PanelApi(store);connectionError="";}
        catch(Exception e){api=null;connectionError="Zapisz ponownie połączenie z ESP32.";}
    }
    private void poll() {
        PanelApi selected=api;int expected=generation;
        if(selected==null)return;
        try {
            JSONObject data=MainActivity.validatedState(selected.get("/api/state"));
            main.post(()->{
                if(!running||expected!=generation)return;
                latest=data;lastSuccess=SystemClock.elapsedRealtime();online=true;connectionError="";pollCount++;
                JSONArray zones=data.optJSONArray("partitions");
                int oldSeen=alarm.observedMask,oldLatched=alarm.latchedMask;
                int triggered=alarm.accept(data.optBoolean("status_fresh"),new int[]{zones.optJSONObject(0).optInt("alarm",-1),zones.optJSONObject(1).optInt("alarm",-1)});
                if(triggered!=0){alarmText=describe(data,alarm.latchedMask);latchedMask=alarm.latchedMask;notifyAlarm();}
                if(oldSeen!=alarm.observedMask||oldLatched!=alarm.latchedMask)saveAlarm();
                if(alarm.active()&&!sounding&&SystemClock.elapsedRealtime()>=retrySoundAt){retrySoundAt=SystemClock.elapsedRealtime()+5000;startSound();}
                updateMonitor(data.optBoolean("status_fresh")?"Centrala online · "+("supla".equals(data.optString("transport"))?"SUPLA / internet":"LAN / ESP32"):"Brak aktualnego stanu centrali");
            });
        }catch(Exception e) {
            String error=e instanceof CloudWire.ApiException?e.getMessage():e instanceof EspApi.ApiException&&((EspApi.ApiException)e).status==401?"Błędny login lub hasło ESP32":
                e instanceof IllegalStateException?e.getMessage():selected.isRemote()?"Brak danych SUPLA. Sprawdź dostęp do internetu.":"Brak połączenia z ESP32. Sprawdź domowe Wi-Fi.";
            main.post(()->{if(!running||expected!=generation)return;online=false;connectionError=error;updateMonitor(error);});
        }
    }
    private String describe(JSONObject state,int mask) {
        StringBuilder text=new StringBuilder();JSONArray zones=state.optJSONArray("partitions"),inputs=state.optJSONArray("inputs");
        for(int i=0;i<2;i++)if((mask&(1<<i))!=0){
            if(text.length()>0)text.append(" · ");JSONObject z=zones.optJSONObject(i);
            text.append(z.optString("name","Strefa "+(i+1)));int source=z.optInt("source");
            if(source>=1&&source<=inputs.length())text.append(": ").append(inputs.optJSONObject(source-1).optString("name","Wejście "+source));
        }
        return text.toString();
    }
    private PendingIntent openApp(){return PendingIntent.getActivity(this,1,new Intent(this,MainActivity.class).setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP|Intent.FLAG_ACTIVITY_CLEAR_TOP),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);}
    private PendingIntent silenceAction(){return PendingIntent.getService(this,2,new Intent(this,MonitorService.class).setAction(SILENCE),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);}
    private Notification monitorNotification(String text) {
        return new Notification.Builder(this,MONITOR_CHANNEL).setSmallIcon(R.drawable.ic_launcher).setContentTitle("Perfecta LAN — monitorowanie")
            .setContentText(text).setContentIntent(openApp()).setOngoing(true).setOnlyAlertOnce(true).setShowWhen(false).setCategory(Notification.CATEGORY_SERVICE).build();
    }
    private void updateMonitor(String text) {if(!text.equals(notificationText)){notificationText=text;notifications.notify(1,monitorNotification(text));}}
    private void notifyAlarm() {
        Notification notification=new Notification.Builder(this,ALARM_CHANNEL).setSmallIcon(R.drawable.ic_launcher).setContentTitle("ALARM PERFECTA")
            .setContentText(alarmText).setStyle(new Notification.BigTextStyle().bigText(alarmText+"\nDźwięk trwa do ręcznego wyciszenia telefonu."))
            .setContentIntent(openApp()).addAction(new Notification.Action.Builder(null,"Wycisz telefon",silenceAction()).build())
            .setCategory(Notification.CATEGORY_ALARM).setVisibility(Notification.VISIBILITY_PRIVATE).setOngoing(true).setOnlyAlertOnce(true).build();
        notifications.notify(2,notification);
    }
    private AudioAttributes alarmAttributes(){return new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_ALARM).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build();}
    private void startSound() {
        if(sounding)return;
        try {
            focus=new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT).setAudioAttributes(alarmAttributes()).setOnAudioFocusChangeListener(change->{}).build();
            if(audio.requestAudioFocus(focus)!=AudioManager.AUDIOFOCUS_REQUEST_GRANTED){serviceProblem="Telefon nie przyznał dostępu do dźwięku alarmu.";return;}
            player=new MediaPlayer();player.setAudioAttributes(alarmAttributes());
            try(AssetFileDescriptor file=getResources().openRawResourceFd(R.raw.alarm_signal)){
                player.setDataSource(file.getFileDescriptor(),file.getStartOffset(),file.getLength());
            }
            player.setLooping(true);player.setVolume(1,1);player.prepare();player.start();sounding=true;
            serviceProblem=audio.getStreamVolume(AudioManager.STREAM_ALARM)==0?"Głośność alarmów telefonu wynosi zero — zwiększ ją w aplikacji.":"";
            if(vibrator!=null&&vibrator.hasVibrator())vibrator.vibrate(VibrationEffect.createWaveform(new long[]{0,600,200,600,500},0),alarmAttributes());
        }catch(Exception e){serviceProblem="Nie uruchomiono dźwięku alarmu. Otwórz aplikację i wykonaj test.";stopSound();}
    }
    private void stopSound() {
        if(player!=null){try{player.stop();}catch(Exception ignored){}player.release();player=null;}
        if(vibrator!=null)vibrator.cancel();if(focus!=null){audio.abandonAudioFocusRequest(focus);focus=null;}sounding=false;
    }
    private void silence() {
        testGeneration++;testing=false;alarm.acknowledge();latchedMask=0;saveAlarm();stopSound();notifications.cancel(2);serviceProblem="";
    }
    private void testSound() {
        if(alarm.active())return;
        final int expected=++testGeneration;testing=true;serviceProblem="Test dźwięku rozpocznie się za 5 sekund i potrwa 3 sekundy.";
        main.postDelayed(()->{
            if(!running||testGeneration!=expected||alarm.active())return;
            startSound();
            main.postDelayed(()->{if(testGeneration==expected){testing=false;if(!alarm.active()){stopSound();serviceProblem="";}}},3000);
        },5000);
    }
    private void saveAlarm(){store.preferences.edit().putInt("alarm_seen_mask",alarm.observedMask).putInt("alarm_latched_mask",alarm.latchedMask).putString("alarm_text",alarmText).commit();}
    @Override public void onDestroy() {
        running=false;online=false;latchedMask=0;testGeneration++;testing=false;worker.shutdownNow();main.removeCallbacksAndMessages(null);stopSound();
        if(wakeLock!=null&&wakeLock.isHeld())wakeLock.release();notifications.cancel(2);stopForeground(STOP_FOREGROUND_REMOVE);super.onDestroy();
    }
    @Override public IBinder onBind(Intent intent){return null;}
    @Override protected void dump(FileDescriptor fd,PrintWriter out,String[] args) {
        out.println("connection_mode="+store.preferences.getString("connection_mode","lan"));
        out.println("state_source="+(latest==null?"none":latest.optString("transport","lan")));
        out.println("running="+running);out.println("online="+online);out.println("poll_count="+pollCount);
        out.println("last_success_age_ms="+(SystemClock.elapsedRealtime()-lastSuccess));out.println("alarm_observed_mask="+alarm.observedMask);
        out.println("alarm_latched_mask="+alarm.latchedMask);out.println("sounding="+sounding);out.println("testing="+testing);
        out.println("media_player_playing="+(player!=null&&player.isPlaying()));out.println("wake_lock="+(wakeLock!=null&&wakeLock.isHeld()));
        out.println("battery_exempt="+getSystemService(PowerManager.class).isIgnoringBatteryOptimizations(getPackageName()));
        out.println("notifications_granted="+(Build.VERSION.SDK_INT<33||checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)==PackageManager.PERMISSION_GRANTED));
        out.println("alarm_volume="+audio.getStreamVolume(AudioManager.STREAM_ALARM)+"/"+audio.getStreamMaxVolume(AudioManager.STREAM_ALARM));
        out.println("connection_error="+connectionError);out.println("service_problem="+serviceProblem);
    }
}
