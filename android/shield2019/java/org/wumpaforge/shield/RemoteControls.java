package org.wumpaforge.shield;

import android.view.KeyEvent;
import java.util.HashMap;
import java.util.Map;

/** Remote-to-keyboard state machine. Native SDL keyboard input remains focus scoped. */
final class RemoteControls {
    interface Sink { void key(int code, boolean down); void layer(String name); }
    private final Sink sink;
    private final Map<Integer, int[]> held = new HashMap<>();
    private final Map<Integer, Integer> refs = new HashMap<>();
    private int layer;
    RemoteControls(Sink sink) { this.sink = sink; }

    boolean event(int code, boolean down) {
        if (code == KeyEvent.KEYCODE_MENU) {
            if (down && !held.containsKey(code)) {
                releaseAll();
                layer = (layer + 1) % 3;
                held.put(code, new int[0]);
                sink.layer(new String[]{"Movement", "Camera", "Extra buttons"}[layer]);
            } else if (!down) held.remove(code);
            return true;
        }
        int[] keys = mapping(code);
        if (keys == null) return false;
        if (down) {
            if (held.containsKey(code)) return true; // Android repeats must not re-press.
            held.put(code, keys);
            for (int key : keys) {
                int count = refs.containsKey(key) ? refs.get(key) : 0;
                refs.put(key, count + 1);
                if (count == 0) sink.key(key, true);
            }
        } else {
            keys = held.remove(code);
            if (keys != null) for (int key : keys) {
                int count = refs.get(key) - 1;
                if (count == 0) { refs.remove(key); sink.key(key, false); }
                else refs.put(key, count);
            }
        }
        return true;
    }
    void releaseAll() {
        for (int key : refs.keySet()) sink.key(key, false);
        refs.clear(); held.clear();
    }
    private int[] mapping(int code) {
        switch (code) {
        case KeyEvent.KEYCODE_DPAD_UP:
            return layer == 0 ? new int[]{KeyEvent.KEYCODE_W, KeyEvent.KEYCODE_DPAD_UP} :
                   new int[]{layer == 1 ? KeyEvent.KEYCODE_I : KeyEvent.KEYCODE_Q};
        case KeyEvent.KEYCODE_DPAD_DOWN:
            return layer == 0 ? new int[]{KeyEvent.KEYCODE_S, KeyEvent.KEYCODE_DPAD_DOWN} :
                   new int[]{layer == 1 ? KeyEvent.KEYCODE_K : KeyEvent.KEYCODE_R};
        case KeyEvent.KEYCODE_DPAD_LEFT:
            return layer == 0 ? new int[]{KeyEvent.KEYCODE_A, KeyEvent.KEYCODE_DPAD_LEFT} :
                   new int[]{layer == 1 ? KeyEvent.KEYCODE_J : KeyEvent.KEYCODE_SHIFT_LEFT};
        case KeyEvent.KEYCODE_DPAD_RIGHT:
            return layer == 0 ? new int[]{KeyEvent.KEYCODE_D, KeyEvent.KEYCODE_DPAD_RIGHT} :
                   new int[]{layer == 1 ? KeyEvent.KEYCODE_L : KeyEvent.KEYCODE_CTRL_LEFT};
        case KeyEvent.KEYCODE_DPAD_CENTER: case KeyEvent.KEYCODE_BUTTON_A:
            return new int[]{layer == 2 ? KeyEvent.KEYCODE_DEL : KeyEvent.KEYCODE_SPACE};
        case KeyEvent.KEYCODE_BACK: case KeyEvent.KEYCODE_BUTTON_B:
            return new int[]{layer == 2 ? KeyEvent.KEYCODE_Z : KeyEvent.KEYCODE_C};
        case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE: case KeyEvent.KEYCODE_MEDIA_PLAY:
        case KeyEvent.KEYCODE_MEDIA_PAUSE:
            return new int[]{layer == 2 ? KeyEvent.KEYCODE_V : KeyEvent.KEYCODE_ENTER};
        case KeyEvent.KEYCODE_MEDIA_REWIND: return new int[]{KeyEvent.KEYCODE_X};
        case KeyEvent.KEYCODE_MEDIA_FAST_FORWARD: return new int[]{KeyEvent.KEYCODE_E};
        default: return null;
        }
    }
}
