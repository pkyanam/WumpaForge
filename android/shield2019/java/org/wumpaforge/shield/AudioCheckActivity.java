package org.wumpaforge.shield;
import android.os.Bundle;
import android.graphics.Color;
import android.widget.TextView;
import android.widget.RelativeLayout;
import org.libsdl.app.SDLActivity;
public final class AudioCheckActivity extends SDLActivity {
    @Override protected String[] getLibraries(){return new String[]{"SDL2","audio_check"};}
    @Override public void onCreate(Bundle state){
        super.onCreate(state);
        TextView help=new TextView(this);help.setTextColor(Color.WHITE);help.setTextSize(22);help.setPadding(40,40,40,40);
        help.setText("Stereo audio check\nPlaying a quiet left tone, then a right tone.\nA callback report will follow.");
        if(mLayout!=null)mLayout.addView(help,new RelativeLayout.LayoutParams(-1,-2));
    }
}
