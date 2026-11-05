package com.mictobt;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.NoiseSuppressor;
import android.os.Build;

import java.lang.reflect.Method;
import java.util.List;

/**
 * Captures the phone's built-in microphone and plays it back to a Bluetooth device.
 *
 * MODE_CALL  : routes through the Bluetooth "call" profile (SCO / HFP) - low latency, narrow band.
 * MODE_MEDIA : routes through the Bluetooth "media" profile (A2DP) - high quality, more latency.
 */
public class AudioEngine {

    public static final int MODE_CALL = 0;
    public static final int MODE_MEDIA = 1;

    public interface Listener {
        void onStatus(String text);
        void onLevel(int percent);
        void onStopped();
    }

    private final Context ctx;
    private final AudioManager am;
    private final Listener listener;

    private volatile boolean running = false;
    private volatile float gain = 1.0f;
    private Thread worker;
    private int mode;
    private boolean scoStartedLegacy = false;
    private boolean commDeviceSet = false;
    private int savedAudioMode = AudioManager.MODE_NORMAL;
    private BroadcastReceiver scoReceiver;
    private volatile int scoState = AudioManager.SCO_AUDIO_STATE_DISCONNECTED;

    public AudioEngine(Context ctx, Listener listener) {
        this.ctx = ctx.getApplicationContext();
        this.am = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);
        this.listener = listener;
    }

    public boolean isRunning() {
        return running;
    }

    public void setGain(float g) {
        gain = g;
    }

    // ---------- Bluetooth detection ----------

    public boolean hasA2dp() {
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
            if (d.getType() == AudioDeviceInfo.TYPE_BLUETOOTH_A2DP || d.getType() == 26 /*BLE_HEADSET*/
                    || d.getType() == 27 /*BLE_SPEAKER*/) return true;
        }
        return false;
    }

    public boolean hasSco() {
        if (Build.VERSION.SDK_INT >= 31 && findCommDevice() != null) return true;
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
            if (d.getType() == AudioDeviceInfo.TYPE_BLUETOOTH_SCO) return true;
        }
        return am.isBluetoothScoAvailableOffCall() && hasA2dp();
    }

    private AudioDeviceInfo builtinMic() {
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_INPUTS)) {
            if (d.getType() == AudioDeviceInfo.TYPE_BUILTIN_MIC) return d;
        }
        return null;
    }

    private AudioDeviceInfo findOutput(int... types) {
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
            for (int t : types) if (d.getType() == t) return d;
        }
        return null;
    }

    /** API 31+: AudioManager.getAvailableCommunicationDevices(), via reflection. */
    @SuppressWarnings("unchecked")
    private AudioDeviceInfo findCommDevice() {
        try {
            Method m = AudioManager.class.getMethod("getAvailableCommunicationDevices");
            List<AudioDeviceInfo> list = (List<AudioDeviceInfo>) m.invoke(am);
            for (AudioDeviceInfo d : list) {
                if (d.getType() == AudioDeviceInfo.TYPE_BLUETOOTH_SCO) return d;
            }
            for (AudioDeviceInfo d : list) {
                if (d.getType() == 26 /*TYPE_BLE_HEADSET*/) return d;
            }
        } catch (Throwable ignored) {
        }
        return null;
    }

    // ---------- start / stop ----------

    public synchronized void start(final int mode) {
        if (running) return;
        this.mode = mode;
        running = true;
        worker = new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    loop();
                } catch (Throwable t) {
                    listener.onStatus("שגיאה: " + t.getMessage());
                } finally {
                    releaseRouting();
                    running = false;
                    listener.onLevel(0);
                    listener.onStopped();
                }
            }
        }, "mic2bt");
        worker.setPriority(Thread.MAX_PRIORITY);
        worker.start();
    }

    public synchronized void stop() {
        running = false;
        Thread t = worker;
        if (t != null) {
            try {
                t.join(1500);
            } catch (InterruptedException ignored) {
            }
        }
        worker = null;
    }

    private void setupCallRouting() throws InterruptedException {
        savedAudioMode = am.getMode();
        am.setMode(AudioManager.MODE_IN_COMMUNICATION);

        if (Build.VERSION.SDK_INT >= 31) {
            AudioDeviceInfo dev = findCommDevice();
            if (dev != null) {
                try {
                    Method m = AudioManager.class.getMethod("setCommunicationDevice", AudioDeviceInfo.class);
                    commDeviceSet = (Boolean) m.invoke(am, dev);
                } catch (Throwable ignored) {
                }
            }
        }
        if (!commDeviceSet) {
            // Legacy path (also works on Android 13, deprecated)
            registerScoReceiver();
            am.startBluetoothSco();
            am.setBluetoothScoOn(true);
            scoStartedLegacy = true;
            listener.onStatus("מתחבר לבלוטוס (ערוץ שיחה)...");
            long deadline = System.currentTimeMillis() + 5000;
            while (running && scoState != AudioManager.SCO_AUDIO_STATE_CONNECTED
                    && System.currentTimeMillis() < deadline) {
                Thread.sleep(100);
            }
            if (scoState != AudioManager.SCO_AUDIO_STATE_CONNECTED) {
                listener.onStatus("אזהרה: לא אושר חיבור SCO, ממשיך בכל זאת");
            }
        } else {
            Thread.sleep(400); // give the SCO link a moment to come up
        }
    }

    private void registerScoReceiver() {
        scoReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context c, Intent i) {
                scoState = i.getIntExtra(AudioManager.EXTRA_SCO_AUDIO_STATE, AudioManager.SCO_AUDIO_STATE_ERROR);
            }
        };
        Intent sticky = ctx.registerReceiver(scoReceiver,
                new IntentFilter(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED));
        if (sticky != null) {
            scoState = sticky.getIntExtra(AudioManager.EXTRA_SCO_AUDIO_STATE, scoState);
        }
    }

    private void releaseRouting() {
        if (mode != MODE_CALL) return;
        try {
            if (commDeviceSet) {
                Method m = AudioManager.class.getMethod("clearCommunicationDevice");
                m.invoke(am);
                commDeviceSet = false;
            }
        } catch (Throwable ignored) {
        }
        if (scoStartedLegacy) {
            am.setBluetoothScoOn(false);
            am.stopBluetoothSco();
            scoStartedLegacy = false;
        }
        if (scoReceiver != null) {
            try {
                ctx.unregisterReceiver(scoReceiver);
            } catch (Throwable ignored) {
            }
            scoReceiver = null;
        }
        am.setMode(savedAudioMode);
    }

    // ---------- audio loop ----------

    private void loop() throws Exception {
        final boolean call = mode == MODE_CALL;
        final int rate = call ? 16000 : 48000;
        final int framesPerChunk = call ? rate / 100 : rate / 200; // 10ms / 5ms chunks

        if (call) setupCallRouting();
        if (!running) return;

        int minRec = AudioRecord.getMinBufferSize(rate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
        int minPlay = AudioTrack.getMinBufferSize(rate, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT);

        AudioRecord rec = new AudioRecord.Builder()
                .setAudioSource(call ? MediaRecorder.AudioSource.VOICE_COMMUNICATION : MediaRecorder.AudioSource.MIC)
                .setAudioFormat(new AudioFormat.Builder()
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                        .setSampleRate(rate)
                        .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                        .build())
                .setBufferSizeInBytes(Math.max(minRec, framesPerChunk * 2 * 4))
                .build();

        AudioAttributes attrs = new AudioAttributes.Builder()
                .setUsage(call ? AudioAttributes.USAGE_VOICE_COMMUNICATION : AudioAttributes.USAGE_MEDIA)
                .setContentType(call ? AudioAttributes.CONTENT_TYPE_SPEECH : AudioAttributes.CONTENT_TYPE_MUSIC)
                .build();
        AudioTrack.Builder tb = new AudioTrack.Builder()
                .setAudioAttributes(attrs)
                .setAudioFormat(new AudioFormat.Builder()
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                        .setSampleRate(rate)
                        .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                        .build())
                .setBufferSizeInBytes(Math.max(minPlay, framesPerChunk * 2 * 2))
                .setTransferMode(AudioTrack.MODE_STREAM);
        try { // API 26+: AudioTrack.Builder.setPerformanceMode(PERFORMANCE_MODE_LOW_LATENCY = 1)
            Method pm = AudioTrack.Builder.class.getMethod("setPerformanceMode", int.class);
            pm.invoke(tb, 1);
        } catch (Throwable ignored) {
        }
        AudioTrack track = tb.build();

        if (rec.getState() != AudioRecord.STATE_INITIALIZED || track.getState() != AudioTrack.STATE_INITIALIZED) {
            rec.release();
            track.release();
            throw new IllegalStateException("לא ניתן לפתוח את המיקרופון / הרמקול");
        }

        // Always capture from the phone's own microphone (not the headset mic).
        AudioDeviceInfo mic = builtinMic();
        if (mic != null) rec.setPreferredDevice(mic);
        AudioDeviceInfo out = call
                ? findOutput(AudioDeviceInfo.TYPE_BLUETOOTH_SCO)
                : findOutput(AudioDeviceInfo.TYPE_BLUETOOTH_A2DP, 26, 27);
        if (out != null) track.setPreferredDevice(out);

        AcousticEchoCanceler aec = null;
        NoiseSuppressor ns = null;
        if (call) {
            try {
                if (AcousticEchoCanceler.isAvailable()) {
                    aec = AcousticEchoCanceler.create(rec.getAudioSessionId());
                    if (aec != null) aec.setEnabled(true);
                }
                if (NoiseSuppressor.isAvailable()) {
                    ns = NoiseSuppressor.create(rec.getAudioSessionId());
                    if (ns != null) ns.setEnabled(true);
                }
            } catch (Throwable ignored) {
            }
        }

        short[] buf = new short[framesPerChunk];
        try {
            rec.startRecording();
            track.play();
            listener.onStatus(call
                    ? "פעיל – ערוץ שיחה (השהיה נמוכה, " + rate / 1000 + "kHz)"
                    : "פעיל – ערוץ מוזיקה (איכות גבוהה, " + rate / 1000 + "kHz)");
            int levelCounter = 0;
            int peak = 0;
            while (running) {
                int n = rec.read(buf, 0, buf.length);
                if (n <= 0) continue;
                float g = gain;
                for (int i = 0; i < n; i++) {
                    int v = (int) (buf[i] * g);
                    if (v > 32767) v = 32767;
                    else if (v < -32768) v = -32768;
                    buf[i] = (short) v;
                    int a = v < 0 ? -v : v;
                    if (a > peak) peak = a;
                }
                track.write(buf, 0, n);
                if (++levelCounter >= 8) {
                    listener.onLevel(peak * 100 / 32768);
                    levelCounter = 0;
                    peak = 0;
                }
            }
        } finally {
            try { rec.stop(); } catch (Throwable ignored) { }
            try { track.stop(); } catch (Throwable ignored) { }
            rec.release();
            track.release();
            if (aec != null) aec.release();
            if (ns != null) ns.release();
            listener.onStatus("נעצר");
        }
    }
}
