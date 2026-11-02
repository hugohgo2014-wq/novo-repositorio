#pragma once
#ifdef GDCPP_GLES
#include <cstdint>
#include <functional>
#include <vector>
#include "gdcpp/renderer.h"

namespace gdcpp {

// Renderer OpenGL ES 3.0. Câmera em perspectiva alinhada ao plano 2D (z=0), como no GDevelop.
class GLRenderer : public IRenderer {
 public:
  using ImageDecoder = std::function<bool(const std::string& file, int& w, int& h, std::vector<uint8_t>& rgba)>;
  explicit GLRenderer(ImageDecoder decoder) : decoder_(std::move(decoder)) {}
  ~GLRenderer() override;

  bool init();  // requer contexto GL atual
  void setSurfaceSize(int w, int h) { surfaceW_ = w; surfaceH_ = h; }

  TextureInfo loadTexture(const std::string& file) override;
  void beginFrame(int gameW, int gameH, float r, float g, float b) override;
  void drawSprite(const SpriteDraw& d) override;
  void drawCube(const CubeDraw& d) override;
  void endFrame() override {}

 private:
  ImageDecoder decoder_;
  unsigned program_ = 0, quadVao_ = 0, quadVbo_ = 0, cubeVao_ = 0, cubeVbo_ = 0;
  int uMVP_ = -1, uRot_ = -1, uColor_ = -1, uTex_ = -1, uUseTex_ = -1, uLit_ = -1;
  std::vector<unsigned> textures_;
  float viewProj_[16];
  int surfaceW_ = 0, surfaceH_ = 0;
};

}  // namespace gdcpp
#endif
