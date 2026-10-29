package org.suplaforum.perfectalan;

/** Only a fresh, explicitly decoded alarm 68 can trigger. Missing data never invents an alarm or its end. */
public final class AlarmLatch {
    public int observedMask, latchedMask;
    public AlarmLatch(int observedMask,int latchedMask) {this.observedMask=observedMask&3;this.latchedMask=latchedMask&3;}
    public int accept(boolean fresh,int[] alarms) {
        if(!fresh||alarms==null||alarms.length!=2)return 0;
        int present=observedMask;
        for(int i=0;i<2;i++) {
            if(alarms[i]==68)present|=1<<i;
            else if(alarms[i]==0)present&=~(1<<i);
        }
        int newlyTriggered=present&~observedMask;
        observedMask=present;latchedMask|=newlyTriggered;return newlyTriggered;
    }
    public void acknowledge(){latchedMask=0;}
    public boolean active(){return latchedMask!=0;}
}
