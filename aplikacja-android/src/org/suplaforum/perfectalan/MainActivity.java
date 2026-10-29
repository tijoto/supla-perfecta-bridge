package org.suplaforum.perfectalan;

import android.app.Activity;
import android.app.AlertDialog;
import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.Build;
import android.os.PowerManager;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.text.InputType;
import android.media.AudioManager;
import android.net.Uri;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.webkit.HttpAuthHandler;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Spinner;
import android.widget.ArrayAdapter;
import org.json.JSONArray;
import org.json.JSONObject;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

public final class MainActivity extends Activity {
    private static final int BG = Color.rgb(16,24,36), CARD = Color.rgb(27,43,59), TEXT = Color.rgb(232,240,249);
    private static final int MUTED = Color.rgb(165,185,204), GREEN = Color.rgb(107,213,174), RED = Color.rgb(255,137,143), AMBER = Color.rgb(255,212,132);
    private final Handler ui = new Handler(Looper.getMainLooper());
    private final ScheduledExecutorService worker = Executors.newSingleThreadScheduledExecutor();
    private ScheduledFuture<?> polling;
    private CredentialStore store;
    private volatile PanelApi api;
    private volatile boolean foreground;
    private boolean busy, online, showAll;
    private volatile int generation;
    private JSONObject current;
    private long lastSuccess;
    private TextView connection, command, details;
    private TextView monitorStatus;
    private CheckBox includePartition1;
    private TextView z7Status;
    private long pendingZ7Id;
    private Button silence;
    private TextView[] zoneTitle = new TextView[2], zoneState = new TextView[2], zoneAlarm = new TextView[2], zoneSource = new TextView[2];
    private InputRow[] inputRows = new InputRow[16];
    private Button arm, disarm, notifications;
    private LinearLayout root;
    private AlertDialog connectionDialog, notificationDialog;
    private AlertDialog sensorDialog;
    private CheckBox sensorP1,sensorP2,sensorBlocked;
    private int sensorNumber;
    private long sensorRevision;
    private WebView notificationWeb;
    private String connectionFailure = "";
    private final Runnable ticker = new Runnable() {
        public void run() {
            if (!foreground) return;
            if(monitorEnabled()&&api!=null) {
                JSONObject data=MonitorService.latest;
                if(data!=null){current=data;lastSuccess=MonitorService.lastSuccess;}
                online=MonitorService.running&&MonitorService.online;
                connectionFailure=MonitorService.connectionError;render();
            }
            if (online && SystemClock.elapsedRealtime() - lastSuccess >= 4000) { online = false; render(); }
            refreshMonitor();
            enableButtons(); ui.postDelayed(this, 1000);
        }
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        store = new CredentialStore(this);
        showAll = store.preferences.getBoolean("show_all", false);
        getWindow().setStatusBarColor(BG); getWindow().setNavigationBarColor(BG);
        setVolumeControlStream(AudioManager.STREAM_ALARM);
        buildUi();
        if (store.configured()) {
            try { api = new PanelApi(store); }
            catch (Exception e) { connectionFailure = "Otwórz Połączenie i ponownie zapisz hasło."; }
        } else ui.post(() -> showConnection());
        render();
    }

    @Override protected void onResume() {
        super.onResume(); foreground = true; startPolling(); ui.removeCallbacks(ticker); ui.post(ticker);
        if (notificationWeb != null) notificationWeb.onResume();
        if(api!=null&&Build.VERSION.SDK_INT>=33&&checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED&&!store.preferences.getBoolean("notifications_asked",false)) {
            store.preferences.edit().putBoolean("notifications_asked",true).apply();
            requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS},51);
        }
    }
    @Override protected void onPause() {
        foreground = false; stopPolling(); ui.removeCallbacks(ticker);
        if (notificationWeb != null) notificationWeb.onPause();
        super.onPause();
    }
    @Override protected void onDestroy() {
        stopPolling(); worker.shutdownNow(); ui.removeCallbacksAndMessages(null);
        if (notificationDialog != null) notificationDialog.dismiss();
        if (connectionDialog != null) connectionDialog.dismiss();
        if (sensorDialog != null) sensorDialog.dismiss();
        super.onDestroy();
    }

    private int dp(float value) { return Math.round(getResources().getDisplayMetrics().density * value); }
    private LinearLayout column() { LinearLayout v = new LinearLayout(this); v.setOrientation(LinearLayout.VERTICAL); return v; }
    private LinearLayout row() { LinearLayout v = new LinearLayout(this); v.setOrientation(LinearLayout.HORIZONTAL); v.setGravity(Gravity.CENTER_VERTICAL); return v; }
    private TextView label(String value, float size, int color) {
        TextView v = new TextView(this); v.setText(value); v.setTextColor(color); v.setTextSize(size); v.setPadding(0,dp(1),0,dp(1)); return v;
    }
    private void put(TextView view,String value){if(!value.contentEquals(view.getText()))view.setText(value);}
    private GradientDrawable background(int color, int border) {
        GradientDrawable d = new GradientDrawable(); d.setColor(color); d.setCornerRadius(dp(9));
        if (border != 0) d.setStroke(dp(1), border); return d;
    }
    private LinearLayout card() {
        LinearLayout v = column(); v.setPadding(dp(10),dp(8),dp(10),dp(8)); v.setBackground(background(CARD,0)); return v;
    }
    private Button button(String text, int color, int id) {
        Button b = new Button(this); b.setText(text); b.setTextSize(12); b.setTextColor(Color.WHITE); b.setAllCaps(false);
        b.setPadding(dp(8),dp(4),dp(8),dp(4)); b.setMinHeight(dp(44)); b.setMinimumHeight(dp(44));
        b.setBackground(background(color,0)); if (id != 0) b.setId(id); return b;
    }
    private void full(View v, LinearLayout parent, int marginTop) {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(-1,-2); p.topMargin = dp(marginTop); parent.addView(v,p);
    }
    private void half(View v, LinearLayout parent, boolean gap) {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0,-2,1); if(gap) p.leftMargin=dp(7); parent.addView(v,p);
    }
    private void buildUi() {
        root = column(); root.setBackgroundColor(BG);
        root.setOnApplyWindowInsetsListener((v,insets) -> {
            v.setPadding(dp(12)+insets.getSystemWindowInsetLeft(),insets.getSystemWindowInsetTop(),dp(12)+insets.getSystemWindowInsetRight(),insets.getSystemWindowInsetBottom());
            return insets.consumeSystemWindowInsets();
        });
        setContentView(root);
        LinearLayout header = row(); header.setPadding(0,dp(10),0,dp(4));
        TextView title = label("Perfecta LAN",22,TEXT); title.setTypeface(null,Typeface.BOLD);
        header.addView(title,new LinearLayout.LayoutParams(0,-2,1));
        Button settings = button("Połączenie",Color.rgb(42,70,94),R.id.connection_settings);
        settings.setOnClickListener(v -> showConnection()); header.addView(settings);
        full(header,root,0);
        connection = label("Oczekiwanie na połączenie…",12,MUTED); connection.setId(R.id.online_status); full(connection,root,2);
        ScrollView scroll = new ScrollView(this); scroll.setFillViewport(false);
        root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout body = column(); body.setPadding(0,dp(4),0,dp(12)); scroll.addView(body);
        LinearLayout zones = row(); zones.setGravity(Gravity.TOP);
        for(int i=0;i<2;i++) {
            LinearLayout c=card(); zoneTitle[i]=label("Strefa "+(i+1),14,TEXT); zoneTitle[i].setTypeface(null,Typeface.BOLD);
            LinearLayout zoneHeader=row();zoneHeader.addView(zoneTitle[i],new LinearLayout.LayoutParams(0,-2,1));
            if(i==0){
                includePartition1=new CheckBox(this);includePartition1.setId(R.id.z7_partition1);includePartition1.setText("Uzbrajaj");
                includePartition1.setTextSize(10);includePartition1.setTextColor(TEXT);includePartition1.setEnabled(false);
                includePartition1.setMinWidth(0);includePartition1.setMinimumWidth(0);includePartition1.setPadding(0,0,0,0);
                includePartition1.setContentDescription("Uzbrajaj również strefę 1 — przypisanie Z7 do S1");
                includePartition1.setOnClickListener(v->confirmZ7(includePartition1.isChecked()));zoneHeader.addView(includePartition1);
            }
            zoneState[i]=label("Nieodczytana",18,MUTED); zoneState[i].setId(i==0?R.id.zone1_status:R.id.zone2_status);
            zoneAlarm[i]=label("Alarm: brak danych",12,MUTED); zoneSource[i]=label("",11,MUTED);
            c.addView(zoneHeader); c.addView(zoneState[i]); c.addView(zoneAlarm[i]); c.addView(zoneSource[i]); half(c,zones,i>0);
        }
        full(zones,body,5);
        LinearLayout controls=row(); arm=button("Załącz czuwanie",Color.rgb(29,123,83),R.id.arm_button);
        disarm=button("Wyłącz czuwanie",Color.rgb(159,48,59),R.id.disarm_button);
        arm.setOnClickListener(v -> confirmAction("arm")); disarm.setOnClickListener(v -> confirmAction("disarm"));
        half(arm,controls,false); half(disarm,controls,true); full(controls,body,8);
        command=label("Polecenia wymagają potwierdzenia. Stan pochodzi z centrali.",11,MUTED); command.setId(R.id.command_status); full(command,body,4);
        z7Status=label("Odczyt przypisania Z7…",10,MUTED);z7Status.setId(R.id.z7_status);full(z7Status,body,0);
        LinearLayout monitoring=card();CheckBox enabled=new CheckBox(this);enabled.setId(R.id.monitor_enabled);
        enabled.setText("Monitoruj w tle i alarmuj telefonem");enabled.setTextSize(12);enabled.setTextColor(TEXT);enabled.setChecked(monitorEnabled());monitoring.addView(enabled);
        enabled.setOnCheckedChangeListener((v,on)->{
            store.preferences.edit().putBoolean("monitor_enabled",on).apply();
            if(!on){store.preferences.edit().putInt("alarm_seen_mask",0).putInt("alarm_latched_mask",0).apply();stopService(new Intent(this,MonitorService.class));}
            startPolling();refreshMonitor();
        });
        monitorStatus=label("",11,MUTED);monitorStatus.setId(R.id.monitor_status);monitoring.addView(monitorStatus);
        LinearLayout monitorButtons=row();Button test=button("Test za 5 s (3 s)",Color.rgb(42,70,94),R.id.test_sound);
        test.setOnClickListener(v->{if(api!=null&&monitorEnabled())startForegroundService(new Intent(this,MonitorService.class).setAction(MonitorService.TEST));});
        Button soundSettings=button("Alarm telefonu",Color.rgb(42,70,94),0);soundSettings.setOnClickListener(v->showSoundSettings());
        half(test,monitorButtons,false);half(soundSettings,monitorButtons,true);full(monitorButtons,monitoring,4);
        silence=button("Wycisz telefon",Color.rgb(159,48,59),R.id.silence_alarm);silence.setOnClickListener(v->startForegroundService(new Intent(this,MonitorService.class).setAction(MonitorService.SILENCE)));
        full(silence,monitoring,5);silence.setVisibility(View.GONE);full(monitoring,body,6);
        LinearLayout inputHeader=row(); TextView name=label("Czujki",16,TEXT); name.setTypeface(null,Typeface.BOLD);
        inputHeader.addView(name,new LinearLayout.LayoutParams(0,-2,1));
        CheckBox all=new CheckBox(this); all.setText("Wejścia 1–16"); all.setTextSize(11); all.setTextColor(MUTED); all.setChecked(showAll);
        all.setOnCheckedChangeListener((b,checked)->{showAll=checked;store.preferences.edit().putBoolean("show_all",checked).apply();render();});
        inputHeader.addView(all); full(inputHeader,body,6);
        full(label("Przytrzymaj czujkę: S1 / S2 / stan blokady",10,MUTED),body,0);
        for(int i=0;i<16;i++) { inputRows[i]=new InputRow(i+1); full(inputRows[i].layout,body,4); }
        notifications=button("Powiadomienia — dzień / noc / harmonogram",Color.rgb(42,70,94),R.id.notifications_button);
        notifications.setOnClickListener(v -> showNotifications()); full(notifications,body,9);
        details=label("",10,MUTED); full(details,body,6);
    }

    private final class InputRow {
        final int number; final LinearLayout layout; final TextView name,status,extra,batteryDate;
        InputRow(int number) {
            this.number=number; layout=card(); layout.setPadding(dp(9),dp(5),dp(9),dp(5));
            layout.setId(getResources().getIdentifier("input_card_"+number,"id",getPackageName()));
            layout.setOnLongClickListener(v->{showInputSettings(number);return true;});
            LinearLayout top=row(); name=label(number+". Wejście",13,TEXT); name.setTypeface(null,Typeface.BOLD);
            top.addView(name,new LinearLayout.LayoutParams(0,-2,1)); status=label("Brak danych",11,MUTED); top.addView(status);
            layout.addView(top); extra=label("RF: —  ·  Bateria: —",10,MUTED); layout.addView(extra);
            batteryDate=label("",10,AMBER); layout.addView(batteryDate);
        }
        void update(JSONObject item,boolean fresh,boolean diagFresh,boolean rfFresh) {
            layout.setVisibility(showAll||number>=9?View.VISIBLE:View.GONE);
            put(name,number+". "+item.optString("name","Wejście "+number));
            boolean configured=item.optBoolean("configured",false), violated=item.optBoolean("violated"), blocked=item.optBoolean("blocked");
            boolean available=item.optBoolean("status_available",true);
            String state=!fresh?"Nieodczytany":!available?"Niedostępna w SUPLA":!configured?"Nieużywane":blocked?"Zablokowana":violated?"NARUSZENIE":"Spokojna";
            put(status,state); status.setTextColor(!fresh?AMBER:violated?AMBER:blocked?MUTED:GREEN);
            String battery=!diagFresh?"brak danych":item.optBoolean("low_battery")?"SŁABA":"OK";
            String rf=item.optBoolean("wireless")&&rfFresh?item.optInt("rf")+"%":"—";
            String membership=item.optBoolean("partition_settings_ready")?((item.optBoolean("partition1")?"S1 ":"")+(item.optBoolean("partition2")?"S2":"")).trim():"—";
            if(membership.isEmpty())membership="Bez stref";
            put(extra,remoteState()?"Stan kanału SUPLA · historia i RF przez LAN":"RF: "+rf+"  ·  Bateria: "+battery+"  ·  "+membership+"\nNaruszenie: "+date(item.optLong("last_violation")));
            extra.setTextColor(diagFresh&&item.optBoolean("low_battery")?AMBER:MUTED);
            layout.setBackground(background(CARD,diagFresh&&item.optBoolean("low_battery")?AMBER:blocked?MUTED:0));
            long low=item.optLong("last_battery_low"); put(batteryDate,low>0?"Słaba bateria: "+date(low):"");
            batteryDate.setVisibility(low>0?View.VISIBLE:View.GONE);
        }
    }

    private JSONObject inputSettings(JSONObject state,int number){
        JSONArray inputs=state==null?null:state.optJSONArray("inputs");
        return inputs!=null&&number>=1&&number<=inputs.length()?inputs.optJSONObject(number-1):null;
    }
    private CheckBox inputOption(String text,int id,boolean checked){
        CheckBox box=new CheckBox(this);box.setText(text);box.setTextSize(14);box.setTextColor(TEXT);box.setChecked(checked);box.setId(id);return box;
    }
    private void showInputSettings(int number){
        if(remoteState()){
            new AlertDialog.Builder(this).setTitle("Wejście "+number).setMessage("Edycja wejść i przypisań S1/S2 jest dostępna tylko przez LAN. Połączenie SUPLA służy do odczytu, załączania i wyłączania czuwania.")
                .setPositiveButton("Zamknij",null).show();return;
        }
        if(sensorDialog!=null&&sensorDialog.isShowing())return;
        JSONObject input=inputSettings(current,number);if(input==null)return;
        boolean ready=fresh()&&input.optBoolean("partition_settings_ready");
        boolean writable=ready&&!busy&&input.optBoolean("can_assign");
        final boolean old1=input.optBoolean("partition1"),old2=input.optBoolean("partition2");
        final long revision=input.optLong("partition_revision",-1);
        LinearLayout content=column();content.setPadding(dp(18),dp(8),dp(18),dp(10));
        LinearLayout options=row();
        CheckBox p1=inputOption("S1",R.id.sensor_partition1,old1),p2=inputOption("S2",R.id.sensor_partition2,old2);
        CheckBox blocked=inputOption("Blokada",R.id.sensor_blocked,fresh()&&input.optBoolean("blocked"));
        p1.setEnabled(writable);p2.setEnabled(writable);blocked.setEnabled(false);
        options.addView(p1);options.addView(p2);options.addView(blocked);content.addView(options);
        String info="S1/S2: przypisanie czujki do stref. Zapis wymaga rozbrojenia obu stref.\n\nBlokada: tylko odczyt stanu z centrali. Polecenie blokowania po UART nie jest potwierdzone. Blokadę ustaw na manipulatorze: menu użytkownika → 4. Blokady wejść.";
        if(!ready)info="Brak aktualnych ustawień czujki. Poczekaj na odczyt.\n\n"+info;
        else if(!writable)info="Edycja niedostępna przy obecnym stanie centrali lub dla tego typu wejścia.\n\n"+info;
        if(current.optBoolean("diagnostics_fresh")&&input.optBoolean("low_battery"))
            info+="\n\nSŁABA BATERIA tej czujki. Blokadę możesz ustawić na manipulatorze. Baterię tej czujki trzeba wymienić.";
        full(label(info,12,MUTED),content,6);
        AlertDialog dialog=new AlertDialog.Builder(this).setTitle(number+". "+input.optString("name","Czujka"))
            .setView(content).setNegativeButton("Anuluj",null).setPositiveButton("Zapisz S1/S2",null).create();
        sensorDialog=dialog;sensorNumber=number;sensorRevision=revision;sensorP1=p1;sensorP2=p2;sensorBlocked=blocked;
        dialog.setOnDismissListener(d->{sensorDialog=null;sensorP1=sensorP2=sensorBlocked=null;});
        dialog.setOnShowListener(d->{
            Button save=dialog.getButton(AlertDialog.BUTTON_POSITIVE);save.setId(R.id.save_sensor);
            Runnable changed=()->refreshInputDialog();
            p1.setOnCheckedChangeListener((v,on)->changed.run());p2.setOnCheckedChangeListener((v,on)->changed.run());changed.run();
            save.setOnClickListener(v->{
                final boolean desired1=p1.isChecked(),desired2=p2.isChecked();dialog.dismiss();
                String before=(old1?"S1 ":"")+(old2?"S2":""),after=(desired1?"S1 ":"")+(desired2?"S2":"");
                String message="Przypisanie: "+(before.isEmpty()?"Brak stref":before.trim())+" → "+(after.isEmpty()?"Brak stref":after.trim())+
                    ".\n\nZmiana zostanie zapisana w centrali. To trwałe przypisanie, oddzielne od blokady wejścia.";
                if(!desired1&&!desired2)message+="\n\nCzujka nie będzie przypisana do żadnej strefy. Rejestracja radiowa i zgłaszanie usterek pozostaną aktywne.";
                new AlertDialog.Builder(this).setTitle("Zmienić strefy czujki "+number+"?")
                    .setMessage(message)
                    .setNegativeButton("Anuluj",null).setPositiveButton("Zapisz w centrali",(x,w)->writeInputPartitions(number,desired1,desired2,revision)).show();
            });
        });dialog.show();
    }
    private void refreshInputDialog(){
        if(sensorDialog==null||!sensorDialog.isShowing()||sensorP1==null)return;
        JSONObject input=inputSettings(current,sensorNumber);boolean ready=fresh()&&input!=null;
        sensorBlocked.setText(ready?"Blokada":"Blokada (?)");sensorBlocked.setChecked(ready&&input.optBoolean("blocked"));
        boolean writable=ready&&!busy&&input.optBoolean("partition_settings_ready")&&input.optBoolean("can_assign")&&input.optLong("partition_revision",-2)==sensorRevision;
        sensorP1.setEnabled(writable);sensorP2.setEnabled(writable);
        Button save=sensorDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        if(save!=null)save.setEnabled(writable&&
            (sensorP1.isChecked()!=input.optBoolean("partition1")||sensorP2.isChecked()!=input.optBoolean("partition2")));
    }
    private void writeInputPartitions(int number,boolean p1,boolean p2,long revision){
        if(api==null||busy)return;
        final PanelApi selected=api;final int expected=generation;busy=true;enableButtons();
        command.setText("Sprawdzanie ustawień czujki "+number+"…");
        worker.execute(()->{
            try{
                JSONObject state=validatedState(selected.get("/api/state"));JSONObject input=inputSettings(state,number);
                if(input==null||!state.optBoolean("status_fresh")||!input.optBoolean("can_assign")||input.optLong("partition_revision",-2)!=revision)
                    throw new IllegalStateException("Ustawienia lub stan centrali zmieniły się. Odczytaj ponownie dane.");
                long checkedAt=SystemClock.elapsedRealtime();String token=new JSONObject(selected.get("/api/session")).getString("token");
                if(!foreground||expected!=generation||SystemClock.elapsedRealtime()-checkedAt>=4000)throw new IllegalStateException("Przerwano zapis przed wysłaniem polecenia.");
                JSONObject result=new JSONObject(selected.configureInput(number,p1,p2,revision,token));
                ui.post(()->{if(expected!=generation||isFinishing())return;
                    pendingZ7Id=result.optBoolean("busy")?result.optLong("id"):0;busy=pendingZ7Id!=0;
                    command.setText(busy?"Zapis i kontrola przypisania czujki "+number+"…":"Przypisanie nie wymagało zmiany.");enableButtons();});
            }catch(Exception e){String message=errorText(e);ui.post(()->{if(expected!=generation||isFinishing())return;busy=false;command.setText("Czujka "+number+": "+message+" Nie ponowiono zapisu.");enableButtons();});}
            if(!monitorEnabled())poll(selected,expected);
        });
    }

    private String date(long seconds) {
        if(seconds<=0)return "brak zapisu";
        SimpleDateFormat format=new SimpleDateFormat("dd.MM.yyyy HH:mm:ss",new Locale("pl","PL"));
        format.setTimeZone(TimeZone.getTimeZone("Europe/Warsaw")); return format.format(new Date(seconds*1000));
    }
    private void startPolling() {
        stopPolling(); if(api==null||!foreground||worker.isShutdown())return;
        if(monitorEnabled()){ensureMonitor(false);return;}
        final int expected=generation; final PanelApi selected=api;
        polling=worker.scheduleWithFixedDelay(()->poll(selected,expected),0,1000,TimeUnit.MILLISECONDS);
    }
    private void stopPolling() { if(polling!=null){polling.cancel(false);polling=null;} }
    private void poll(PanelApi selected,int expected) {
        if(!foreground||expected!=generation)return;
        try {
            JSONObject state=validatedState(selected.get("/api/state"));
            ui.post(()->{
                if(!foreground||expected!=generation)return;
                current=state; lastSuccess=SystemClock.elapsedRealtime(); online=true; connectionFailure="";
                if(command.getText().toString().startsWith("Połączenie zapisane."))command.setText("Polecenia wymagają potwierdzenia. Stan pochodzi z centrali.");
                render();
            });
        } catch(Exception error) {
            String message=errorText(error);
            ui.post(()->{if(expected!=generation||!foreground)return;online=false;connectionFailure=message;render();});
        }
    }
    static JSONObject validatedState(String body) throws Exception {
        JSONObject state=new JSONObject(body); JSONArray zones=state.getJSONArray("partitions"),inputs=state.getJSONArray("inputs");
        if(zones.length()!=2||inputs.length()!=16)throw new IllegalArgumentException("Nieprawidłowe dane centrali");
        for(int i=0;i<2;i++)if(zones.getJSONObject(i).getInt("number")!=i+1)throw new IllegalArgumentException("Nieprawidłowe numery stref");
        for(int i=0;i<16;i++)if(inputs.getJSONObject(i).getInt("number")!=i+1)throw new IllegalArgumentException("Nieprawidłowe numery wejść");
        state.getJSONObject("bridge");state.getJSONObject("control");state.getBoolean("status_fresh");return state;
    }
    private boolean fresh() { return online&&current!=null&&current.optBoolean("status_fresh")&&SystemClock.elapsedRealtime()-lastSuccess<4000; }
    private static boolean permitted(JSONObject state,boolean online,long age,boolean busy) {
        if(state==null)return false;
        JSONArray zones=state.optJSONArray("partitions"); if(zones==null||zones.length()!=2)return false;
        int[] states={zones.optJSONObject(0).optInt("state",-1),zones.optJSONObject(1).optInt("state",-1)};
        JSONObject bridge=state.optJSONObject("bridge"),control=state.optJSONObject("control"); if(bridge==null||control==null)return false;
        if(control.has("enabled")&&!control.optBoolean("enabled"))return false;
        JSONObject z7=state.optJSONObject("input7_arming");if(z7!=null&&z7.optBoolean("busy"))return false;
        return PanelPolicy.mayControl(online,age,state.optBoolean("status_fresh"),states,bridge.optBoolean("connected",true),
            control.optInt("active_gpio",-1),control.optLong("cooldown_ms",Long.MAX_VALUE),busy);
    }
    private void enableButtons() {
        boolean enabled=permitted(current,online,SystemClock.elapsedRealtime()-lastSuccess,busy);
        arm.setEnabled(enabled);disarm.setEnabled(enabled);arm.setAlpha(enabled?1:.4f);disarm.setAlpha(enabled?1:.4f);
        boolean local=api!=null&&!remoteState();notifications.setEnabled(local);notifications.setAlpha(local?1:.4f);
    }
    private boolean remoteState(){return current!=null&&"supla".equals(current.optString("transport"));}
    private void render() {
        boolean fresh=fresh();
        String route=remoteState()?"SUPLA · internet":"LAN · ESP32";
        String text=api==null?"Ustaw połączenie":fresh?"● Centrala online  ·  "+route:online?"● "+route+" · brak aktualnego stanu centrali":"● Brak połączenia z centralą";
        if(!connectionFailure.isEmpty())text+="\n"+connectionFailure;
        put(connection,text);connection.setTextColor(fresh?GREEN:AMBER);
        JSONObject s=current==null?new JSONObject():current;
        JSONArray zones=s.optJSONArray("partitions"),inputs=s.optJSONArray("inputs");
        for(int i=0;i<2;i++) {
            JSONObject z=zones==null?new JSONObject():zones.optJSONObject(i); if(z==null)z=new JSONObject();
            put(zoneTitle[i],z.optString("name","Strefa "+(i+1)));
            int state=z.optInt("state",-1),alarm=z.optInt("alarm",-1),source=z.optInt("source");
            put(zoneState[i],!fresh?"Nieodczytana":state==0?"Rozbrojona":state==1?"Czas na wyjście":state==5?"CZUWA":"Stan "+state);
            zoneState[i].setTextColor(!fresh?MUTED:state==5?GREEN:state==1?AMBER:TEXT);
            put(zoneAlarm[i],!fresh?"Alarm: brak danych":alarm==68?"ALARM":alarm==0?"Brak alarmu":"Alarm: kod "+alarm);
            zoneAlarm[i].setTextColor(fresh&&alarm!=0?RED:MUTED);
            String sourceName="";
            if(inputs!=null&&source>=1&&source<=inputs.length())sourceName=inputs.optJSONObject(source-1).optString("name","");
            put(zoneSource[i],source>0?"Ostatnie źródło: "+source+" "+sourceName:remoteState()&&alarm==68?"Źródło alarmu: dostępne przez LAN":"");
            zoneSource[i].setVisibility(source>0||(remoteState()&&alarm==68)?View.VISIBLE:View.GONE);
        }
        for(int i=0;i<16;i++) {
            JSONObject input=inputs==null?new JSONObject():inputs.optJSONObject(i);if(input==null)input=new JSONObject();
            inputRows[i].update(input,fresh,online&&s.optBoolean("diagnostics_fresh"),online&&s.optBoolean("rf_fresh"));
        }
        JSONObject bridge=s.optJSONObject("bridge"); boolean inBridge=bridge!=null&&bridge.optBoolean("connected");
        String clock=s.optString("panel_clock","—");if(clock.length()>16)clock=clock.substring(0,16);
        put(details,remoteState()?s.optString("transport_endpoint")+"\nStany i sterowanie przez SUPLA. RF, historia, przypisania i harmonogramy są dostępne przez LAN.\n"+s.optString("remote_note"):(api==null?store.endpoint():api.endpoint.base)+"  ·  "+s.optString("version","")+"\n"+
            (inBridge?"Połączenie PERFECTA Soft — sterowanie wstrzymane":"Podgląd API ESP32")+"  ·  Czas centrali: "+clock+
            "\nRF i bateria: co 5 s. RF nie potwierdza naruszenia. PUSH dostarcza SUPLA.");
        renderZ7();enableButtons();refreshMonitor();refreshInputDialog();
    }

    private void confirmAction(String action) {
        if(!permitted(current,online,SystemClock.elapsedRealtime()-lastSuccess,busy))return;
        new AlertDialog.Builder(this).setTitle(action.equals("arm")?"Załączyć czuwanie?":"Wyłączyć czuwanie?")
            .setMessage("Polecenie zostanie przekazane do centrali. Rzeczywisty stan sprawdzisz na kartach stref.")
            .setNegativeButton("Anuluj",null).setPositiveButton("Potwierdź",(d,w)->sendAction(action)).show();
    }
    private void renderZ7() {
        includePartition1.setVisibility(remoteState()?View.GONE:View.VISIBLE);
        if(remoteState()){includePartition1.setEnabled(false);put(z7Status,"Przypisania Z7 i czujek zmieniaj przez połączenie LAN.");return;}
        JSONObject data=current==null?null:current.optJSONObject("input7_arming");
        if(data==null){includePartition1.setEnabled(false);put(z7Status,"Wymagane rozszerzenie API ESP32 do zapisu przypisania Z7.");return;}
        if(pendingZ7Id!=0&&data.optLong("id")==pendingZ7Id&&!data.optBoolean("busy")){
            pendingZ7Id=0;busy=false;put(command,data.optString("status").equals("success")?"Przypisanie potwierdzone odczytem z centrali.":"Nie potwierdzono zmiany. Sprawdź przypisanie w centrali.");
        }
        includePartition1.setChecked(data.optBoolean("partition1"));
        includePartition1.setEnabled(fresh()&&!busy&&data.optBoolean("ready")&&data.optBoolean("supported")&&data.optBoolean("can_write"));
        String text;
        String result=data.optString("status");
        if(data.optBoolean("busy")) {
            String phase=result.equals("writing")?"Zapisywanie przypisania":result.equals("backup")?"Przygotowanie kopii":
                result.equals("verifying")||result.equals("recovering")?"Sprawdzanie wyniku":"Odczyt ustawień";
            text="Wejście "+data.optInt("input_number",7)+": "+phase+"… "+Math.min(100,data.optInt("read_bytes")*100/Math.max(1,data.optInt("total_bytes",7168)))+"%";
        }
        else if(!data.optBoolean("ready"))text="Oczekiwanie na aktualny odczyt przypisania Z7…";
        else if(!data.optBoolean("supported"))text="Zapis wymaga Z7: obwód NO, reakcja 14 i przypisanie do S2.";
        else text="Z7 → "+(data.optBoolean("partition1")?"S1 + S2":"S2")+". Zmiana przypisania wymaga rozbrojenia obu stref.";
        if(!data.optBoolean("busy")&&result.equals("success"))text+="\nPrzypisanie potwierdzone w centrali.";
        if(!data.optBoolean("busy")&&(result.equals("error")||result.equals("conflict")||result.equals("not_applied")||result.equals("uncertain")))
            text+="\nNie potwierdzono zmiany. Odczytaj ponownie ustawienia przed kolejną próbą.";
        put(z7Status,text);
    }
    private void confirmZ7(boolean enabled) {
        JSONObject data=current==null?null:current.optJSONObject("input7_arming");renderZ7();
        if(data==null||!data.optBoolean("can_write")||busy)return;
        final long revision=data.optLong("revision",-1);
        String message=enabled?"Z7 będzie załączać czuwanie stref 1 i 2.":"Z7 będzie załączać czuwanie tylko strefy 2.";
        message+="\n\nPrzypisanie zostanie zapisane w centrali i sprawdzone odczytem bloków ustawień Z7.";
        if(enabled&&!data.optBoolean("disarm_partition1"))message+="\n\nZ8 nie jest przypisane do S1. Wyłączenie czuwania S1 może wymagać manipulatora lub pilota.";
        new AlertDialog.Builder(this).setTitle("Zmienić przypisanie Z7?").setMessage(message)
            .setNegativeButton("Anuluj",null).setPositiveButton("Zapisz w centrali",(d,w)->writeZ7(enabled,revision)).show();
    }
    private void writeZ7(boolean enabled,long revision) {
        if(api==null||busy)return;
        final PanelApi selected=api;final int expected=generation;busy=true;enableButtons();includePartition1.setEnabled(false);
        command.setText("Sprawdzanie przypisania Z7 przed zapisem…");
        worker.execute(()->{
            try {
                JSONObject before=validatedState(selected.get("/api/state"));JSONObject data=before.getJSONObject("input7_arming");
                if(!data.optBoolean("can_write")||data.optLong("revision",-2)!=revision)throw new IllegalStateException("Ustawienia lub stan centrali zmieniły się. Odczytaj ponownie dane.");
                long checkedAt=SystemClock.elapsedRealtime();String token=new JSONObject(selected.get("/api/session")).getString("token");
                if(!foreground||expected!=generation||SystemClock.elapsedRealtime()-checkedAt>=4000)throw new IllegalStateException("Przerwano zapis przed wysłaniem polecenia.");
                JSONObject result=new JSONObject(selected.configureZ7(enabled,revision,token));
                ui.post(()->{
                    if(expected!=generation||isFinishing())return;
                    pendingZ7Id=result.optBoolean("busy")?result.optLong("id"):0;busy=pendingZ7Id!=0;
                    command.setText(pendingZ7Id!=0?"Trwa zapis i kontrola przypisania Z7…":result.optString("message","Przypisanie zapisane."));enableButtons();
                });
            }catch(Exception e){String message=errorText(e);ui.post(()->{if(expected!=generation||isFinishing())return;busy=false;command.setText("Zapis Z7: "+message+" Nie ponowiono polecenia.");enableButtons();});}
            if(!monitorEnabled())poll(selected,expected);
        });
    }
    private void sendAction(String action) {
        if(!permitted(current,online,SystemClock.elapsedRealtime()-lastSuccess,busy)||api==null)return;
        busy=true;enableButtons();command.setText("Wysyłanie polecenia…");command.setTextColor(AMBER);
        final PanelApi selected=api;final int expected=generation;
        worker.execute(()->{
            String message;boolean ok=false;
            try {
                JSONObject latest=validatedState(selected.get("/api/state"));
                if(!permitted(latest,true,0,false))throw new IllegalStateException("Brak aktualnego stanu lub centrala jest zajęta.");
                long checkedAt=SystemClock.elapsedRealtime();
                String token=new JSONObject(selected.get("/api/session")).getString("token");
                if(SystemClock.elapsedRealtime()-checkedAt>=4000)throw new IllegalStateException("Stan centrali utracił aktualność przed wysłaniem polecenia.");
                if(!foreground||expected!=generation)throw new IllegalStateException("Aplikacja nie jest już na pierwszym planie.");
                selected.control(action,token);
                message="Polecenie wysłane. Sprawdź rzeczywisty stan stref.";ok=true;
            }catch(Exception e){
                message="Brak potwierdzenia polecenia: "+errorText(e)+" Sprawdź stan stref przed kolejną próbą.";
            }
            final String result=message;final boolean success=ok;
            ui.post(()->{if(expected!=generation||isFinishing())return;busy=false;command.setText(result);command.setTextColor(success?GREEN:AMBER);enableButtons();});
            poll(selected,expected);
        });
    }
    private String errorText(Exception e) {
        if(e instanceof CloudWire.ApiException)return e.getMessage();
        if(e instanceof EspApi.ApiException) {
            EspApi.ApiException error=(EspApi.ApiException)e;
            if(error.status==401)return "Błędny login lub hasło ESP32.";
            try{return new JSONObject(error.body).optString("error","Błąd HTTP "+error.status);}catch(Exception ignored){return "Błąd HTTP "+error.status;}
        }
        if(e instanceof java.net.SocketTimeoutException)return "Przekroczono czas połączenia. Sprawdź Wi-Fi.";
        if(e instanceof java.net.UnknownHostException)return "Nie znaleziono lokalnego adresu ESP32.";
        if(e instanceof java.io.IOException)return "ESP32 nie odpowiada. Sprawdź połączenie z domowym Wi-Fi.";
        if(e instanceof IllegalStateException||e instanceof IllegalArgumentException)return e.getMessage();
        return "Nieprawidłowa odpowiedź ESP32.";
    }

    private EditText field(LinearLayout parent,String title,String value,int id,int inputType) {
        full(label(title,12,MUTED),parent,7);EditText edit=new EditText(this);edit.setId(id);edit.setSingleLine(true);
        edit.setTextColor(TEXT);edit.setTextSize(15);edit.setInputType(inputType);edit.setText(value);full(edit,parent,0);return edit;
    }
    private void showConnection() {
        if(connectionDialog!=null&&connectionDialog.isShowing())return;
        LinearLayout form=column();form.setPadding(dp(20),dp(8),dp(20),dp(8));
        full(label("Tryb połączenia",12,MUTED),form,3);
        Spinner mode=new Spinner(this);mode.setId(R.id.connection_mode);
        mode.setAdapter(new ArrayAdapter<String>(this,android.R.layout.simple_spinner_dropdown_item,new String[]{"Automatycznie: LAN → SUPLA","Tylko LAN","Tylko SUPLA — internet"}));
        String oldMode=store.preferences.getString("connection_mode","lan");mode.setSelection(oldMode.equals("auto")?0:oldMode.equals("supla")?2:1);full(mode,form,0);
        EditText server=field(form,"Adres ESP32",store.endpoint(),R.id.server,InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_URI);
        EditText user=field(form,"Login ESP32",store.username(),R.id.username,InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD);
        EditText password=field(form,"Hasło ESP32", "",R.id.password,InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_PASSWORD);
        password.setHint(store.configured()?"Puste pole zachowa zapisane hasło":"Hasło strony ESP32");password.setHintTextColor(MUTED);
        EditText cloud=field(form,"SUPLA Cloud — adres HTTPS",store.preferences.getString("supla_endpoint",""),R.id.supla_server,InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_URI);
        EditText device=field(form,"Numer urządzenia SUPLA",store.preferences.getInt("supla_device",0)>0?String.valueOf(store.preferences.getInt("supla_device",0)):"",R.id.supla_device,InputType.TYPE_CLASS_NUMBER);
        EditText cloudToken=field(form,"Token SUPLA", "",R.id.supla_token,InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_PASSWORD);
        cloudToken.setHint(store.cloudConfigured()?"Puste pole zachowa zapisany token":"Odczyt kanałów i wykonywanie akcji");cloudToken.setHintTextColor(MUTED);
        full(label("Hasło ESP32 i token SUPLA są szyfrowane w telefonie. SUPLA umożliwia dostęp przez internet. Edycja ustawień centrali pozostaje dostępna przez LAN.",11,MUTED),form,8);
        ScrollView scroll=new ScrollView(this);scroll.addView(form);
        AlertDialog dialog=new AlertDialog.Builder(this).setTitle("Połączenie LAN / SUPLA").setView(scroll).setNegativeButton("Anuluj",null).setPositiveButton("Zapisz i połącz",null).create();
        connectionDialog=dialog;
        dialog.setOnShowListener(d->{
            dialog.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
            Button save=dialog.getButton(AlertDialog.BUTTON_POSITIVE);save.setId(R.id.save_connection);
            save.setOnClickListener(v->{
                if(busy){server.setError("Poczekaj na zakończenie polecenia");return;}
                try {
                    LanEndpoint endpoint=LanEndpoint.parse(server.getText().toString());String username=user.getText().toString().trim();
                    String secret=password.getText().toString();if(secret.isEmpty()&&store.configured())secret=store.password();
                    if(username.isEmpty()||username.contains(":")){user.setError("Podaj poprawny login");return;}
                    if(secret.isEmpty()){password.setError("Podaj hasło ESP32");return;}
                    String selectedMode=mode.getSelectedItemPosition()==0?"auto":mode.getSelectedItemPosition()==2?"supla":"lan";
                    String cloudText=cloud.getText().toString().trim(),deviceText=device.getText().toString().trim();
                    String cloudBase=cloudText.isEmpty()&&selectedMode.equals("lan")?"":CloudEndpoint.parse(cloudText).base,newToken=cloudToken.getText().toString().trim();
                    int deviceNumber=deviceText.isEmpty()&&selectedMode.equals("lan")?0:Integer.parseInt(deviceText);if(!selectedMode.equals("lan")&&deviceNumber<=0)throw new IllegalArgumentException("Podaj numer urządzenia SUPLA.");
                    if(!selectedMode.equals("lan")&&newToken.isEmpty()&&!store.cloudConfigured()){cloudToken.setError("Podaj token SUPLA");return;}
                    if(!newToken.isEmpty())new CloudWire(CloudEndpoint.parse(cloudBase),newToken);
                    store.save(endpoint.base,username,secret);store.saveCloud(cloudBase,newToken,deviceNumber,selectedMode);generation++;api=new PanelApi(store);
                    current=null;online=false;connectionFailure="";command.setText("Połączenie zapisane. Oczekiwanie na dane centrali…");
                    dialog.dismiss();render();ensureMonitor(true);startPolling();
                }catch(Exception error){server.setError(error instanceof IllegalArgumentException?error.getMessage():"Nie zapisano danych połączenia.");}
            });
        });dialog.show();
    }

    private boolean monitorEnabled(){return store.preferences.getBoolean("monitor_enabled",true);}
    private void ensureMonitor(boolean reload) {
        if(api==null||!monitorEnabled())return;
        try {
            Intent intent=new Intent(this,MonitorService.class);if(reload)intent.setAction(MonitorService.RELOAD);
            if(!MonitorService.running||reload)startForegroundService(intent);
        }catch(RuntimeException e){MonitorService.serviceProblem="Android zatrzymał usługę. Otwórz ustawienia pracy w tle.";}
    }
    private void refreshMonitor() {
        if(monitorStatus==null)return;
        AudioManager audio=getSystemService(AudioManager.class);PowerManager power=getSystemService(PowerManager.class);
        boolean exempt=power.isIgnoringBatteryOptimizations(getPackageName());
        boolean permitted=Build.VERSION.SDK_INT<33||checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)==PackageManager.PERMISSION_GRANTED;
        String text=!monitorEnabled()?"Monitorowanie w tle wyłączone":MonitorService.running?"Usługa monitorowania działa także po zamknięciu ekranu aplikacji":"Uruchamianie monitorowania…";
        if(!exempt)text+="\nZezwól na pracę bez optymalizacji baterii — Alarm telefonu.";
        if(!permitted)text+="\nZezwól aplikacji na powiadomienia.";
        if(audio.getStreamVolume(AudioManager.STREAM_ALARM)==0)text+="\nGłośność alarmów wynosi 0 — zwiększ ją.";
        if(MonitorService.latchedMask!=0)text="ALARM: "+MonitorService.alarmText+"\nSygnał trwa do ręcznego wyciszenia telefonu.";
        if(!MonitorService.serviceProblem.isEmpty())text+="\n"+MonitorService.serviceProblem;
        put(monitorStatus,text);monitorStatus.setTextColor(MonitorService.latchedMask!=0?RED:!exempt||!permitted?AMBER:MUTED);
        silence.setVisibility(MonitorService.latchedMask!=0||MonitorService.sounding||MonitorService.testing?View.VISIBLE:View.GONE);
    }
    private void showSoundSettings() {
        LinearLayout content=column();content.setPadding(dp(20),dp(8),dp(20),dp(10));
        AudioManager audio=getSystemService(AudioManager.class);
        TextView volume=label("Głośność alarmów: "+audio.getStreamVolume(AudioManager.STREAM_ALARM)+" / "+audio.getStreamMaxVolume(AudioManager.STREAM_ALARM),14,TEXT);content.addView(volume);
        SeekBar slider=new SeekBar(this);slider.setMax(audio.getStreamMaxVolume(AudioManager.STREAM_ALARM));slider.setProgress(audio.getStreamVolume(AudioManager.STREAM_ALARM));
        slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onStartTrackingTouch(SeekBar s){}public void onStopTrackingTouch(SeekBar s){}
            public void onProgressChanged(SeekBar s,int value,boolean fromUser){if(fromUser)audio.setStreamVolume(AudioManager.STREAM_ALARM,value,0);volume.setText("Głośność alarmów: "+value+" / "+slider.getMax());refreshMonitor();}});
        full(slider,content,8);
        full(label("Sygnał jest zapętlony i używa kanału alarmów telefonu. Tryb Nie przeszkadzać musi zezwalać na alarmy. Do wykrycia alarmu potrzebne jest połączenie z domową siecią Wi-Fi.",12,MUTED),content,8);
        Button notify=button("Zezwól na powiadomienia",Color.rgb(42,70,94),R.id.notification_permission);
        notify.setOnClickListener(v->{
            if(Build.VERSION.SDK_INT>=33&&checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED)
                requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS},51);
            else startActivity(new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE,getPackageName()));
        });full(notify,content,8);
        Button battery=button("Zezwól na stałą pracę w tle",Color.rgb(42,70,94),R.id.battery_optimization);
        battery.setOnClickListener(v->{
            PowerManager power=getSystemService(PowerManager.class);
            if(!power.isIgnoringBatteryOptimizations(getPackageName()))startActivity(new Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS,Uri.parse("package:"+getPackageName())));
            else android.widget.Toast.makeText(this,"Praca bez optymalizacji baterii jest już dozwolona.",android.widget.Toast.LENGTH_SHORT).show();
        });full(battery,content,7);
        new AlertDialog.Builder(this).setTitle("Alarm telefonu").setView(content).setPositiveButton("Zamknij",null).show();
    }

    private void showNotifications() {
        final PanelApi selected=api;if(selected==null)return;
        if(notificationDialog!=null&&notificationDialog.isShowing())return;
        final String username=store.username(),password;
        try{password=store.password();}catch(Exception e){connectionFailure="Zapisz ponownie hasło ESP32.";render();return;}
        LinearLayout container=column();container.setPadding(dp(5),dp(5),dp(5),dp(5));
        TextView status=label("Ustawienia PUSH są zapisywane w ESP32. Powiadomienia dostarcza SUPLA.",11,MUTED);container.addView(status);
        WebView view=new WebView(this);notificationWeb=view;
        view.setBackgroundColor(BG);view.getSettings().setJavaScriptEnabled(true);
        view.getSettings().setAllowFileAccess(false);view.getSettings().setAllowContentAccess(false);
        view.getSettings().setSupportMultipleWindows(false);view.getSettings().setDomStorageEnabled(false);
        view.getSettings().setMixedContentMode(android.webkit.WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        container.addView(view,new LinearLayout.LayoutParams(-1,0,1));
        AlertDialog dialog=new AlertDialog.Builder(this).setTitle("Powiadomienia PUSH").setView(container).setPositiveButton("Zamknij",null).create();
        notificationDialog=dialog;
        view.setWebViewClient(new WebViewClient(){
            @Override public WebResourceResponse shouldInterceptRequest(WebView v,WebResourceRequest request) {
                String url=request.getUrl().toString();
                if(!url.startsWith(selected.endpoint.base+"/"))
                    return new WebResourceResponse("text/plain","UTF-8",403,"Local access only",java.util.Collections.emptyMap(),new java.io.ByteArrayInputStream(new byte[0]));
                return null;
            }
            @Override public void onReceivedHttpAuthRequest(WebView v,HttpAuthHandler handler,String host,String realm) {
                if(selected.endpoint.host.equalsIgnoreCase(host))handler.proceed(username,password);else handler.cancel();
            }
            @Override public boolean shouldOverrideUrlLoading(WebView v,WebResourceRequest request) {
                if(!request.isForMainFrame())return false;
                String url=request.getUrl().toString();
                if(url.equals(selected.endpoint.base+"/")){dialog.dismiss();return true;}
                return !(url.equals(selected.endpoint.base+"/notifications")||url.equals(selected.endpoint.base+"/notifications/"));
            }
        });
        dialog.setOnDismissListener(d->{view.stopLoading();view.destroy();notificationWeb=null;notificationDialog=null;});
        dialog.show();dialog.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT,ViewGroup.LayoutParams.MATCH_PARENT);
        view.loadUrl(selected.endpoint.base+"/notifications");
    }
}
