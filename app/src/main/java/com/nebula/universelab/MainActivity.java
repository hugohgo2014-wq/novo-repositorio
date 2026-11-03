package com.nebula.universelab;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.graphics.Color;
import android.graphics.Typeface;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;
import android.opengl.GLSurfaceView;
import android.view.GestureDetector;
import android.view.ScaleGestureDetector;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MainActivity extends Activity {
    static { System.loadLibrary("nebula"); }

    private GLSurfaceView glView;
    private TextView statusView;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private GestureDetector gestures;
    private ScaleGestureDetector scaler;
    private float lastX, lastY, scaleDelta = 1.0f;
    private boolean fingerDown = false;

    private native void nativeInit(int width, int height);
    private native void nativeResize(int width, int height);
    private native void nativeRender(float deltaSeconds);
    private native void nativeTouch(int action, float x, float y, float dx, float dy, float scale);
    private native void nativeDoubleTap(float x, float y);
    private native void nativeCommand(int command);
    private native String nativeGetStatus();
    private native void nativeSetParameters(float massMultiplier, float radiusMultiplier, float spinMultiplier);

    private final Runnable statusUpdater = new Runnable() {
        @Override public void run() {
            if (statusView != null) statusView.setText(nativeGetStatus());
            handler.postDelayed(this, 700);
        }
    };

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);

        FrameLayout root = new FrameLayout(this);
        glView = new GLSurfaceView(this);
        glView.setEGLContextClientVersion(3);
        glView.setPreserveEGLContextOnPause(true);
        glView.setRenderer(new GLSurfaceView.Renderer() {
            private long previous = 0;
            @Override public void onSurfaceCreated(GL10 gl, EGLConfig config) {
                nativeInit(glView.getWidth(), glView.getHeight());
                previous = System.nanoTime();
            }
            @Override public void onSurfaceChanged(GL10 gl, int width, int height) { nativeResize(width, height); }
            @Override public void onDrawFrame(GL10 gl) {
                long now = System.nanoTime();
                float dt = previous == 0 ? 0.016f : Math.min(0.05f, (now - previous) / 1.0e9f);
                previous = now;
                nativeRender(dt);
            }
        });
        root.addView(glView, new FrameLayout.LayoutParams(-1, -1));
        buildOverlay(root);
        installGestures();
        setContentView(root);
        handler.post(statusUpdater);
    }

    private void buildOverlay(FrameLayout root) {
        LinearLayout top = new LinearLayout(this);
        top.setOrientation(LinearLayout.VERTICAL);
        top.setPadding(dp(12), dp(8), dp(12), dp(8));
        top.setBackgroundColor(0xB8101728);
        FrameLayout.LayoutParams tp = new FrameLayout.LayoutParams(-1, -2, Gravity.TOP);
        tp.setMargins(dp(8), dp(8), dp(8), 0);
        root.addView(top, tp);

        TextView title = new TextView(this);
        title.setText("NEBULA  /  UNIVERSE SANDBOX     •     toque duplo em um corpo para focar");
        title.setTextColor(0xFF8FE8FF);
        title.setTextSize(13);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        top.addView(title);

        statusView = new TextView(this);
        statusView.setTextColor(Color.WHITE);
        statusView.setTextSize(11);
        statusView.setMaxLines(3);
        statusView.setText("Inicializando simulação...");
        top.addView(statusView);

        LinearLayout controls = new LinearLayout(this);
        controls.setOrientation(LinearLayout.VERTICAL);
        controls.setPadding(dp(8), dp(6), dp(8), dp(6));
        controls.setBackgroundColor(0xB8101728);
        FrameLayout.LayoutParams cp = new FrameLayout.LayoutParams(-1, -2, Gravity.BOTTOM);
        cp.setMargins(dp(8), 0, dp(8), dp(8));
        root.addView(controls, cp);

        LinearLayout row1 = new LinearLayout(this);
        row1.setGravity(Gravity.CENTER);
        controls.addView(row1, new LinearLayout.LayoutParams(-1, -2));
        addButton(row1, "Ⅱ / ▶", 0);
        addButton(row1, "TEMPO ×", 1);
        addButton(row1, "CÂMERA", 2);
        addButton(row1, "+ PLANETA", 3);
        addButton(row1, "METEORO", 9);

        LinearLayout row2 = new LinearLayout(this);
        row2.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams r2 = new LinearLayout.LayoutParams(-1, -2);
        r2.topMargin = dp(3);
        controls.addView(row2, r2);
        addButton(row2, "GRAVIDADE", 4);
        addButton(row2, "LUZ / VISÃO", 5);
        addButton(row2, "EDITAR", 6);
        addButton(row2, "REMOVER", 7);
        addButton(row2, "RESET", 8);
    }

    private void addButton(LinearLayout row, String label, int command) {
        Button b = new Button(this);
        b.setText(label);
        b.setTextSize(10);
        b.setAllCaps(false);
        b.setTextColor(Color.WHITE);
        b.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFF263B58));
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, dp(42), 1f);
        p.setMargins(dp(2), 0, dp(2), 0);
        row.addView(b, p);
        b.setOnClickListener(v -> {
            if (command == 6) showEditDialog();
            else {
                nativeCommand(command);
                if (command == 3) Toast.makeText(this, "Novo planeta adicionado", Toast.LENGTH_SHORT).show();
                if (command == 9) Toast.makeText(this, "Asteroide lançado em rota de impacto", Toast.LENGTH_SHORT).show();
                if (command == 7) Toast.makeText(this, "Corpo selecionado removido", Toast.LENGTH_SHORT).show();
            }
        });
    }

    private void showEditDialog() {
        LinearLayout panel = new LinearLayout(this);
        panel.setPadding(dp(20), dp(12), dp(20), dp(4));
        panel.setOrientation(LinearLayout.VERTICAL);
        SeekBar mass = makeSeek(panel, "Massa (% do valor atual)", 500, 100);
        SeekBar radius = makeSeek(panel, "Raio (% do valor atual)", 500, 100);
        SeekBar spin = makeSeek(panel, "Rotação (% do valor atual)", 500, 100);
        new AlertDialog.Builder(this).setTitle("Editar corpo selecionado")
                .setMessage("Ajuste os parâmetros físicos. 100% mantém o valor atual.")
                .setView(panel)
                .setNegativeButton("Cancelar", null)
                .setPositiveButton("Aplicar", (d, w) -> nativeSetParameters(
                        Math.max(0.05f, mass.getProgress() / 100f),
                        Math.max(0.05f, radius.getProgress() / 100f),
                        spin.getProgress() / 100f))
                .show();
    }

    private SeekBar makeSeek(LinearLayout panel, String label, int max, int progress) {
        TextView t = new TextView(this);
        t.setText(label);
        t.setTextColor(Color.WHITE);
        panel.addView(t);
        SeekBar s = new SeekBar(this);
        s.setMax(max);
        s.setProgress(progress);
        panel.addView(s);
        return s;
    }

    private void installGestures() {
        gestures = new GestureDetector(this, new GestureDetector.SimpleOnGestureListener() {
            @Override public boolean onDown(MotionEvent e) { return true; }
            @Override public boolean onDoubleTap(MotionEvent e) {
                nativeDoubleTap(e.getX(), e.getY());
                return true;
            }
        });
        scaler = new ScaleGestureDetector(this, new ScaleGestureDetector.SimpleOnScaleGestureListener() {
            @Override public boolean onScale(ScaleGestureDetector detector) {
                scaleDelta *= detector.getScaleFactor();
                return true;
            }
        });
        glView.setOnTouchListener((v, e) -> {
            gestures.onTouchEvent(e);
            scaler.onTouchEvent(e);
            int action = e.getActionMasked();
            float x = e.getX(), y = e.getY(), dx = 0, dy = 0;
            if (action == MotionEvent.ACTION_DOWN) {
                lastX = x; lastY = y; fingerDown = true;
            } else if (action == MotionEvent.ACTION_MOVE && fingerDown) {
                dx = x - lastX; dy = y - lastY;
                lastX = x; lastY = y;
            } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                fingerDown = false;
            }
            nativeTouch(action == MotionEvent.ACTION_DOWN ? 0 : action == MotionEvent.ACTION_UP ? 1 : (e.getPointerCount() > 1 ? 3 : 2),
                    x, y, dx, dy, scaleDelta);
            scaleDelta = 1.0f;
            return true;
        });
    }

    private int dp(float value) { return (int)(value * getResources().getDisplayMetrics().density + 0.5f); }

    @Override protected void onResume() { super.onResume(); if (glView != null) glView.onResume(); handler.removeCallbacks(statusUpdater); handler.post(statusUpdater); }
    @Override protected void onPause() { handler.removeCallbacks(statusUpdater); if (glView != null) glView.onPause(); super.onPause(); }
}
