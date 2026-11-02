#ifdef GDCPP_GLES
#include "gdcpp/gl_renderer.h"
#include <GLES3/gl3.h>
#include <android/log.h>
#include <cstring>
#include "gdcpp/mat4.h"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "gdcpp", __VA_ARGS__)

namespace gdcpp {

static const char* kVS = R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUv;
uniform mat4 uMVP;
uniform mat4 uRot;
out vec2 vUv;
out vec3 vN;
void main() {
  vUv = aUv;
  vN = mat3(uRot) * aNormal;
  gl_Position = uMVP * vec4(aPos, 1.0);
})";

static const char* kFS = R"(#version 300 es
precision mediump float;
in vec2 vUv;
in vec3 vN;
uniform sampler2D uTex;
uniform vec4 uColor;
uniform int uUseTex;
uniform int uLit;
out vec4 oColor;
void main() {
  vec4 c = uColor;
  if (uUseTex == 1) c *= texture(uTex, vUv);
  if (uLit == 1) {
    vec3 L = normalize(vec3(-0.4, -0.6, 0.7));
    float d = max(dot(normalize(vN), L), 0.0);
    c.rgb *= 0.55 + 0.45 * d;
  }
  oColor = c;
})";

static unsigned compile(GLenum type, const char* src) {
  unsigned s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  int ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetShaderInfoLog(s, sizeof log, nullptr, log);
    LOGE("shader: %s", log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

static void setupVao(unsigned& vao, unsigned& vbo, const std::vector<float>& data) {
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
  const GLsizei stride = 8 * sizeof(float);  // pos(3) normal(3) uv(2)
  glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
  glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
  glBindVertexArray(0);
}

bool GLRenderer::init() {
  unsigned vs = compile(GL_VERTEX_SHADER, kVS), fs = compile(GL_FRAGMENT_SHADER, kFS);
  if (!vs || !fs) return false;
  program_ = glCreateProgram();
  glAttachShader(program_, vs);
  glAttachShader(program_, fs);
  glLinkProgram(program_);
  glDeleteShader(vs);
  glDeleteShader(fs);
  int ok = 0;
  glGetProgramiv(program_, GL_LINK_STATUS, &ok);
  if (!ok) { LOGE("falha ao linkar o programa GL"); return false; }
  uMVP_ = glGetUniformLocation(program_, "uMVP");
  uRot_ = glGetUniformLocation(program_, "uRot");
  uColor_ = glGetUniformLocation(program_, "uColor");
  uTex_ = glGetUniformLocation(program_, "uTex");
  uUseTex_ = glGetUniformLocation(program_, "uUseTex");
  uLit_ = glGetUniformLocation(program_, "uLit");

  // Quad centrado na origem (y para baixo: topo = -0.5), normal +Z.
  std::vector<float> quad = {
      -0.5f, -0.5f, 0, 0, 0, 1, 0, 0,  0.5f, -0.5f, 0, 0, 0, 1, 1, 0,  0.5f, 0.5f, 0, 0, 0, 1, 1, 1,
      -0.5f, -0.5f, 0, 0, 0, 1, 0, 0,  0.5f, 0.5f, 0, 0, 0, 1, 1, 1,  -0.5f, 0.5f, 0, 0, 0, 1, 0, 1};
  setupVao(quadVao_, quadVbo_, quad);

  // Cubo unitário: 6 faces, 2 triângulos cada.
  const float faces[6][9] = {  // normal, eixo u, eixo v
      {0, 0, 1, 1, 0, 0, 0, 1, 0},  {0, 0, -1, -1, 0, 0, 0, 1, 0}, {1, 0, 0, 0, 0, -1, 0, 1, 0},
      {-1, 0, 0, 0, 0, 1, 0, 1, 0}, {0, 1, 0, 1, 0, 0, 0, 0, -1},  {0, -1, 0, 1, 0, 0, 0, 0, 1}};
  const int cu[4] = {-1, 1, 1, -1}, cv[4] = {-1, -1, 1, 1};
  const int tri[6] = {0, 1, 2, 0, 2, 3};
  std::vector<float> cube;
  for (const auto& f : faces)
    for (int t : tri) {
      for (int k = 0; k < 3; k++) cube.push_back(0.5f * (f[k] + cu[t] * f[3 + k] + cv[t] * f[6 + k]));
      for (int k = 0; k < 3; k++) cube.push_back(f[k]);
      cube.push_back((cu[t] + 1) * 0.5f);
      cube.push_back((cv[t] + 1) * 0.5f);
    }
  setupVao(cubeVao_, cubeVbo_, cube);
  return true;
}

GLRenderer::~GLRenderer() {
  if (!textures_.empty()) glDeleteTextures((GLsizei)textures_.size(), textures_.data());
  if (quadVbo_) glDeleteBuffers(1, &quadVbo_);
  if (cubeVbo_) glDeleteBuffers(1, &cubeVbo_);
  if (quadVao_) glDeleteVertexArrays(1, &quadVao_);
  if (cubeVao_) glDeleteVertexArrays(1, &cubeVao_);
  if (program_) glDeleteProgram(program_);
}

TextureInfo GLRenderer::loadTexture(const std::string& file) {
  TextureInfo info;
  int w = 0, h = 0;
  std::vector<uint8_t> rgba;
  if (!decoder_ || !decoder_(file, w, h, rgba) || w <= 0 || h <= 0) { LOGE("textura não carregada: %s", file.c_str()); return info; }
  unsigned tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  textures_.push_back(tex);
  info.id = (int)tex;
  info.w = w; info.h = h;
  return info;
}

void GLRenderer::beginFrame(int gw, int gh, float r, float g, float b) {
  Viewport v = fitViewport(surfaceW_, surfaceH_, gw, gh);
  glDisable(GL_SCISSOR_TEST);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);  // barras do letterbox
  glViewport(v.x, v.y, v.w, v.h);
  glEnable(GL_SCISSOR_TEST);
  glScissor(v.x, v.y, v.w, v.h);
  glClearColor(r, g, b, 1);
  glDepthMask(GL_TRUE);
  glClearDepthf(1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDisable(GL_CULL_FACE);  // a inversão de Y da câmera troca o sentido dos triângulos
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  Mat4 vp = gameViewProj((float)gw, (float)gh);
  std::memcpy(viewProj_, vp.m, sizeof viewProj_);
  glUseProgram(program_);
  glUniform1i(uTex_, 0);
}

void GLRenderer::drawSprite(const SpriteDraw& d) {
  Mat4 vp; std::memcpy(vp.m, viewProj_, sizeof vp.m);
  Mat4 model = mul(translate(d.x + d.w / 2, d.y + d.h / 2, d.z), mul(rotZ(d.angle), scale(d.w, d.h, 1)));
  Mat4 mvp = mul(vp, model);
  Mat4 id = identity();
  glDepthMask(GL_FALSE);
  glUniformMatrix4fv(uMVP_, 1, GL_FALSE, mvp.m);
  glUniformMatrix4fv(uRot_, 1, GL_FALSE, id.m);
  glUniform4f(uColor_, d.r, d.g, d.b, d.a);
  glUniform1i(uUseTex_, d.tex >= 0 ? 1 : 0);
  glUniform1i(uLit_, 0);
  if (d.tex >= 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, (unsigned)d.tex); }
  glBindVertexArray(quadVao_);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

void GLRenderer::drawCube(const CubeDraw& d) {
  Mat4 vp; std::memcpy(vp.m, viewProj_, sizeof vp.m);
  Mat4 rot = mul(rotZ(d.rotZ), mul(rotY(d.rotY), rotX(d.rotX)));
  Mat4 model = mul(translate(d.x + d.w / 2, d.y + d.h / 2, d.z + d.d / 2), mul(rot, scale(d.w, d.h, d.d)));
  Mat4 mvp = mul(vp, model);
  glDepthMask(GL_TRUE);
  glUniformMatrix4fv(uMVP_, 1, GL_FALSE, mvp.m);
  glUniformMatrix4fv(uRot_, 1, GL_FALSE, rot.m);
  glUniform4f(uColor_, d.r, d.g, d.b, 1.f);
  glUniform1i(uUseTex_, d.tex >= 0 ? 1 : 0);
  glUniform1i(uLit_, 1);
  if (d.tex >= 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, (unsigned)d.tex); }
  glBindVertexArray(cubeVao_);
  glDrawArrays(GL_TRIANGLES, 0, 36);
}

}  // namespace gdcpp
#endif
