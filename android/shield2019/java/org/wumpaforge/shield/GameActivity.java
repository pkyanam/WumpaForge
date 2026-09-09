package org.wumpaforge.shield;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.widget.Toast;
import org.libsdl.app.SDLActivity;

/** SDL supplies real gamepads; limited TV remotes use a separate keyboard bridge. */
public final class GameActivity extends SDLActivity implements android.hardware.input.InputManager.InputDeviceListener {
    private android.hardware.input.InputManager inputs;
    private final RemoteControls remote = new RemoteControls(new RemoteControls.Sink() {
        public void key(int code, boolean down) {
            if (down) SDLActivity.onNativeKeyDown(code); else SDLActivity.onNativeKeyUp(code);
        }
        public void layer(String name) {
            Toast.makeText(GameActivity.this, "Remote: " + name, Toast.LENGTH_SHORT).show();
        }
    });
    @Override protected void onCreate(android.os.Bundle state) {
        super.onCreate(state);
        inputs = (android.hardware.input.InputManager)getSystemService(INPUT_SERVICE);
        inputs.registerInputDeviceListener(this, null);
    }
    public void onInputDeviceAdded(int id) { }
    public void onInputDeviceChanged(int id) { remote.releaseAll(); }
    public void onInputDeviceRemoved(int id) { remote.releaseAll(); }
    @Override protected String[] getLibraries() { return new String[]{"SDL2", "main"}; }
    private boolean isRemote(KeyEvent event) {
        InputDevice device = event.getDevice();
        if (device == null) return true; // Explicit ADB key injection for diagnostics.
        if (device.getKeyboardType() == InputDevice.KEYBOARD_TYPE_ALPHABETIC) return false;
        if (device.getMotionRange(android.view.MotionEvent.AXIS_X) != null &&
            device.getMotionRange(android.view.MotionEvent.AXIS_Y) != null) return false;
        boolean[] buttons = device.hasKeys(KeyEvent.KEYCODE_BUTTON_A,
                KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BUTTON_START);
        if (buttons[0] && buttons[1] && buttons[2]) return false;
        return device.supportsSource(InputDevice.SOURCE_DPAD) ||
               device.supportsSource(InputDevice.SOURCE_GAMEPAD) ||
               device.supportsSource(InputDevice.SOURCE_KEYBOARD);
    }
    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        if ((event.getAction() == KeyEvent.ACTION_DOWN || event.getAction() == KeyEvent.ACTION_UP)
                && isRemote(event) && remote.event(event.getKeyCode(), event.getAction() == KeyEvent.ACTION_DOWN))
            return true;
        return super.dispatchKeyEvent(event); // Home, volume and voice keep system behavior.
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        if (!focused) remote.releaseAll();
        super.onWindowFocusChanged(focused);
    }
    @Override protected void onPause() { remote.releaseAll(); super.onPause(); }
    @Override protected void onDestroy() {
        remote.releaseAll();
        if (inputs != null) inputs.unregisterInputDeviceListener(this);
        super.onDestroy();
    }
}
