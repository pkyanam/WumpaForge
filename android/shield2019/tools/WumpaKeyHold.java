import android.os.SystemClock;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import java.lang.reflect.Method;
import java.util.concurrent.atomic.AtomicBoolean;

/** Runs as ADB shell via app_process; injects an actual held remote key. */
public final class WumpaKeyHold {
    static int keyCode(String name) {
        switch(name.toUpperCase(java.util.Locale.ROOT)) {
        case "UP": return KeyEvent.KEYCODE_DPAD_UP;
        case "DOWN": return KeyEvent.KEYCODE_DPAD_DOWN;
        case "LEFT": return KeyEvent.KEYCODE_DPAD_LEFT;
        case "RIGHT": return KeyEvent.KEYCODE_DPAD_RIGHT;
        case "CENTER": return KeyEvent.KEYCODE_DPAD_CENTER;
        case "BACK": return KeyEvent.KEYCODE_BACK;
        case "PLAY_PAUSE": return KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE;
        case "REWIND": return KeyEvent.KEYCODE_MEDIA_REWIND;
        case "FAST_FORWARD": return KeyEvent.KEYCODE_MEDIA_FAST_FORWARD;
        case "MENU": return KeyEvent.KEYCODE_MENU;
        default: throw new IllegalArgumentException("Unsupported remote key: "+name);
        }
    }
    static int keyboardCode(String name) {
        switch(name.toUpperCase(java.util.Locale.ROOT)) {
        case "W": return KeyEvent.KEYCODE_W;
        case "A": return KeyEvent.KEYCODE_A;
        case "S": return KeyEvent.KEYCODE_S;
        case "D": return KeyEvent.KEYCODE_D;
        case "I": return KeyEvent.KEYCODE_I;
        case "J": return KeyEvent.KEYCODE_J;
        case "K": return KeyEvent.KEYCODE_K;
        case "L": return KeyEvent.KEYCODE_L;
        case "SPACE": return KeyEvent.KEYCODE_SPACE;
        case "C": return KeyEvent.KEYCODE_C;
        case "X": return KeyEvent.KEYCODE_X;
        case "E": return KeyEvent.KEYCODE_E;
        case "ENTER": return KeyEvent.KEYCODE_ENTER;
        case "BACKSPACE": return KeyEvent.KEYCODE_DEL;
        case "Q": return KeyEvent.KEYCODE_Q;
        case "R": return KeyEvent.KEYCODE_R;
        case "SHIFT": return KeyEvent.KEYCODE_SHIFT_LEFT;
        case "CTRL": return KeyEvent.KEYCODE_CTRL_LEFT;
        default: throw new IllegalArgumentException("Unsupported diagnostic keyboard key: "+name);
        }
    }
    static long duration(String value) {
        long ms=Long.parseLong(value);
        if(ms<1||ms>10000)throw new IllegalArgumentException("Hold must be 1..10000 milliseconds");
        return ms;
    }
    interface Injection { void send(boolean down) throws Exception; }
    interface Delay { void sleep(long ms) throws Exception; }
    static void hold(Injection injection,Delay delay,long ms) throws Exception {
        try { injection.send(true);delay.sleep(ms); }
        finally { injection.send(false); }
    }
    public static void main(String[] args) throws Exception {
        if(args.length<2||args.length>3||(args.length==3&&!args[2].equals("keyboard")))
            throw new IllegalArgumentException("Usage: WumpaKeyHold remote-key milliseconds(1..10000), or WumpaKeyHold W milliseconds keyboard");
        final boolean keyboard=args.length==3;
        final int key=keyboard?keyboardCode(args[0]):keyCode(args[0]);final long ms=duration(args[1]);
        final int source=keyboard?InputDevice.SOURCE_KEYBOARD:InputDevice.SOURCE_DPAD;
        System.out.println("Injecting key="+args[0]+" code="+key+" source=0x"+Integer.toHexString(source)+" device="+KeyCharacterMap.VIRTUAL_KEYBOARD+" flags=0 hold_ms="+ms);
        Class<?> managerClass=Class.forName("android.hardware.input.InputManager");
        Method getInstance=managerClass.getDeclaredMethod("getInstance");getInstance.setAccessible(true);
        final Object manager=getInstance.invoke(null);
        final Method inject=managerClass.getDeclaredMethod("injectInputEvent",InputEvent.class,int.class);inject.setAccessible(true);
        final long downTime=SystemClock.uptimeMillis();final AtomicBoolean released=new AtomicBoolean();
        final Injection injection=new Injection() { public synchronized void send(boolean down) throws Exception {
            if(!down&&released.get())return;
            KeyEvent event=new KeyEvent(downTime,SystemClock.uptimeMillis(),down?KeyEvent.ACTION_DOWN:KeyEvent.ACTION_UP,
                    key,0,0,KeyCharacterMap.VIRTUAL_KEYBOARD,0,0,source);
            // WAIT_FOR_FINISH confirms Android dispatch; game simulation may consume it later.
            if(!Boolean.TRUE.equals(inject.invoke(manager,event,2)))throw new IllegalStateException("Android rejected key "+(down?"DOWN":"UP"));
            System.out.println((down?"DOWN":"UP")+" code="+key+" source=0x"+Integer.toHexString(source)+" event_ms="+event.getEventTime()+" dispatched_ms="+SystemClock.uptimeMillis());
            if(!down)released.set(true);
        }};
        Thread cleanup=new Thread(() -> {try{injection.send(false);}catch(Exception e){System.err.println("Emergency key release failed: "+e);}},"Wumpa-key-release");
        Runtime.getRuntime().addShutdownHook(cleanup);
        try {hold(injection,Thread::sleep,ms);}
        finally {if(released.get())Runtime.getRuntime().removeShutdownHook(cleanup);}
        System.out.println("Released "+args[0]+" after requested "+ms+"ms hold");
    }
}
