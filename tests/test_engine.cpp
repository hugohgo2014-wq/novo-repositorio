#include <cstdio>
#include <fstream>
#include <sstream>
#include "gdcpp/expr.h"
#include "gdcpp/json.h"
#include "gdcpp/mat4.h"
#include "gdcpp/runtime.h"

using namespace gdcpp;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FALHOU %s:%d  %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
static bool near(double a, double b, double eps = 1e-3) { return std::fabs(a - b) < eps; }

struct StubCtx : ExprContext {
  Value call(const std::string& fn, const std::vector<Value>& a) override {
    if (fn == "TimeDelta") return Value::number(0.5);
    if (fn == "max") return Value::number(std::max(toNumber(a[0]), toNumber(a[1])));
    if (fn == "Variable") return Value::text(toText(a[0]));
    return Value::number(0);
  }
};

static void testJson() {
  Json j = Json::parse("{\"a\":[1,2,{\"b\":\"x\\u00e9\\n\"}],\"t\":true,\"n\":null}");
  CHECK(j["a"].size() == 3);
  CHECK(j["a"].at(2)["b"].str() == "x\xC3\xA9\n");
  CHECK(j["t"].boolean());
  CHECK(j["n"].isNull());
  CHECK(j["missing"]["deep"].num(7) == 7);
  bool threw = false;
  try { Json::parse("{\"a\":"); } catch (...) { threw = true; }
  CHECK(threw);
}

static void testExpr() {
  StubCtx c;
  CHECK(near(toNumber(evaluate("1+2*3", c)), 7));
  CHECK(near(toNumber(evaluate("(1+2)*3-4/2", c)), 7));
  CHECK(near(toNumber(evaluate("-TimeDelta()*10", c)), -5));
  CHECK(near(toNumber(evaluate("max(3, 9)", c)), 9));
  CHECK(toText(evaluate("\"pontos: \" + 3", c)) == "pontos: 3");
  CHECK(toText(evaluate("Variable(score)", c)) == "score");
  CHECK(near(toNumber(evaluate("1 +", c)), 0));  // erro de sintaxe => 0
}

static void testMath() {
  Mat4 vp = gameViewProj(1280, 720);
  float p[3];
  transformPoint(vp, 0, 0, 0, p);        CHECK(near(p[0], -1) && near(p[1], 1));   // canto superior esquerdo
  transformPoint(vp, 1280, 720, 0, p);   CHECK(near(p[0], 1) && near(p[1], -1));   // canto inferior direito
  transformPoint(vp, 640, 360, 0, p);    CHECK(near(p[0], 0) && near(p[1], 0));
  Viewport v = fitViewport(2400, 1080, 1280, 720);
  CHECK(v.h == 1080 && v.w == 1920 && v.x == 240 && v.y == 0);
}

static std::string readFile(const char* path) {
  std::ifstream f(path, std::ios::binary);
  std::stringstream ss; ss << f.rdbuf();
  return ss.str();
}

static void testGame(const char* projectPath) {
  Game g;
  g.logger = [](const std::string& m) { std::printf("  log: %s\n", m.c_str()); };
  std::string err;
  CHECK(g.load(readFile(projectPath), err));
  CHECK(g.project.windowWidth == 1280 && g.project.layouts.size() == 2);
  NullRenderer r;
  g.setRenderer(&r);
  g.setSurfaceSize(1280, 720);
  g.start();

  g.step(1.0 / 60); g.render();
  CHECK(g.scene()->layout().name == "Level1");
  CHECK(r.sprites == 3 && r.cubes == 1);                      // Player + 2 moedas, 1 cubo 3D
  CHECK(g.scene()->instancesOf("Player")[0]->w == 64);         // tamanho vem da textura

  g.input.keys.insert("Right");
  double x0 = g.scene()->instancesOf("Player")[0]->x;
  g.step(0.1);
  CHECK(near(g.scene()->instancesOf("Player")[0]->x - x0, 40));  // 400 px/s * 0,1 s
  g.input.keys.clear();

  g.setPointer(632, 332, true);                                // toque em cima da moeda 1
  g.step(1.0 / 60);
  CHECK(near(g.scene()->vars.find("score")->asNumber(), 1));
  CHECK(near(g.globals.find("totalCoins")->asNumber(), 1));
  CHECK(g.scene()->instancesOf("Coin").size() == 1);

  g.setPointer(932, 232, true);                                // moeda 2 => vitória
  g.step(1.0 / 60);
  g.step(1.0 / 60);
  CHECK(g.scene()->layout().name == "Win");
  g.setPointer(0, 0, false);

  for (int i = 0; i < 40; i++) g.step(0.1);                    // (dt é limitado a 0,1 s) timer de 3 s volta ao Level1
  CHECK(g.scene()->layout().name == "Level1");
  CHECK(near(g.scene()->vars.find("score")->asNumber(), 0));   // cena recriada
  CHECK(near(g.globals.find("totalCoins")->asNumber(), 2));    // global persiste
  CHECK(g.unsupported().empty());
}

int main(int argc, char** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  testJson();
  testExpr();
  testMath();
  if (argc > 1) testGame(argv[1]); else { std::printf("Informe o caminho do project.json\n"); failures++; }
  std::printf(failures ? "%d FALHA(S)\n" : "todos os testes passaram\n", failures);
  return failures ? 1 : 0;
}
