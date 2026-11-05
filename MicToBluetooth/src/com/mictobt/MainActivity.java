package com.mictobt;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

public class MainActivity extends Activity implements AudioEngine.Listener {

    private static final int REQ_PERMS = 1;

    private AudioEngine engine;
    private final Handler ui = new Handler(Looper.getMainLooper());

    private RadioButton rbCall, rbMedia;
    private Button btnToggle;
    private TextView tvStatus, tvGain, tvBt;
    private ProgressBar level;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        engine = new AudioEngine(this, this);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(buildUi());
        refreshBt();
    }

    private int dp(int v) {
        return (int) (v * getResources().getDisplayMetrics().density + 0.5f);
    }

    private View buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        root.setPadding(dp(20), dp(20), dp(20), dp(20));

        TextView title = new TextView(this);
        title.setText("מיקרופון ← בלוטוס");
        title.setTextSize(24);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setGravity(Gravity.CENTER);
        root.addView(title);

        tvBt = new TextView(this);
        tvBt.setTextSize(14);
        tvBt.setGravity(Gravity.CENTER);
        tvBt.setPadding(0, dp(8), 0, dp(16));
        root.addView(tvBt);

        TextView modeLabel = new TextView(this);
        modeLabel.setText("בחר ערוץ:");
        modeLabel.setTextSize(18);
        modeLabel.setTypeface(Typeface.DEFAULT_BOLD);
        root.addView(modeLabel);

        RadioGroup rg = new RadioGroup(this);
        rbCall = new RadioButton(this);
        rbCall.setId(View.generateViewId());
        rbCall.setText("ערוץ שיחה (SCO) – השהיה נמוכה, איכות טלפונית");
        rbCall.setTextSize(16);
        rbCall.setPadding(dp(8), dp(12), dp(8), dp(12));
        rbMedia = new RadioButton(this);
        rbMedia.setId(View.generateViewId());
        rbMedia.setText("ערוץ מוזיקה (A2DP) – איכות גבוהה, השהיה גבוהה יותר");
        rbMedia.setTextSize(16);
        rbMedia.setPadding(dp(8), dp(12), dp(8), dp(12));
        rg.addView(rbCall);
        rg.addView(rbMedia);
        rbCall.setChecked(true);
        rg.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup group, int checkedId) {
                // Switching channel while running: restart on the new channel.
                if (engine.isRunning()) {
                    engine.stop();
                    startEngine();
                }
            }
        });
        root.addView(rg);

        tvGain = new TextView(this);
        tvGain.setTextSize(16);
        tvGain.setPadding(0, dp(20), 0, dp(4));
        root.addView(tvGain);
        SeekBar sb = new SeekBar(this);
        sb.setMax(40); // 0.0x .. 4.0x
        sb.setProgress(10);
        setGainText(10);
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar s, int p, boolean fromUser) {
                engine.setGain(p / 10f);
                setGainText(p);
            }
            @Override public void onStartTrackingTouch(SeekBar s) { }
            @Override public void onStopTrackingTouch(SeekBar s) { }
        });
        root.addView(sb);

        TextView lvlLabel = new TextView(this);
        lvlLabel.setText("עוצמת קלט:");
        lvlLabel.setPadding(0, dp(16), 0, dp(4));
        root.addView(lvlLabel);
        level = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        level.setMax(100);
        root.addView(level, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, dp(18)));

        btnToggle = new Button(this);
        btnToggle.setTextSize(22);
        btnToggle.setTextColor(Color.WHITE);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, dp(80));
        lp.topMargin = dp(28);
        root.addView(btnToggle, lp);
        btnToggle.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (engine.isRunning()) {
                    engine.stop();
                } else {
                    startEngine();
                }
                updateButton();
            }
        });
        updateButton();

        tvStatus = new TextView(this);
        tvStatus.setTextSize(15);
        tvStatus.setGravity(Gravity.CENTER);
        tvStatus.setPadding(0, dp(16), 0, 0);
        tvStatus.setText("מוכן");
        root.addView(tvStatus);

        TextView hint = new TextView(this);
        hint.setTextSize(13);
        hint.setTextColor(Color.GRAY);
        hint.setPadding(0, dp(24), 0, 0);
        hint.setText("טיפ: כדי למנוע צפצוף (פידבק), הרחק את הטלפון מהרמקול.\n"
                + "המסך יישאר דלוק כל עוד האפליקציה פתוחה.");
        root.addView(hint);

        ScrollView sv = new ScrollView(this);
        sv.addView(root);
        return sv;
    }

    private void setGainText(int p) {
        tvGain.setText("הגברה: " + (p * 10) + "%");
    }

    private void updateButton() {
        boolean on = engine.isRunning();
        btnToggle.setText(on ? "עצור" : "התחל");
        GradientDrawable bg = new GradientDrawable();
        bg.setCornerRadius(dp(16));
        bg.setColor(on ? Color.parseColor("#C62828") : Color.parseColor("#2E7D32"));
        btnToggle.setBackground(bg);
    }

    private void refreshBt() {
        boolean a2dp = engine.hasA2dp();
        boolean sco = engine.hasSco();
        if (!a2dp && !sco) {
            tvBt.setText("⚠ לא זוהה התקן בלוטוס מחובר");
            tvBt.setTextColor(Color.parseColor("#C62828"));
        } else {
            tvBt.setText("בלוטוס מחובר ✓  (מוזיקה: " + (a2dp ? "כן" : "לא") + " | שיחה: " + (sco ? "כן" : "לא") + ")");
            tvBt.setTextColor(Color.parseColor("#2E7D32"));
        }
    }

    private void startEngine() {
        if (!ensurePermissions()) return;
        refreshBt();
        engine.start(rbMedia.isChecked() ? AudioEngine.MODE_MEDIA : AudioEngine.MODE_CALL);
        updateButton();
    }

    private boolean ensurePermissions() {
        List<String> need = new ArrayList<>();
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            need.add(Manifest.permission.RECORD_AUDIO);
        }
        if (Build.VERSION.SDK_INT >= 31
                && checkSelfPermission("android.permission.BLUETOOTH_CONNECT") != PackageManager.PERMISSION_GRANTED) {
            need.add("android.permission.BLUETOOTH_CONNECT");
        }
        if (need.isEmpty()) return true;
        requestPermissions(need.toArray(new String[0]), REQ_PERMS);
        return false;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode != REQ_PERMS) return;
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            startEngine();
        } else {
            tvStatus.setText("נדרשת הרשאת מיקרופון");
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshBt();
    }

    @Override
    protected void onDestroy() {
        engine.stop();
        super.onDestroy();
    }

    // ---- AudioEngine.Listener (called from worker thread) ----

    @Override
    public void onStatus(final String text) {
        ui.post(new Runnable() {
            @Override
            public void run() {
                tvStatus.setText(text);
            }
        });
    }

    @Override
    public void onLevel(final int percent) {
        ui.post(new Runnable() {
            @Override
            public void run() {
                level.setProgress(percent);
            }
        });
    }

    @Override
    public void onStopped() {
        ui.post(new Runnable() {
            @Override
            public void run() {
                updateButton();
            }
        });
    }
}
