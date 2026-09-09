# Held remote key injection for ADB debugging

Android 30's `input keyevent --longpress` does not necessarily sustain a key for
long enough for a slow game frame to consume it. This small shell tool sends a
real key-down, waits the requested milliseconds, and sends key-up in `finally`.
It uses Android's `InputManager` as the ADB shell user, without changing game input
mapping, device settings, or permissions. Android must allow shell injection.

Build locally (uses the installed SDK and Java, never connects automatically):

```sh
python3 android/shield2019/tools/build_keyhold.py
```

With the game already focused, push and run against the explicit authorized TV:

```sh
adb -s 192.168.1.46:5555 push build/shield2019/keyhold/wumpa-keyhold.jar /data/local/tmp/wumpa-keyhold.jar
adb -s 192.168.1.46:5555 shell 'CLASSPATH=/data/local/tmp/wumpa-keyhold.jar app_process /system/bin WumpaKeyHold UP 1500'
```

Accepted keys: `UP`, `DOWN`, `LEFT`, `RIGHT`, `CENTER`, `BACK`, `PLAY_PAUSE`,
`REWIND`, `FAST_FORWARD`, `MENU`. Hold duration is **1–10000ms**. Injection uses
`SOURCE_DPAD` and the virtual keyboard device, so the game's existing remote
layers apply. Home, voice, and volume are deliberately outside this debug tool.

The tool waits for Android dispatch before starting the hold interval. A finally
block releases the key after normal completion, interruption, or injection error;
a shutdown hook also attempts release during ordinary process termination.
SIGKILL, device disconnection or Android rejecting injection can prevent cleanup.
Changing game focus clears the game's remote state if recovery is needed.
Host tests cover event ordering, release on interruption/down failure, input-key
allowlisting and duration limits. They do not validate Android reflection or TV
injection; that requires the authorized physical device.

For an A/B test against the remote mapping, use keyboard source explicitly:

```sh
adb -s 192.168.1.46:5555 shell 'CLASSPATH=/data/local/tmp/wumpa-keyhold.jar app_process /system/bin WumpaKeyHold W 1500 keyboard'
```

Keyboard mode allows `W A S D I J K L SPACE C X E ENTER BACKSPACE Q R SHIFT CTRL`.
Its events use `SOURCE_KEYBOARD`; normal remote mode retains `SOURCE_DPAD`.
Both use flags zero, matching stock shell key injection. Output reports the key,
source, device, flags, down/up event times and dispatch-completion times so an
input trace can distinguish Android dispatch from game-frame consumption.
