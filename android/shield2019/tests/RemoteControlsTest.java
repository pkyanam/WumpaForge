package org.wumpaforge.shield;
import android.view.KeyEvent;
import java.util.HashSet;
import java.util.Set;

public final class RemoteControlsTest {
    public static void main(String[] args) {
        final Set<Integer> active = new HashSet<>();
        final int[] presses = {0};
        final int[] layerChanges = {0};
        RemoteControls r = new RemoteControls(new RemoteControls.Sink() {
            public void key(int k, boolean down) {
                if (down) { if (!active.add(k)) throw new AssertionError("duplicate down"); presses[0]++; }
                else if (!active.remove(k)) throw new AssertionError("unpaired up");
            }
            public void layer(String s) { layerChanges[0]++; }
        });
        r.event(KeyEvent.KEYCODE_DPAD_UP, true);
        assert active.contains(KeyEvent.KEYCODE_W) && active.contains(KeyEvent.KEYCODE_DPAD_UP);
        int count = presses[0];
        r.event(KeyEvent.KEYCODE_DPAD_UP, true);
        assert presses[0] == count;
        r.event(KeyEvent.KEYCODE_MENU, true); // Changing layer releases old movement.
        assert active.isEmpty();
        r.event(KeyEvent.KEYCODE_MENU, true); // Menu auto-repeat must not cycle again.
        r.event(KeyEvent.KEYCODE_MENU, false);
        r.event(KeyEvent.KEYCODE_DPAD_UP, false);
        r.event(KeyEvent.KEYCODE_DPAD_UP, true);
        assert active.contains(KeyEvent.KEYCODE_I);
        r.releaseAll(); assert active.isEmpty();
        r.event(KeyEvent.KEYCODE_MENU, true); r.event(KeyEvent.KEYCODE_MENU, false);
        r.event(KeyEvent.KEYCODE_DPAD_LEFT, true);
        assert active.contains(KeyEvent.KEYCODE_SHIFT_LEFT);
        r.releaseAll();
        r.event(KeyEvent.KEYCODE_MENU, true); r.event(KeyEvent.KEYCODE_MENU, false);
        r.event(KeyEvent.KEYCODE_DPAD_CENTER, true); r.event(KeyEvent.KEYCODE_BUTTON_A, true);
        r.event(KeyEvent.KEYCODE_DPAD_CENTER, false);
        assert active.contains(KeyEvent.KEYCODE_SPACE); // Shared logical key reference count.
        r.event(KeyEvent.KEYCODE_BUTTON_A, false); assert active.isEmpty();

        // Lifecycle callbacks clear state before Android's delayed key-up can
        // arrive.  The late up must be harmless, and a subsequent Menu press
        // must still cycle exactly once.
        r.event(KeyEvent.KEYCODE_DPAD_RIGHT, true);
        r.event(KeyEvent.KEYCODE_MENU, true);
        assert active.isEmpty();
        r.event(KeyEvent.KEYCODE_DPAD_RIGHT, true);
        assert !active.isEmpty();
        r.releaseAll(); // onPause/onFocusLost/device removal may repeat this.
        assert active.isEmpty();
        r.releaseAll(); // Repeated lifecycle callbacks must not emit more ups.
        r.event(KeyEvent.KEYCODE_DPAD_RIGHT, false);
        r.event(KeyEvent.KEYCODE_MENU, false);
        int beforeMenu = layerChanges[0];
        r.event(KeyEvent.KEYCODE_MENU, true);
        assert active.isEmpty(); // Menu itself is consumed while changing layer.
        assert layerChanges[0] == beforeMenu + 1;
        r.event(KeyEvent.KEYCODE_MENU, true); // Repeat must not cycle again.
        assert layerChanges[0] == beforeMenu + 1;
        r.event(KeyEvent.KEYCODE_MENU, false);

        assert !r.event(KeyEvent.KEYCODE_HOME, true);
        assert !r.event(KeyEvent.KEYCODE_VOLUME_UP, true);
        assert !r.event(KeyEvent.KEYCODE_SEARCH, true);
        System.out.println("Remote state checks passed: repeat, layers, overlap, lifecycle clears, delayed releases, system keys");
    }
}
