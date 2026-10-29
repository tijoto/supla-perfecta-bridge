package org.suplaforum.perfectalan;

import org.json.JSONArray;
import org.json.JSONObject;

/** Maps only explicitly published channel states; absent values stay unknown. */
public final class SuplaState {
    private SuplaState(){}
    static boolean connected(JSONObject channel){
        JSONObject state=channel.optJSONObject("state");return channel.optBoolean("connected")&&state!=null&&state.optBoolean("connected")&&"CONNECTED".equals(state.optString("connectedCode"));
    }
    public static JSONObject decode(String body,int device,String endpoint)throws Exception{
        JSONArray rows=new JSONArray(body);if(rows.length()>2048)throw new IllegalArgumentException("Zbyt wiele kanałów SUPLA");
        JSONObject[] channels=new JSONObject[13];
        for(int i=0;i<rows.length();i++){
            JSONObject row=rows.getJSONObject(i);if(row.optInt("iodeviceId",-1)!=device)continue;
            int n=row.optInt("channelNumber",-1);if(n<0||n>=13)continue;
            if(channels[n]!=null)throw new IllegalArgumentException("Powielony numer kanału SUPLA");channels[n]=row;
        }
        for(int n=0;n<13;n++)if(channels[n]==null)throw new IllegalArgumentException("Brak kanału Perfecty w SUPLA: "+n);
        if(channels[12].getJSONObject("function").optInt("id")!=130)throw new IllegalArgumentException("Kanał sterowania SUPLA ma inną funkcję");
        JSONArray zones=new JSONArray(),inputs=new JSONArray();boolean fresh=true;
        for(int n=0;n<4;n++){
            JSONObject state=channels[n].optJSONObject("state");
            fresh&=connected(channels[n])&&state!=null&&state.opt("hi") instanceof Boolean;
            if(channels[n].getJSONObject("function").optString("name").equals("UNSUPPORTED"))fresh=false;
        }
        for(int n=0;n<2;n++){
            JSONObject arming=channels[n].optJSONObject("state"),alarm=channels[n+2].optJSONObject("state");
            boolean armKnown=connected(channels[n])&&arming!=null&&arming.opt("hi") instanceof Boolean;
            boolean alarmKnown=connected(channels[n+2])&&alarm!=null&&alarm.opt("hi") instanceof Boolean;
            zones.put(new JSONObject().put("number",n+1).put("name","Strefa "+(n+1)).put("state",armKnown?(arming.getBoolean("hi")?5:0):-1)
                .put("alarm",alarmKnown?(alarm.getBoolean("hi")?68:0):-1).put("source",0));
        }
        for(int n=1;n<=16;n++){
            JSONObject row=n>=9?channels[n-5]:null,state=row==null?null:row.optJSONObject("state");
            String name=row==null?"Wejście "+n:row.optString("caption","Wejście "+n).replaceFirst("^\\d+\\s*:\\s*","");
            boolean available=row!=null&&connected(row)&&state!=null&&state.opt("hi") instanceof Boolean;
            inputs.put(new JSONObject().put("number",n).put("name",name).put("configured",true).put("configured_known",false)
                .put("status_available",available).put("violated",available&&state.getBoolean("hi")).put("wireless",n>=9)
                .put("blocked",false).put("history_available",false).put("partition_settings_ready",false).put("can_assign",false));
        }
        boolean control=connected(channels[12])&&channels[12].getJSONObject("state").opt("on") instanceof Boolean;
        return new JSONObject().put("version","SUPLA Cloud").put("transport","supla").put("transport_endpoint",endpoint).put("status_fresh",fresh)
            .put("diagnostics_fresh",false).put("rf_fresh",false).put("names_ready",true).put("partitions",zones).put("inputs",inputs)
            .put("bridge",new JSONObject().put("connected",!control)).put("control",new JSONObject().put("active_gpio",0).put("cooldown_ms",0))
            .put("remote_control_channel",channels[12].getInt("id")).put("remote_control_ready",control)
            .put("remote_note",fresh?"":"SUPLA nie udostępnia aktualnego stanu wszystkich stref i alarmów.");
    }
}
