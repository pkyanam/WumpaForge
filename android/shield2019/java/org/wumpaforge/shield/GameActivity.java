package org.wumpaforge.shield;
import org.libsdl.app.SDLActivity;
/** SDL supplies Android controller/JNI integration; original game runs in libmain. */
public final class GameActivity extends SDLActivity {
    @Override protected String[] getLibraries() { return new String[]{"SDL2", "main"}; }
}
