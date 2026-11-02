#pragma once
#include <cmath>

namespace gdcpp {

// Matrizes 4x4 column-major (layout do OpenGL): elemento (linha r, coluna c) = m[c*4+r].
struct Mat4 {
  float m[16];
};

inline Mat4 identity() {
  Mat4 r = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  return r;
}
inline Mat4 mul(const Mat4& a, const Mat4& b) {
  Mat4 o;
  for (int c = 0; c < 4; c++)
    for (int r = 0; r < 4; r++) {
      float s = 0;
      for (int k = 0; k < 4; k++) s += a.m[k * 4 + r] * b.m[c * 4 + k];
      o.m[c * 4 + r] = s;
    }
  return o;
}
inline Mat4 translate(float x, float y, float z) {
  Mat4 r = identity();
  r.m[12] = x; r.m[13] = y; r.m[14] = z;
  return r;
}
inline Mat4 scale(float x, float y, float z) {
  Mat4 r = identity();
  r.m[0] = x; r.m[5] = y; r.m[10] = z;
  return r;
}
inline Mat4 rotZ(float deg) {
  float a = deg * 3.14159265358979f / 180.f, c = std::cos(a), s = std::sin(a);
  Mat4 r = identity();
  r.m[0] = c; r.m[4] = -s; r.m[1] = s; r.m[5] = c;
  return r;
}
inline Mat4 rotY(float deg) {
  float a = deg * 3.14159265358979f / 180.f, c = std::cos(a), s = std::sin(a);
  Mat4 r = identity();
  r.m[0] = c; r.m[8] = s; r.m[2] = -s; r.m[10] = c;
  return r;
}
inline Mat4 rotX(float deg) {
  float a = deg * 3.14159265358979f / 180.f, c = std::cos(a), s = std::sin(a);
  Mat4 r = identity();
  r.m[5] = c; r.m[9] = -s; r.m[6] = s; r.m[10] = c;
  return r;
}
inline Mat4 perspective(float fovyRad, float aspect, float zn, float zf) {
  float f = 1.f / std::tan(fovyRad / 2.f);
  Mat4 r = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
  r.m[0] = f / aspect; r.m[5] = f;
  r.m[10] = (zf + zn) / (zn - zf); r.m[11] = -1.f;
  r.m[14] = 2.f * zf * zn / (zn - zf);
  return r;
}

// Câmera padrão do GDevelop: o plano z=0 coincide exatamente com as coordenadas 2D do jogo
// (x para a direita, y para baixo, z positivo vem na direção da câmera). FOV vertical = 45°.
inline Mat4 gameViewProj(float w, float h) {
  const float fov = 45.f * 3.14159265358979f / 180.f;
  float camZ = (h / 2.f) / std::tan(fov / 2.f);
  Mat4 view = identity();
  view.m[5] = -1.f;
  view.m[12] = -w / 2.f; view.m[13] = h / 2.f; view.m[14] = -camZ;
  return mul(perspective(fov, w / h, camZ * 0.02f, camZ + 100000.f), view);
}

inline void transformPoint(const Mat4& m, float x, float y, float z, float out[3]) {
  float v[4];
  for (int r = 0; r < 4; r++) v[r] = m.m[r] * x + m.m[4 + r] * y + m.m[8 + r] * z + m.m[12 + r];
  out[0] = v[0] / v[3]; out[1] = v[1] / v[3]; out[2] = v[2] / v[3];
}

}  // namespace gdcpp
