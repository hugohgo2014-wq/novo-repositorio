#pragma once
#include <functional>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "gdcpp/expr.h"
#include "gdcpp/project.h"
#include "gdcpp/renderer.h"

namespace gdcpp {

class Game;

struct RuntimeObject {
  enum Kind { Sprite, Cube3D, Other };
  int id = 0;
  std::string name, image;
  Kind kind = Other;
  double x = 0, y = 0, z = 0, angle = 0, rotX = 0, rotY = 0;
  double w = 0, h = 0, d = 0;
  int zOrder = 0, layerIndex = 0;
  bool deleted = false, visible = true;
  VariablesContainer vars;
};

struct InputState {
  std::unordered_set<std::string> keys;  // "Left","Right","Up","Down","Space","a"...
  bool mouse[3] = {false, false, false}; // Left, Right, Middle (toque = Left)
  double mouseX = 0, mouseY = 0;         // em coordenadas do jogo
};

struct Selection {
  std::unordered_map<std::string, std::vector<RuntimeObject*>> picked;
};

class Scene : public ExprContext {
 public:
  Scene(Game& game, const LayoutDef& layout);
  void step(double dt);
  RuntimeObject* spawn(const std::string& objectName, double x, double y, const std::string& layer);
  std::vector<RuntimeObject*> instancesOf(const std::string& name) const;
  const LayoutDef& layout() const { return layout_; }
  const std::vector<std::unique_ptr<RuntimeObject>>& objects() const { return objects_; }
  Value call(const std::string& fn, const std::vector<Value>& args) override;

  VariablesContainer vars;
  double timeFromStart = 0;
  bool firstFrame = true;
  std::map<std::string, double> timers;

 private:
  RuntimeObject* spawnFrom(const ObjectDef& def, const InstanceDef* inst, double x, double y,
                           const std::string& layer);
  const ObjectDef* findDef(const std::string& name) const;
  int layerIndex(const std::string& name) const;
  std::vector<RuntimeObject*> candidates(const std::string& name, const Selection& sel) const;

  void runEvents(const Json& events, const Selection& parent);
  bool evalConditions(const Json& list, Selection& sel);
  bool evalCondition(const Json& inst, Selection& sel);
  bool conditionImpl(const std::string& id, const std::vector<std::string>& p, Selection& sel);
  void execActions(const Json& list, Selection& sel);
  void execAction(const Json& inst, Selection& sel);
  Value ev(const std::string& expr, const Selection& sel);
  void cleanup();

  Game& game_;
  const LayoutDef& layout_;
  std::vector<std::unique_ptr<RuntimeObject>> objects_;
  std::unordered_map<std::string, std::vector<RuntimeObject*>> byName_;
  std::unordered_map<std::string, const ObjectDef*> defs_;
  const Selection* curSel_ = nullptr;
  double dt_ = 0;
  int nextId_ = 1;
};

class Game {
 public:
  Project project;
  VariablesContainer globals;
  InputState input;
  bool quit = false;
  std::mt19937 rng{12345};
  std::function<void(const std::string&)> logger;

  bool load(const std::string& projectJson, std::string& error);
  void setRenderer(IRenderer* r);  // pode ser nullptr (contexto GL perdido)
  void setSurfaceSize(int w, int h) { surfaceW_ = w; surfaceH_ = h; }
  void setPointer(float screenX, float screenY, bool down);  // toque/mouse em pixels da tela
  void start();
  void step(double dt);
  void render();

  TextureInfo getTexture(const std::string& resourceName);
  void requestScene(const std::string& name) { pending_ = name; }
  void log(const std::string& msg);
  void noteUnsupported(const std::string& id);

  Scene* scene() { return scene_.get(); }
  const std::set<std::string>& unsupported() const { return unsupported_; }
  double elapsed() const { return elapsed_; }

 private:
  IRenderer* renderer_ = nullptr;
  std::unique_ptr<Scene> scene_;
  std::string pending_;
  std::unordered_map<std::string, TextureInfo> textures_;
  std::set<std::string> unsupported_;
  int surfaceW_ = 0, surfaceH_ = 0;
  double elapsed_ = 0;
};

}  // namespace gdcpp
