package com.nebula360.studio;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.net.Uri;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;

public class SpherePreview extends GLSurfaceView {
    public interface SurfaceReadyListener {
        void onSurfaceReady(SurfaceTexture texture);
    }

    private final SphereRenderer sphereRenderer;
    private float previousX, previousY;
    private float yaw = 0f, pitch = 0f, zoom = 78f;
    private final ScaleGestureDetector scaleDetector;

    public SpherePreview(Context context) {
        super(context);
        setEGLContextClientVersion(2);
        setPreserveEGLContextOnPause(true);
        sphereRenderer = new SphereRenderer();
        setRenderer(sphereRenderer);
        setRenderMode(RENDERMODE_CONTINUOUSLY);
        scaleDetector = new ScaleGestureDetector(context, new ScaleGestureDetector.SimpleOnScaleGestureListener() {
            @Override public boolean onScale(ScaleGestureDetector detector) {
                zoom = Math.max(45f, Math.min(105f, zoom / detector.getScaleFactor()));
                sphereRenderer.setCamera(yaw, pitch, zoom);
                return true;
            }
        });
    }

    public void setSurfaceReadyListener(SurfaceReadyListener listener) {
        sphereRenderer.listener = listener;
    }

    public void setVideoUri(Uri uri) {
        sphereRenderer.uri = uri;
        queueEvent(() -> sphereRenderer.requestSurfaceCallback());
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        scaleDetector.onTouchEvent(event);
        if (event.getPointerCount() == 1) {
            if (event.getAction() == MotionEvent.ACTION_DOWN) {
                previousX = event.getX();
                previousY = event.getY();
                return true;
            } else if (event.getAction() == MotionEvent.ACTION_MOVE) {
                float dx = event.getX() - previousX;
                float dy = event.getY() - previousY;
                previousX = event.getX();
                previousY = event.getY();
                yaw += dx * 0.22f;
                pitch = Math.max(-85f, Math.min(85f, pitch + dy * 0.22f));
                sphereRenderer.setCamera(yaw, pitch, zoom);
                return true;
            }
        }
        return true;
    }

    private static final class SphereRenderer implements Renderer, SurfaceTexture.OnFrameAvailableListener {
        volatile SurfaceReadyListener listener;
        volatile Uri uri;
        private SurfaceTexture surfaceTexture;
        private int textureId;
        private int program;
        private int positionHandle, uvHandle, matrixHandle, texMatrixHandle, samplerHandle;
        private FloatBuffer vertices;
        private ShortBuffer indices;
        private int indexCount;
        private final float[] projection = new float[16];
        private volatile float aspectRatio = 1f;
        private final float[] view = new float[16];
        private final float[] model = new float[16];
        private final float[] mvp = new float[16];
        private final float[] texMatrix = new float[16];
        private volatile float yaw, pitch, zoom = 78f;
        private volatile boolean frameAvailable;

        void setCamera(float y, float p, float z) {
            yaw = y; pitch = p; zoom = z;
        }

        void requestSurfaceCallback() {
            if (surfaceTexture != null && listener != null) listener.onSurfaceReady(surfaceTexture);
        }

        @Override public void onSurfaceCreated(javax.microedition.khronos.opengles.GL10 gl,
                                               javax.microedition.khronos.egl.EGLConfig config) {
            GLES20.glClearColor(0.02f, 0.03f, 0.08f, 1f);
            GLES20.glEnable(GLES20.GL_DEPTH_TEST);
            textureId = createExternalTexture();
            surfaceTexture = new SurfaceTexture(textureId);
            surfaceTexture.setOnFrameAvailableListener(this);
            surfaceTexture.setDefaultBufferSize(1920, 960);
            program = createProgram(VERTEX_SHADER, FRAGMENT_SHADER);
            positionHandle = GLES20.glGetAttribLocation(program, "aPosition");
            uvHandle = GLES20.glGetAttribLocation(program, "aTexCoord");
            matrixHandle = GLES20.glGetUniformLocation(program, "uMvp");
            texMatrixHandle = GLES20.glGetUniformLocation(program, "uTexMatrix");
            samplerHandle = GLES20.glGetUniformLocation(program, "sTexture");
            buildSphere(64, 40);
            Matrix.setLookAtM(view, 0, 0f, 0f, 0f, 0f, 0f, -1f, 0f, 1f, 0f);
            Matrix.setIdentityM(texMatrix, 0);
            if (listener != null) listener.onSurfaceReady(surfaceTexture);
        }

        @Override public void onSurfaceChanged(javax.microedition.khronos.opengles.GL10 gl, int width, int height) {
            GLES20.glViewport(0, 0, width, height);
            aspectRatio = width / (float) Math.max(1, height);
            Matrix.perspectiveM(projection, 0, zoom, aspectRatio, 0.1f, 10f);
        }

        @Override public void onDrawFrame(javax.microedition.khronos.opengles.GL10 gl) {
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);
            Matrix.perspectiveM(projection, 0, zoom, aspectRatio, 0.1f, 10f);
            if (surfaceTexture != null && frameAvailable) {
                surfaceTexture.updateTexImage();
                surfaceTexture.getTransformMatrix(texMatrix);
                frameAvailable = false;
            }
            Matrix.setIdentityM(model, 0);
            Matrix.rotateM(model, 0, yaw, 0f, 1f, 0f);
            Matrix.rotateM(model, 0, pitch, 1f, 0f, 0f);
            Matrix.multiplyMM(mvp, 0, view, 0, model, 0);
            Matrix.multiplyMM(mvp, 0, projection, 0, mvp, 0);

            GLES20.glUseProgram(program);
            GLES20.glUniformMatrix4fv(matrixHandle, 1, false, mvp, 0);
            GLES20.glUniformMatrix4fv(texMatrixHandle, 1, false, texMatrix, 0);
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, textureId);
            GLES20.glUniform1i(samplerHandle, 0);

            vertices.position(0);
            GLES20.glEnableVertexAttribArray(positionHandle);
            GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 20, vertices);
            vertices.position(3);
            GLES20.glEnableVertexAttribArray(uvHandle);
            GLES20.glVertexAttribPointer(uvHandle, 2, GLES20.GL_FLOAT, false, 20, vertices);
            GLES20.glDrawElements(GLES20.GL_TRIANGLES, indexCount, GLES20.GL_UNSIGNED_SHORT, indices);
            GLES20.glDisableVertexAttribArray(positionHandle);
            GLES20.glDisableVertexAttribArray(uvHandle);
        }

        @Override public void onFrameAvailable(SurfaceTexture surfaceTexture) {
            frameAvailable = true;
        }

        private int createExternalTexture() {
            int[] tex = new int[1];
            GLES20.glGenTextures(1, tex, 0);
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, tex[0]);
            GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
            GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
            return tex[0];
        }

        private void buildSphere(int segments, int rings) {
            float[] data = new float[(segments + 1) * (rings + 1) * 5];
            short[] idx = new short[segments * rings * 6];
            int p = 0;
            for (int y = 0; y <= rings; y++) {
                float v = y / (float) rings;
                float theta = (float) (Math.PI * v);
                for (int x = 0; x <= segments; x++) {
                    float u = x / (float) segments;
                    float phi = (float) (2.0 * Math.PI * u);
                    float sx = (float) (-Math.sin(theta) * Math.sin(phi));
                    float sy = (float) Math.cos(theta);
                    float sz = (float) (Math.sin(theta) * Math.cos(phi));
                    data[p++] = sx; data[p++] = sy; data[p++] = sz;
                    data[p++] = u; data[p++] = v;
                }
            }
            int q = 0;
            for (int y = 0; y < rings; y++) {
                for (int x = 0; x < segments; x++) {
                    short a = (short) (y * (segments + 1) + x);
                    short b = (short) (a + segments + 1);
                    idx[q++] = a; idx[q++] = b; idx[q++] = (short) (a + 1);
                    idx[q++] = (short) (a + 1); idx[q++] = b; idx[q++] = (short) (b + 1);
                }
            }
            vertices = ByteBuffer.allocateDirect(data.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
            vertices.put(data).position(0);
            indices = ByteBuffer.allocateDirect(idx.length * 2).order(ByteOrder.nativeOrder()).asShortBuffer();
            indices.put(idx).position(0);
            indexCount = idx.length;
        }

        private int createProgram(String vertex, String fragment) {
            int vs = compileShader(GLES20.GL_VERTEX_SHADER, vertex);
            int fs = compileShader(GLES20.GL_FRAGMENT_SHADER, fragment);
            int p = GLES20.glCreateProgram();
            GLES20.glAttachShader(p, vs);
            GLES20.glAttachShader(p, fs);
            GLES20.glLinkProgram(p);
            int[] status = new int[1];
            GLES20.glGetProgramiv(p, GLES20.GL_LINK_STATUS, status, 0);
            if (status[0] == 0) throw new RuntimeException("Falha ao linkar shader: " + GLES20.glGetProgramInfoLog(p));
            GLES20.glDeleteShader(vs);
            GLES20.glDeleteShader(fs);
            return p;
        }

        private int compileShader(int type, String source) {
            int shader = GLES20.glCreateShader(type);
            GLES20.glShaderSource(shader, source);
            GLES20.glCompileShader(shader);
            int[] status = new int[1];
            GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, status, 0);
            if (status[0] == 0) throw new RuntimeException("Falha no shader: " + GLES20.glGetShaderInfoLog(shader));
            return shader;
        }

        private static final String VERTEX_SHADER =
                "attribute vec4 aPosition;\n" +
                "attribute vec2 aTexCoord;\n" +
                "uniform mat4 uMvp;\n" +
                "uniform mat4 uTexMatrix;\n" +
                "varying vec2 vTexCoord;\n" +
                "void main(){ gl_Position=uMvp*aPosition; vTexCoord=(uTexMatrix*vec4(aTexCoord,0.0,1.0)).xy; }\n";

        private static final String FRAGMENT_SHADER =
                "#extension GL_OES_EGL_image_external : require\n" +
                "precision mediump float;\n" +
                "uniform samplerExternalOES sTexture;\n" +
                "varying vec2 vTexCoord;\n" +
                "void main(){ gl_FragColor=texture2D(sTexture, vec2(vTexCoord.x, 1.0-vTexCoord.y)); }\n";
    }
}
