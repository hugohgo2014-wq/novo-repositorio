// Player Android (NativeActivity): EGL/GLES3 + loop principal + entrada de toque/teclado.
#include <EGL/egl.h>
#include <android/asset_manager.h>
#include <android/imagedecoder.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <time.h>
#include <memory>
#include <string>
#include <vector>
#include "gdcpp/gl_renderer.h"
#include "gdcpp/runtime.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "gdcpp", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "gdcpp", __VA_ARGS__)

namespace {

struct Engine {
  android_app* app = nullptr;
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLSurface surface = EGL_NO_SURFACE;
  EGLContext context = EGL_NO_CONTEXT;
  int w = 0, h = 0;
  bool active = false;
  double last = 0;
  std::unique_ptr<gdcpp::GLRenderer> renderer;
  std::unique_ptr<gdcpp::Game> game;
};

double nowSeconds() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

bool readAsset(AAssetManager* mgr, const std::string& path, std::vector<uint8_t>& out) {
  AAsset* a = AAssetManager_open(mgr, path.c_str(), AASSET_MODE_BUFFER);
  if (!a) return false;
  off_t len = AAsset_getLength(a);
  out.resize((size_t)len);
  int n = AAsset_read(a, out.data(), (size_t)len);
  AAsset_close(a);
  return n == len;
}

bool decodeImage(AAssetManager* mgr, const std::string& file, int& w, int& h, std::vector<uint8_t>& rgba) {
  std::vector<uint8_t> data;
  if (!readAsset(mgr, "game/" + file, data)) return false;
  AImageDecoder* dec = nullptr;
  if (AImageDecoder_createFromBuffer(data.data(), data.size(), &dec) != ANDROID_IMAGE_DECODER_SUCCESS) return false;
  const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(dec);
  w = AImageDecoderHeaderInfo_getWidth(info);
  h = AImageDecoderHeaderInfo_getHeight(info);
  AImageDecoder_setAndroidBitmapFormat(dec, ANDROID_BITMAP_FORMAT_RGBA_8888);
  AImageDecoder_setUnpremultipliedRequired(dec, true);
  size_t stride = AImageDecoder_getMinimumStride(dec);
  rgba.resize(stride * (size_t)h);
  int rc = AImageDecoder_decodeImage(dec, rgba.data(), stride, rgba.size());
  AImageDecoder_delete(dec);
  return rc == ANDROID_IMAGE_DECODER_SUCCESS && stride == (size_t)w * 4;
}

bool initDisplay(Engine& e) {
  e.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize(e.display, nullptr, nullptr);
  const EGLint attribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE};
  EGLConfig config;
  EGLint n = 0;
  if (!eglChooseConfig(e.display, attribs, &config, 1, &n) || n < 1) { LOGE("sem EGLConfig ES3"); return false; }
  e.surface = eglCreateWindowSurface(e.display, config, e.app->window, nullptr);
  const EGLint ctx[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  e.context = eglCreateContext(e.display, config, EGL_NO_CONTEXT, ctx);
  if (e.surface == EGL_NO_SURFACE || e.context == EGL_NO_CONTEXT) return false;
  if (!eglMakeCurrent(e.display, e.surface, e.surface, e.context)) return false;
  eglSwapInterval(e.display, 1);
  eglQuerySurface(e.display, e.surface, EGL_WIDTH, &e.w);
  eglQuerySurface(e.display, e.surface, EGL_HEIGHT, &e.h);
  return true;
}

void termDisplay(Engine& e) {
  if (e.display != EGL_NO_DISPLAY) {
    eglMakeCurrent(e.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (e.context != EGL_NO_CONTEXT) eglDestroyContext(e.display, e.context);
    if (e.surface != EGL_NO_SURFACE) eglDestroySurface(e.display, e.surface);
    eglTerminate(e.display);
  }
  e.display = EGL_NO_DISPLAY; e.context = EGL_NO_CONTEXT; e.surface = EGL_NO_SURFACE;
}

bool createGame(Engine& e) {
  std::vector<uint8_t> data;
  if (!readAsset(e.app->activity->assetManager, "game/project.json", data)) { LOGE("game/project.json não encontrado"); return false; }
  e.game = std::make_unique<gdcpp::Game>();
  e.game->logger = [](const std::string& m) { LOGI("%s", m.c_str()); };
  std::string err;
  if (!e.game->load(std::string(data.begin(), data.end()), err)) { LOGE("projeto inválido: %s", err.c_str()); e.game.reset(); return false; }
  return true;
}

void onCmd(android_app* app, int32_t cmd) {
  Engine& e = *(Engine*)app->userData;
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      if (!app->window || !initDisplay(e)) break;
      {
        AAssetManager* mgr = app->activity->assetManager;
        e.renderer = std::make_unique<gdcpp::GLRenderer>(
            [mgr](const std::string& f, int& w, int& h, std::vector<uint8_t>& px) { return decodeImage(mgr, f, w, h, px); });
        if (!e.renderer->init()) { LOGE("falha ao iniciar o renderer"); break; }
        e.renderer->setSurfaceSize(e.w, e.h);
        bool fresh = !e.game;
        if (fresh && !createGame(e)) break;
        e.game->setRenderer(e.renderer.get());
        e.game->setSurfaceSize(e.w, e.h);
        if (fresh) e.game->start();
        e.last = nowSeconds();
        e.active = true;
      }
      break;
    case APP_CMD_TERM_WINDOW:
      e.active = false;
      if (e.game) e.game->setRenderer(nullptr);
      e.renderer.reset();
      termDisplay(e);
      break;
    case APP_CMD_GAINED_FOCUS: e.last = nowSeconds(); break;
    default: break;
  }
}

const char* keyName(int32_t code) {
  switch (code) {
    case AKEYCODE_DPAD_LEFT: return "Left";
    case AKEYCODE_DPAD_RIGHT: return "Right";
    case AKEYCODE_DPAD_UP: return "Up";
    case AKEYCODE_DPAD_DOWN: return "Down";
    case AKEYCODE_SPACE: return "Space";
    case AKEYCODE_ENTER: return "Return";
    default: return nullptr;
  }
}

int32_t onInput(android_app* app, AInputEvent* ev) {
  Engine& e = *(Engine*)app->userData;
  if (!e.game) return 0;
  if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
    int32_t action = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
    bool down = action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_MOVE ||
                action == AMOTION_EVENT_ACTION_POINTER_DOWN;
    e.game->setPointer(AMotionEvent_getX(ev, 0), AMotionEvent_getY(ev, 0), down);
    return 1;
  }
  if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_KEY) {
    int32_t code = AKeyEvent_getKeyCode(ev);
    int32_t act = AKeyEvent_getAction(ev);
    std::string name;
    if (const char* n = keyName(code)) name = n;
    else if (code >= AKEYCODE_A && code <= AKEYCODE_Z) name = std::string(1, (char)('a' + (code - AKEYCODE_A)));
    if (name.empty()) return 0;  // deixa o sistema tratar BACK etc.
    if (act == AKEY_EVENT_ACTION_DOWN) e.game->input.keys.insert(name);
    else if (act == AKEY_EVENT_ACTION_UP) e.game->input.keys.erase(name);
    return 1;
  }
  return 0;
}

}  // namespace

void android_main(android_app* app) {
  Engine engine;
  engine.app = app;
  app->userData = &engine;
  app->onAppCmd = onCmd;
  app->onInputEvent = onInput;

  while (true) {
    int events;
    android_poll_source* source;
    while (ALooper_pollOnce(engine.active ? 0 : -1, nullptr, &events, (void**)&source) >= 0) {
      if (source) source->process(app, source);
      if (app->destroyRequested) { termDisplay(engine); return; }
    }
    if (!engine.active || !engine.game) continue;

    EGLint w = 0, h = 0;
    eglQuerySurface(engine.display, engine.surface, EGL_WIDTH, &w);
    eglQuerySurface(engine.display, engine.surface, EGL_HEIGHT, &h);
    if (w != engine.w || h != engine.h) {
      engine.w = w; engine.h = h;
      engine.renderer->setSurfaceSize(w, h);
      engine.game->setSurfaceSize(w, h);
    }
    double t = nowSeconds();
    engine.game->step(t - engine.last);
    engine.last = t;
    engine.game->render();
    eglSwapBuffers(engine.display, engine.surface);
    if (engine.game->quit) ANativeActivity_finish(app->activity);
  }
}
