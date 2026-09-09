package org.wumpaforge.shield;
import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.util.Arrays;

public final class LauncherActivity extends Activity {
    private TextView report;
    private boolean busy;
    private static native String nativeDiagnostics();
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout body=new LinearLayout(this); body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(40,24,40,24);
        TextView title=new TextView(this); title.setText("WumpaForge — SHIELD Pro 2019 development"); title.setTextSize(24); body.addView(title);
        TextView help=new TextView(this); help.setText("Bring your own USA Xbox assets. This build has not been tested on a Shield.\nUse the remote D-pad or a paired gamepad to select an action."); body.addView(help);
        Button diagnostics=new Button(this); diagnostics.setText("Run memory and graphics diagnostics"); body.addView(diagnostics);
        Button controllers=new Button(this); controllers.setText("List connected controllers"); body.addView(controllers);
        Button play=new Button(this); play.setText("Start game"); body.addView(play);
        report=new TextView(this); report.setTextIsSelectable(true); report.setTextSize(16);
        ScrollView scroll=new ScrollView(this); scroll.addView(report); body.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        setContentView(body); diagnostics.requestFocus();
        report.setText(deviceSummary());
        diagnostics.setOnClickListener(v->{
            if(busy)return;
            String gate=deviceGate(); if(gate!=null){report.setText(gate);return;}
            busy=true; report.setText("Running sparse memory and desktop EGL checks…");
            new Thread(()->{
                String result;
                try {System.loadLibrary("shield_probe");result=nativeDiagnostics();}
                catch(Throwable problem){result="Diagnostic failed: "+problem;}
                final String text=result;
                runOnUiThread(()->{busy=false;report.setText(deviceSummary()+"\n"+text);});
            },"ShieldDiagnostics").start();
        });
        controllers.setOnClickListener(v->report.setText(controllerSummary()));
        play.setOnClickListener(v->{
            String gate=deviceGate(); if(gate!=null){report.setText(gate);return;}
            File external=getExternalFilesDir(null);
            if(external==null || !new File(external,"assets/default.xbe").isFile()) {
                report.setText("Game assets are missing. Use the documented host-side verify/import command before starting.\nApp storage: "+external);return;
            }
            startActivity(new Intent(this,GameActivity.class));
        });
    }
    private String deviceGate(){
        if(!"NVIDIA".equalsIgnoreCase(Build.MANUFACTURER)||!"mdarcy".equals(Build.DEVICE))return "This development build targets NVIDIA SHIELD TV Pro 2019 (mdarcy) only.\n"+deviceSummary();
        if(Build.VERSION.SDK_INT<30||!Arrays.asList(Build.SUPPORTED_64_BIT_ABIS).contains("arm64-v8a"))return "Android API 30+ and ARM64 application support are required.";
        return null;
    }
    private String deviceSummary(){return Build.MANUFACTURER+" / "+Build.DEVICE+" / API "+Build.VERSION.SDK_INT+" / "+Arrays.toString(Build.SUPPORTED_ABIS);}
    private String controllerSummary(){
        StringBuilder text=new StringBuilder("Android input devices (pair Bluetooth controllers in Shield settings):\n");
        for(int id:InputDevice.getDeviceIds()){
            InputDevice d=InputDevice.getDevice(id); if(d==null)continue;
            int sources=d.getSources();
            if((sources&InputDevice.SOURCE_GAMEPAD)==InputDevice.SOURCE_GAMEPAD || (sources&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK){
                text.append(d.getName()).append(" — vendor ").append(d.getVendorId()).append(", product ").append(d.getProductId()).append('\n');
            }
        }
        return text.append("Game buttons use SDL mappings. Unmapped devices may need a mapping; physical Bluetooth behavior is unverified.").toString();
    }
}
