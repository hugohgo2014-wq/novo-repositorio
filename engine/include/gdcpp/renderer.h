#pragma once
#include <algorithm>
#include <string>

namespace gdcpp {

struct TextureInfo {
  int id = -1;
  int w = 0, h = 0;
};

struct SpriteDraw {
  int tex = -1;
  float x, y, z, w, h, angle;  // x,y = canto superior esquerdo (coordenadas do jogo)
  float r, g, b, a;
};

struct CubeDraw {
  int tex = -1;
  float x, y, z, w, h, d;      // canto de menor coordenada + dimensões
  float rotX, rotY, rotZ;      // graus
  float r, g, b;
};

// Abstração de renderização: OpenGL ES no Android, Null nos testes.
class IRenderer {
 public:
  virtual ~IRenderer() {}
  virtual TextureInfo loadTexture(const std::string& file) = 0;
  virtual void beginFrame(int gameW, int gameH, float r, float g, float b) = 0;
  virtual void drawSprite(const SpriteDraw& d) = 0;
  virtual void drawCube(const CubeDraw& d) = 0;
  virtual void endFrame() = 0;
};

// Renderer de teste: não desenha, apenas conta.
class NullRenderer : public IRenderer {
 public:
  int sprites = 0, cubes = 0, frames = 0, textures = 0;
  int texW = 64, texH = 64;
  TextureInfo loadTexture(const std::string&) override {
    TextureInfo t; t.id = textures++; t.w = texW; t.h = texH; return t;
  }
  void beginFrame(int, int, float, float, float) override { sprites = cubes = 0; frames++; }
  void drawSprite(const SpriteDraw&) override { sprites++; }
  void drawCube(const CubeDraw&) override { cubes++; }
  void endFrame() override {}
};

// Letterbox: encaixa o jogo (gw x gh) na tela (sw x sh) mantendo a proporção.
struct Viewport {
  int x = 0, y = 0, w = 0, h = 0;
  float scale = 1.f;
};
inline Viewport fitViewport(int sw, int sh, int gw, int gh) {
  Viewport v;
  if (sw <= 0 || sh <= 0 || gw <= 0 || gh <= 0) return v;
  v.scale = std::min(sw / (float)gw, sh / (float)gh);
  v.w = (int)(gw * v.scale);
  v.h = (int)(gh * v.scale);
  v.x = (sw - v.w) / 2;
  v.y = (sh - v.h) / 2;
  return v;
}

}  // namespace gdcpp
