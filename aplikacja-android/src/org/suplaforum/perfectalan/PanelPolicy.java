package org.suplaforum.perfectalan;

public final class PanelPolicy {
    private PanelPolicy() {}
    public static boolean mayControl(boolean online, long ageMs, boolean fresh, int[] states,
                                     boolean bridge, int activeGpio, long cooldownMs, boolean busy) {
        if (!online || ageMs < 0 || ageMs >= 4000 || !fresh || states == null || states.length != 2 ||
            bridge || activeGpio != 0 || cooldownMs > 0 || busy) return false;
        for (int state : states) if (state != 0 && state != 1 && state != 5) return false;
        return true;
    }
}
