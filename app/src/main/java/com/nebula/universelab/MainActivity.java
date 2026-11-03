package com.nebula.universelab;

import android.app.Activity;
import android.app.AlertDialog;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.GestureDetector;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;
import android.view.WindowManager;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.FrameLayout;

import java.util.Locale;
import java.util.HashMap;
import java.util.Map;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/** Android entry point and touch/UI bridge for the native NEBULA engine. */
public final class MainActivity extends Activity {
    static {
        System.loadLibrary("nebula");
    }

    private GLSurfaceView glView;
    private TextView statusView;
    private TextView placementHint;
    private final Map<Integer, TextView> commandButtons = new HashMap<>();
    private final Handler uiHandler = new Handler(Looper.getMainLooper());
    private boolean activityResumed = false;

    private native void nativeInit(int width, int height);
    private native void nativeResize(int width, int height);
    private native void nativeRender(float deltaSeconds);
    private native void nativeTouch(int action, float x, float y,
                                    float dx, float dy, float scale);
    private native void nativeDoubleTap(float x, float y);
    private native void nativePlacePlanet(float x, float y);
    private native void nativeCommand(int command);
    private native String nativeGetStatus();
    private native void nativeSetParameters(float massMultiplier,
                                            float radiusMultiplier,
                                            float spinMultiplier);

    private final Runnable statusUpdater = new Runnable() {
        @Override public void run() {
            if (!activityResumed || statusView == null) return;
            try {
                String status = nativeGetStatus();
                if (status != null) {
                    statusView.setText(status);
                    boolean placing = status.contains("Posicionar: TOQUE NO ESPAÇO");
                    placementHint.setVisibility(placing ? View.VISIBLE : View.GONE);
                    boolean paused = status.contains("PAUSADO");
                    boolean bird = status.contains("Câmera: PASSARINHO");
                    boolean delayedGravity = status.contains("Grav.: c retardada");
                    boolean delayedLight = status.contains("Luz: cone de luz");
                    updateButtonState(0, paused, paused ? "▶  RETOMAR" : "Ⅱ  PAUSAR");
                    updateButtonState(2, bird, bird ? "◎  PASSARINHO" : "◎  CÂMERA");
                    updateButtonState(3, placing, placing ? "✕  CANCELAR" : "＋  PLANETA");
                    updateButtonState(4, delayedGravity, delayedGravity ? "GRAV. RETARDADA" : "GRAV. NEWTON");
                    updateButtonState(5, delayedLight, delayedLight ? "LUZ ATRASADA" : "LUZ ATUAL");
                }
            } catch (RuntimeException ignored) {
                // The GL context can be recreated while Android is changing windows.
            }
            uiHandler.postDelayed(this, 500L);
        }
    };

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        applyImmersiveMode();

        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(Color.rgb(2, 5, 16));

        glView = new NebulaSurfaceView();
        glView.setEGLContextClientVersion(3);
        glView.setEGLConfigChooser(8, 8, 8, 8, 24, 8);
        glView.setPreserveEGLContextOnPause(false);
        glView.setRenderer(new NebulaRenderer());
        glView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        root.addView(glView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        // Compact HUD: the simulation remains visible while controls stay grouped and legible.
        LinearLayout hud = new LinearLayout(this);
        hud.setOrientation(LinearLayout.VERTICAL);
        hud.setPadding(dp(14), dp(10), dp(14), dp(10));
        hud.setBackground(panelBackground(0xE8101D30));
        TextView brand = new TextView(this);
        brand.setText("NEBULA  /  LABORATÓRIO ORBITAL");
        brand.setTextColor(Color.rgb(118, 210, 255));
        brand.setTextSize(12f);
        brand.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        hud.addView(brand);
        statusView = new TextView(this);
        statusView.setTextColor(Color.rgb(232, 241, 255));
        statusView.setTextSize(10f);
        statusView.setMaxLines(3);
        statusView.setPadding(0, dp(5), 0, 0);
        statusView.setText("INICIALIZANDO  ·  preparando o universo...");
        hud.addView(statusView);
        FrameLayout.LayoutParams hudParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.TOP | Gravity.CENTER_HORIZONTAL);
        hudParams.setMargins(dp(10), dp(10), dp(10), 0);
        root.addView(hud, hudParams);

        placementHint = new TextView(this);
        placementHint.setText("＋  POSICIONAR PLANETA  ·  toque no ponto da órbita onde deseja criar");
        placementHint.setTextColor(Color.WHITE);
        placementHint.setTextSize(12f);
        placementHint.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        placementHint.setGravity(Gravity.CENTER);
        placementHint.setPadding(dp(14), dp(11), dp(14), dp(11));
        placementHint.setBackground(panelBackground(0xEE087F8C));
        placementHint.setVisibility(View.GONE);
        FrameLayout.LayoutParams hintParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.CENTER_HORIZONTAL | Gravity.CENTER_VERTICAL);
        hintParams.setMargins(dp(18), 0, dp(18), dp(70));
        root.addView(placementHint, hintParams);

        LinearLayout dock = new LinearLayout(this);
        dock.setOrientation(LinearLayout.VERTICAL);
        dock.setPadding(dp(7), dp(7), dp(7), dp(7));
        dock.setBackground(panelBackground(0xF20A1425));
        LinearLayout rowA = new LinearLayout(this);
        rowA.setOrientation(LinearLayout.HORIZONTAL);
        rowA.setGravity(Gravity.CENTER);
        LinearLayout rowB = new LinearLayout(this);
        rowB.setOrientation(LinearLayout.HORIZONTAL);
        rowB.setGravity(Gravity.CENTER);
        addButton(rowA, "Ⅱ  PAUSAR", 0);
        addButton(rowA, "⏩  TEMPO", 1);
        addButton(rowA, "◎  CÂMERA", 2);
        addButton(rowA, "＋  PLANETA", 3);
        addButton(rowA, "GRAV. NEWTON", 4);
        addButton(rowB, "LUZ ATUAL", 5);
        addButton(rowB, "☄  IMPACTO", 9);
        addButton(rowB, "EDITAR", -1);
        addButton(rowB, "APAGAR", 7);
        addButton(rowB, "RESET", 8);
        dock.addView(rowA);
        dock.addView(rowB);
        TextView gestureHelp = new TextView(this);
        gestureHelp.setText("1 dedo: orbitar  ·  arraste 2 dedos: deslocar  ·  pinça: zoom  ·  toque duplo: focar");
        gestureHelp.setTextColor(Color.rgb(151, 174, 204));
        gestureHelp.setTextSize(9f);
        gestureHelp.setGravity(Gravity.CENTER);
        gestureHelp.setPadding(0, dp(5), 0, 0);
        dock.addView(gestureHelp);
        FrameLayout.LayoutParams dockParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
        dockParams.setMargins(dp(8), 0, dp(8), dp(8));
        root.addView(dock, dockParams);

        setContentView(root);
    }

    private void addButton(LinearLayout parent, String label, int command) {
        TextView button = new TextView(this);
        button.setText(label);
        button.setTextColor(Color.rgb(225, 238, 255));
        button.setTextSize(10f);
        button.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        button.setGravity(Gravity.CENTER);
        button.setPadding(dp(7), dp(10), dp(7), dp(10));
        button.setBackground(panelBackground(0xFF1A2C43));
        button.setMinHeight(dp(42));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, dp(44), 1f);
        params.setMargins(dp(2), dp(2), dp(2), dp(2));
        parent.addView(button, params);
        if (command >= 0) commandButtons.put(command, button);
        button.setOnClickListener(v -> {
            if (command == -1) showBodyEditor();
            else {
                glView.queueEvent(() -> nativeCommand(command));
                glView.requestRender();
            }
        });
    }

    private void updateButtonState(int command, boolean active, String label) {
        TextView button = commandButtons.get(command);
        if (button == null) return;
        button.setText(label);
        int bg = active ? 0xFF087F8C : 0xFF1A2C43;
        button.setBackground(panelBackground(bg));
        button.setTextColor(active ? Color.WHITE : Color.rgb(225, 238, 255));
    }

    private void showBodyEditor() {
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(dp(20), dp(8), dp(20), 0);

        TextView explanation = new TextView(this);
        explanation.setText("Ajuste os multiplicadores do corpo selecionado. 1,0× mantém o valor atual.");
        explanation.setTextSize(13f);
        content.addView(explanation);

        SeekBar mass = addEditorSlider(content, "Massa", 20);
        SeekBar radius = addEditorSlider(content, "Raio", 20);
        SeekBar spin = addEditorSlider(content, "Rotação", 20);

        new AlertDialog.Builder(this)
                .setTitle("Editar corpo selecionado")
                .setView(content)
                .setNegativeButton("Cancelar", null)
                .setPositiveButton("Aplicar", (dialog, which) -> {
                    float m = editorMultiplier(mass.getProgress());
                    float r = editorMultiplier(radius.getProgress());
                    float s = editorMultiplier(spin.getProgress());
                    glView.queueEvent(() -> nativeSetParameters(m, r, s));
                    glView.requestRender();
                }).show();
    }

    private SeekBar addEditorSlider(LinearLayout parent, String name, int initial) {
        TextView label = new TextView(this);
        label.setText(String.format(Locale.US, "%s: 1.00×", name));
        label.setTextSize(13f);
        label.setPadding(0, dp(12), 0, 0);
        parent.addView(label);
        SeekBar slider = new SeekBar(this);
        slider.setMax(100);
        slider.setProgress(initial);
        parent.addView(slider, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dp(40)));
        slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                float multiplier = editorMultiplier(progress);
                label.setText(String.format(Locale.US, "%s: %.2f×", name, multiplier));
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override public void onStopTrackingTouch(SeekBar seekBar) { }
        });
        return slider;
    }

    // Slider position 20 is exactly 1.0x; both smaller and larger values are useful.
    private float editorMultiplier(int progress) {
        if (progress <= 20) return 0.05f + (progress / 20.0f) * 0.95f;
        return 1.0f + ((progress - 20) / 80.0f) * 4.0f;
    }

    private GradientDrawable panelBackground(int color) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(dp(10));
        drawable.setStroke(dp(1), 0x663F79A8);
        return drawable;
    }

    private int dp(float value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private void applyImmersiveMode() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) applyImmersiveMode();
    }

    @Override protected void onResume() {
        super.onResume();
        activityResumed = true;
        if (glView != null) glView.onResume();
        uiHandler.removeCallbacks(statusUpdater);
        uiHandler.post(statusUpdater);
    }

    @Override protected void onPause() {
        activityResumed = false;
        uiHandler.removeCallbacks(statusUpdater);
        if (glView != null) glView.onPause();
        super.onPause();
    }

    private final class NebulaRenderer implements GLSurfaceView.Renderer {
        private long lastFrameNanos;

        @Override public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            lastFrameNanos = System.nanoTime();
            nativeInit(Math.max(1, glView.getWidth()), Math.max(1, glView.getHeight()));
        }

        @Override public void onSurfaceChanged(GL10 gl, int width, int height) {
            nativeResize(width, height);
        }

        @Override public void onDrawFrame(GL10 gl) {
            long now = System.nanoTime();
            float dt = (now - lastFrameNanos) / 1_000_000_000.0f;
            lastFrameNanos = now;
            // Clamp pauses/background stalls so returning to the app never advances days at once.
            nativeRender(Math.max(0.0f, Math.min(dt, 0.05f)));
        }
    }

    private final class NebulaSurfaceView extends GLSurfaceView {
        private final GestureDetector gestures;
        private final ScaleGestureDetector scaleGestures;
        private float previousCenterX;
        private float previousCenterY;

        NebulaSurfaceView() {
            super(MainActivity.this);
            gestures = new GestureDetector(MainActivity.this,
                    new GestureDetector.SimpleOnGestureListener() {
                        @Override public boolean onDown(MotionEvent event) { return true; }
                        @Override public boolean onScroll(MotionEvent first, MotionEvent current,
                                                         float distanceX, float distanceY) {
                            if (current.getPointerCount() == 1) {
                                nativeTouch(2, current.getX(), current.getY(),
                                        -distanceX, -distanceY, 1.0f);
                                return true;
                            }
                            return false;
                        }
                        @Override public boolean onSingleTapConfirmed(MotionEvent event) {
                            // The native placement tool ignores taps unless placement mode is active.
                            nativePlacePlanet(event.getX(), event.getY());
                            requestRender();
                            return true;
                        }
                        @Override public boolean onDoubleTap(MotionEvent event) {
                            nativeDoubleTap(event.getX(), event.getY());
                            requestRender();
                            return true;
                        }
                    });
            scaleGestures = new ScaleGestureDetector(MainActivity.this,
                    new ScaleGestureDetector.SimpleOnScaleGestureListener() {
                        @Override public boolean onScale(ScaleGestureDetector detector) {
                            nativeTouch(2, detector.getFocusX(), detector.getFocusY(),
                                    0.0f, 0.0f, detector.getScaleFactor());
                            requestRender();
                            return true;
                        }
                    });
        }

        @Override public boolean onTouchEvent(MotionEvent event) {
            scaleGestures.onTouchEvent(event);
            gestures.onTouchEvent(event);
            int action = event.getActionMasked();
            if (event.getPointerCount() >= 2) {
                float centerX = 0.0f;
                float centerY = 0.0f;
                for (int i = 0; i < event.getPointerCount(); i++) {
                    centerX += event.getX(i);
                    centerY += event.getY(i);
                }
                centerX /= event.getPointerCount();
                centerY /= event.getPointerCount();
                if (action == MotionEvent.ACTION_MOVE && !scaleGestures.isInProgress()) {
                    nativeTouch(3, centerX, centerY,
                            centerX - previousCenterX, centerY - previousCenterY, 1.0f);
                }
                previousCenterX = centerX;
                previousCenterY = centerY;
                requestRender();
                return true;
            }
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                previousCenterX = 0.0f;
                previousCenterY = 0.0f;
            }
            return true;
        }
    }
}
