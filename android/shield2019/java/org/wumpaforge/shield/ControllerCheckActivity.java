package org.wumpaforge.shield;
import android.os.Bundle;
import android.graphics.Color;
import android.widget.TextView;
import android.widget.RelativeLayout;
import org.libsdl.app.SDLActivity;
public final class ControllerCheckActivity extends SDLActivity {
    private TextView status;
    @Override protected String[] getLibraries(){return new String[]{"SDL2","controller_check"};}
    @Override public void onCreate(Bundle state){
        super.onCreate(state);
        status=new TextView(this);status.setTextColor(Color.WHITE);status.setTextSize(18);status.setPadding(32,20,32,12);status.setBackgroundColor(0xD0101820);
        status.setText("Starting controller diagnostic…");
        if(mLayout!=null)mLayout.addView(status,new RelativeLayout.LayoutParams(-1,-2));
    }
    public void showStatus(String text){runOnUiThread(()->{if(status!=null)status.setText(text);});}
}
