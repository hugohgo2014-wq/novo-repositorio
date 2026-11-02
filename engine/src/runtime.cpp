#include "gdcpp/runtime.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "gdcpp/variable.h"

namespace gdcpp {

namespace {

bool cmpNum(double a, const std::string& op, double b) {
  if (op == "=" || op == "==") return a == b;
  if (op == "!=") return a != b;
  if (op == "<") return a < b;
  if (op == ">") return a > b;
  if (op == "<=") return a <= b;
  if (op == ">=") return a >= b;
  return false;
}
bool cmpStr(const std::string& a, const std::string& op, const std::string& b) {
  if (op == "=" || op == "==") return a == b;
  if (op == "!=") return a != b;
  return false;
}
double applyOp(double cur, const std::string& op, double v) {
  if (op == "+") return cur + v;
  if (op == "-") return cur - v;
  if (op == "*") return cur * v;
  if (op == "/") return cur / v;
  return v;  // "="
}
bool isYes(const std::string& s) { return s == "yes" || s == "true" || s == "True" || s == "Yes"; }

std::vector<std::string> paramsOf(const Json& inst) {
  std::vector<std::string> p;
  const Json& a = inst["parameters"];
  p.reserve(a.size());
  for (size_t i = 0; i < a.size(); i++) p.push_back(a.at(i).str());
  return p;
}

bool overlap(const RuntimeObject& a, const RuntimeObject& b, bool strict) {
  if (strict) return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
  return a.x <= b.x + b.w && b.x <= a.x + a.w && a.y <= b.y + b.h && b.y <= a.y + a.h;
}

void modifyVar(Variable& v, const std::string& op, const Value& val) {
  if (op == "=") {
    if (val.isString) v.setString(val.str);
    else if (v.type == Variable::Boolean) v.boolean = val.num != 0;
    else v.setNumber(val.num);
  } else if (v.type == Variable::String && op == "+") {
    v.str += toText(val);
  } else {
    v.setNumber(applyOp(v.asNumber(), op, toNumber(val)));
  }
}

bool compareVar(const Variable* v, const std::string& op, const Value& val) {
  if (!v) return cmpNum(0, op, toNumber(val));
  if (v->type == Variable::String || val.isString) return cmpStr(v->asString(), op, toText(val));
  return cmpNum(v->asNumber(), op, toNumber(val));
}

}  // namespace

// ---------------------------------------------------------------- Scene

Scene::Scene(Game& game, const LayoutDef& layout) : game_(game), layout_(layout) {
  vars = layout.vars;
  for (const auto& o : layout.objects) defs_[o.name] = &o;
  for (const auto& o : game.project.globalObjects) defs_.emplace(o.name, &o);
  for (const auto& inst : layout.instances) {
    const ObjectDef* def = findDef(inst.name);
    if (!def) { game_.log("Instância de objeto desconhecido: " + inst.name); continue; }
    spawnFrom(*def, &inst, inst.x, inst.y, inst.layer);
  }
}

const ObjectDef* Scene::findDef(const std::string& name) const {
  auto it = defs_.find(name);
  return it == defs_.end() ? nullptr : it->second;
}

int Scene::layerIndex(const std::string& name) const {
  for (size_t i = 0; i < layout_.layers.size(); i++) if (layout_.layers[i].name == name) return (int)i;
  return 0;
}

RuntimeObject* Scene::spawn(const std::string& objectName, double x, double y, const std::string& layer) {
  const ObjectDef* def = findDef(objectName);
  if (!def) { game_.log("Create: objeto desconhecido " + objectName); return nullptr; }
  return spawnFrom(*def, nullptr, x, y, layer);
}

RuntimeObject* Scene::spawnFrom(const ObjectDef& d, const InstanceDef* inst, double x, double y,
                                const std::string& layer) {
  auto o = std::make_unique<RuntimeObject>();
  o->id = nextId_++;
  o->name = d.name;
  o->image = d.image;
  o->vars = d.vars;
  o->x = x; o->y = y;
  o->layerIndex = layerIndex(layer);
  o->visible = layout_.layers[o->layerIndex].visible;
  if (d.type == "Sprite") {
    o->kind = RuntimeObject::Sprite;
    TextureInfo t = game_.getTexture(d.image);
    o->w = t.w; o->h = t.h;
  } else if (d.type == "Scene3D::Cube3DObject") {
    o->kind = RuntimeObject::Cube3D;
    o->w = d.width; o->h = d.height; o->d = d.depth;
  } else {
    o->kind = RuntimeObject::Other;
    o->w = d.width; o->h = d.height;
  }
  if (inst) {
    o->z = inst->z; o->angle = inst->angle; o->rotX = inst->rotX; o->rotY = inst->rotY;
    o->zOrder = inst->zOrder;
    if (inst->customSize) { o->w = inst->width; o->h = inst->height; if (inst->depth > 0) o->d = inst->depth; }
    for (const auto& kv : inst->vars.vars) o->vars.vars[kv.first] = kv.second;
  }
  RuntimeObject* raw = o.get();
  byName_[raw->name].push_back(raw);
  objects_.push_back(std::move(o));
  return raw;
}

std::vector<RuntimeObject*> Scene::instancesOf(const std::string& name) const {
  std::vector<RuntimeObject*> out;
  auto g = layout_.groups.find(name);
  if (g != layout_.groups.end()) {
    for (const auto& member : g->second) {
      auto it = byName_.find(member);
      if (it != byName_.end()) for (auto* o : it->second) if (!o->deleted) out.push_back(o);
    }
    return out;
  }
  auto it = byName_.find(name);
  if (it != byName_.end()) for (auto* o : it->second) if (!o->deleted) out.push_back(o);
  return out;
}

std::vector<RuntimeObject*> Scene::candidates(const std::string& name, const Selection& sel) const {
  auto it = sel.picked.find(name);
  if (it != sel.picked.end()) {
    std::vector<RuntimeObject*> out;
    for (auto* o : it->second) if (!o->deleted) out.push_back(o);
    return out;
  }
  return instancesOf(name);
}

void Scene::cleanup() {
  bool any = false;
  for (const auto& o : objects_) if (o->deleted) { any = true; break; }
  if (!any) return;
  objects_.erase(std::remove_if(objects_.begin(), objects_.end(),
                                [](const std::unique_ptr<RuntimeObject>& o) { return o->deleted; }),
                 objects_.end());
  byName_.clear();
  for (const auto& o : objects_) byName_[o->name].push_back(o.get());
}

void Scene::step(double dt) {
  dt_ = dt;
  timeFromStart += dt;
  for (auto& t : timers) t.second += dt;
  Selection root;
  runEvents(layout_.events, root);
  firstFrame = false;
  cleanup();
}

// ---------------------------------------------------------------- Expressões

Value Scene::ev(const std::string& expr, const Selection& sel) {
  const Selection* prev = curSel_;
  curSel_ = &sel;
  Value v = evaluate(expr, *this);
  curSel_ = prev;
  return v;
}

Value Scene::call(const std::string& fn, const std::vector<Value>& a) {
  auto num = [&](size_t i) { return i < a.size() ? toNumber(a[i]) : 0.0; };
  auto txt = [&](size_t i) { return i < a.size() ? toText(a[i]) : std::string(); };
  auto N = [](double v) { return Value::number(v); };

  size_t dot = fn.rfind('.');
  if (dot != std::string::npos) {  // Objeto.Funcao()
    std::string objName = fn.substr(0, dot), f = fn.substr(dot + 1);
    RuntimeObject* o = nullptr;
    if (curSel_) {
      auto it = curSel_->picked.find(objName);
      if (it != curSel_->picked.end()) for (auto* c : it->second) if (!c->deleted) { o = c; break; }
    }
    if (!o) { auto l = instancesOf(objName); if (!l.empty()) o = l[0]; }
    if (f == "Variable") {
      const Variable* v = o ? o->vars.find(txt(0)) : nullptr;
      if (!v) return N(0);
      return v->type == Variable::String ? Value::text(v->str) : N(v->asNumber());
    }
    if (!o) return N(0);
    if (f == "X") return N(o->x);
    if (f == "Y") return N(o->y);
    if (f == "Z") return N(o->z);
    if (f == "Angle") return N(o->angle);
    if (f == "Width") return N(o->w);
    if (f == "Height") return N(o->h);
    if (f == "Depth") return N(o->d);
    if (f == "CenterX") return N(o->x + o->w / 2);
    if (f == "CenterY") return N(o->y + o->h / 2);
    if (f == "ZOrder") return N(o->zOrder);
    game_.noteUnsupported("expr:" + f);
    return N(0);
  }

  if (fn == "TimeDelta") return N(dt_);
  if (fn == "TimeFromStart") return N(timeFromStart);
  if (fn == "Random") { double m = std::floor(num(0)); if (m < 0) return N(0); return N((double)std::uniform_int_distribution<long long>(0, (long long)m)(game_.rng)); }
  if (fn == "RandomInRange") { long long lo = (long long)num(0), hi = (long long)num(1); if (hi < lo) std::swap(lo, hi); return N((double)std::uniform_int_distribution<long long>(lo, hi)(game_.rng)); }
  if (fn == "RandomFloat") return N(std::uniform_real_distribution<double>(0, num(0))(game_.rng));
  if (fn == "MouseX" || fn == "CursorX" || fn == "TouchX") return N(game_.input.mouseX);
  if (fn == "MouseY" || fn == "CursorY" || fn == "TouchY") return N(game_.input.mouseY);
  if (fn == "SceneWindowWidth" || fn == "WindowWidth") return N(game_.project.windowWidth);
  if (fn == "SceneWindowHeight" || fn == "WindowHeight") return N(game_.project.windowHeight);
  if (fn == "CurrentSceneName") return Value::text(layout_.name);
  if (fn == "Variable" || fn == "GlobalVariable") {
    const Variable* v = (fn == "Variable" ? vars : game_.globals).find(txt(0));
    if (!v) return N(0);
    return v->type == Variable::String ? Value::text(v->str) : N(v->asNumber());
  }
  if (fn == "abs") return N(std::fabs(num(0)));
  if (fn == "sqrt") return N(std::sqrt(num(0)));
  if (fn == "floor") return N(std::floor(num(0)));
  if (fn == "ceil") return N(std::ceil(num(0)));
  if (fn == "round") return N(std::round(num(0)));
  if (fn == "sin") return N(std::sin(num(0)));
  if (fn == "cos") return N(std::cos(num(0)));
  if (fn == "tan") return N(std::tan(num(0)));
  if (fn == "pow") return N(std::pow(num(0), num(1)));
  if (fn == "exp") return N(std::exp(num(0)));
  if (fn == "log") return N(std::log(num(0)));
  if (fn == "min") return N(std::min(num(0), num(1)));
  if (fn == "max") return N(std::max(num(0), num(1)));
  if (fn == "mod") return N(std::fmod(num(0), num(1)));
  if (fn == "ToRad") return N(num(0) * 3.14159265358979323846 / 180.0);
  if (fn == "ToDeg") return N(num(0) * 180.0 / 3.14159265358979323846);
  if (fn == "ToString") return Value::text(txt(0));
  if (fn == "ToNumber") return N(num(0));
  if (fn == "StrLength") return N((double)txt(0).size());
  game_.noteUnsupported("expr:" + fn);
  return N(0);
}

// ---------------------------------------------------------------- Eventos

void Scene::runEvents(const Json& events, const Selection& parent) {
  for (size_t i = 0; i < events.size(); i++) {
    const Json& e = events.at(i);
    if (e["disabled"].boolean()) continue;
    const std::string type = e["type"].str();
    if (type == "BuiltinCommonInstructions::Standard") {
      Selection sel = parent;
      if (evalConditions(e["conditions"], sel)) {
        execActions(e["actions"], sel);
        runEvents(e["events"], sel);
      }
    } else if (type == "BuiltinCommonInstructions::Group") {
      runEvents(e["events"], parent);
    } else if (type == "BuiltinCommonInstructions::Repeat") {
      long long n = (long long)toNumber(ev(e["repeatExpression"].str(), parent));
      for (long long k = 0; k < n && k < 1000000; k++) {
        Selection sel = parent;
        if (evalConditions(e["conditions"], sel)) {
          execActions(e["actions"], sel);
          runEvents(e["events"], sel);
        }
      }
    } else if (type == "BuiltinCommonInstructions::While") {
      for (int guard = 0; guard < 100000; guard++) {
        Selection sel = parent;
        if (!evalConditions(e["whileConditions"], sel)) break;
        if (evalConditions(e["conditions"], sel)) {
          execActions(e["actions"], sel);
          runEvents(e["events"], sel);
        }
      }
    } else if (type == "BuiltinCommonInstructions::ForEach") {
      std::string name = e["object"].str();
      for (RuntimeObject* o : candidates(name, parent)) {
        if (o->deleted) continue;
        Selection sel = parent;
        sel.picked[name] = {o};
        if (evalConditions(e["conditions"], sel)) {
          execActions(e["actions"], sel);
          runEvents(e["events"], sel);
        }
      }
    } else if (type != "BuiltinCommonInstructions::Comment") {
      game_.noteUnsupported("event:" + type);
    }
  }
}

bool Scene::evalConditions(const Json& list, Selection& sel) {
  for (size_t i = 0; i < list.size(); i++)
    if (!evalCondition(list.at(i), sel)) return false;
  return true;
}

bool Scene::evalCondition(const Json& inst, Selection& sel) {
  const std::string id = inst["type"]["value"].str();
  const bool inverted = inst["type"]["inverted"].boolean();
  Selection tmp = sel;
  bool r = conditionImpl(id, paramsOf(inst), tmp);
  if (inverted) return !r;  // condições invertidas não filtram objetos
  if (r) sel = std::move(tmp);
  return r;
}

bool Scene::conditionImpl(const std::string& id, const std::vector<std::string>& p, Selection& sel) {
  auto P = [&](size_t i) -> const std::string& { static const std::string empty; return i < p.size() ? p[i] : empty; };
  if (id == "BuiltinCommonInstructions::Always") return true;
  if (id == "DepartScene") return firstFrame;
  if (id == "VarScene") return compareVar(vars.find(P(0)), P(1), ev(P(2), sel));
  if (id == "VarGlobal") return compareVar(game_.globals.find(P(0)), P(1), ev(P(2), sel));
  if (id == "BuiltinCommonInstructions::CompareNumbers")
    return cmpNum(toNumber(ev(P(0), sel)), P(1), toNumber(ev(P(2), sel)));
  if (id == "BuiltinCommonInstructions::CompareStrings")
    return cmpStr(toText(ev(P(0), sel)), P(1), toText(ev(P(2), sel)));
  if (id == "KeyPressed") return game_.input.keys.count(P(1)) > 0;
  if (id == "MouseButtonPressed") {
    const std::string& b = P(1);
    int idx = b == "Right" ? 1 : b == "Middle" ? 2 : 0;
    return game_.input.mouse[idx];
  }
  if (id == "Timer") {
    auto it = timers.find(toText(ev(P(2), sel)));
    if (it == timers.end()) { timers[toText(ev(P(2), sel))] = 0; return false; }
    return it->second >= toNumber(ev(P(1), sel));
  }
  if (id == "CollisionNP") {
    const std::string &n1 = P(0), &n2 = P(1);
    const bool strict = isYes(P(2));
    std::vector<RuntimeObject*> A = candidates(n1, sel), B = candidates(n2, sel);
    std::vector<RuntimeObject*> pa, pb;
    for (auto* a : A) {
      bool hit = false;
      for (auto* b : B) {
        if (a == b || !overlap(*a, *b, strict)) continue;
        hit = true;
        if (std::find(pb.begin(), pb.end(), b) == pb.end()) pb.push_back(b);
      }
      if (hit) pa.push_back(a);
    }
    if (pa.empty()) return false;
    sel.picked[n1] = pa;
    sel.picked[n2] = pb;
    return true;
  }
  game_.noteUnsupported("cond:" + id);
  return false;
}

void Scene::execActions(const Json& list, Selection& sel) {
  for (size_t i = 0; i < list.size(); i++) execAction(list.at(i), sel);
}

void Scene::execAction(const Json& inst, Selection& sel) {
  const std::string id = inst["type"]["value"].str();
  const std::vector<std::string> p = paramsOf(inst);
  auto P = [&](size_t i) -> const std::string& { static const std::string empty; return i < p.size() ? p[i] : empty; };
  auto E = [&](size_t i) { return ev(P(i), sel); };

  if (id == "ModVarScene") { modifyVar(vars.get(P(0)), P(1), E(2)); return; }
  if (id == "ModVarGlobal") { modifyVar(game_.globals.get(P(0)), P(1), E(2)); return; }
  if (id == "SetX" || id == "MettreX") { double v = toNumber(E(2)); for (auto* o : candidates(P(0), sel)) o->x = applyOp(o->x, P(1), v); return; }
  if (id == "SetY" || id == "MettreY") { double v = toNumber(E(2)); for (auto* o : candidates(P(0), sel)) o->y = applyOp(o->y, P(1), v); return; }
  if (id == "SetXY" || id == "MettreXY") {
    double vx = toNumber(E(2)), vy = toNumber(E(3));
    for (auto* o : candidates(P(0), sel)) { o->x = applyOp(o->x, P(1), vx); o->y = applyOp(o->y, P(1), vy); }
    return;
  }
  if (id == "SetAngle") { double v = toNumber(E(2)); for (auto* o : candidates(P(0), sel)) o->angle = applyOp(o->angle, P(1), v); return; }
  if (id == "Rotate") { double v = toNumber(E(1)) * dt_; for (auto* o : candidates(P(0), sel)) o->angle += v; return; }
  if (id == "SetNumberObjectVariable") {
    Value v = E(3);
    for (auto* o : candidates(P(0), sel)) modifyVar(o->vars.get(P(1)), P(2), v);
    return;
  }
  if (id == "Delete") { for (auto* o : candidates(P(0), sel)) o->deleted = true; return; }
  if (id == "Create") {
    double x = toNumber(E(2)), y = toNumber(E(3));
    RuntimeObject* o = spawn(P(1), x, y, toText(E(4)));
    if (o) sel.picked[P(1)] = {o};
    return;
  }
  if (id == "ResetTimer") { timers[toText(E(1))] = 0; return; }
  if (id == "Scene") { game_.requestScene(toText(E(1))); return; }
  if (id == "Quit") { game_.quit = true; return; }
  game_.noteUnsupported("action:" + id);
}

// ---------------------------------------------------------------- Game

bool Game::load(const std::string& json, std::string& error) {
  if (!project.load(json, error)) return false;
  globals = project.globalVars;
  return true;
}

void Game::setRenderer(IRenderer* r) {
  renderer_ = r;
  textures_.clear();  // ids antigos pertencem ao contexto GL anterior
}

void Game::log(const std::string& msg) {
  if (logger) logger(msg);
  else std::fprintf(stderr, "[gdcpp] %s\n", msg.c_str());
}

void Game::noteUnsupported(const std::string& id) {
  if (unsupported_.insert(id).second) log("Instrução ainda não suportada: " + id);
}

TextureInfo Game::getTexture(const std::string& name) {
  if (!renderer_ || name.empty()) return TextureInfo();
  auto it = textures_.find(name);
  if (it != textures_.end()) return it->second;
  TextureInfo t;
  auto res = project.resources.find(name);
  t = renderer_->loadTexture(res == project.resources.end() ? name : res->second);
  if (t.id < 0) log("Falha ao carregar textura: " + name);
  textures_[name] = t;
  return t;
}

void Game::start() {
  int idx = project.findLayout(project.firstLayout);
  if (idx < 0) idx = 0;
  scene_ = std::make_unique<Scene>(*this, project.layouts[idx]);
}

void Game::step(double dt) {
  if (!scene_) start();
  if (dt < 0) dt = 0;
  if (dt > 0.1) dt = 0.1;  // evita saltos grandes após pausa
  elapsed_ += dt;
  scene_->step(dt);
  if (!pending_.empty()) {
    int idx = project.findLayout(pending_);
    if (idx >= 0) scene_ = std::make_unique<Scene>(*this, project.layouts[idx]);
    else log("Cena não encontrada: " + pending_);
    pending_.clear();
  }
}

void Game::setPointer(float sx, float sy, bool down) {
  Viewport v = fitViewport(surfaceW_, surfaceH_, project.windowWidth, project.windowHeight);
  if (v.scale > 0) {
    input.mouseX = (sx - v.x) / v.scale;
    input.mouseY = (sy - v.y) / v.scale;
  }
  input.mouse[0] = down;
}

void Game::render() {
  if (!renderer_ || !scene_) return;
  const LayoutDef& L = scene_->layout();
  renderer_->beginFrame(project.windowWidth, project.windowHeight, L.bgR / 255.f, L.bgG / 255.f, L.bgB / 255.f);
  std::vector<RuntimeObject*> cubes, sprites;
  for (const auto& o : scene_->objects()) {
    if (o->deleted || !o->visible) continue;
    if (o->kind == RuntimeObject::Cube3D) cubes.push_back(o.get());
    else if (o->kind == RuntimeObject::Sprite) sprites.push_back(o.get());
  }
  auto order = [](const RuntimeObject* a, const RuntimeObject* b) {
    if (a->layerIndex != b->layerIndex) return a->layerIndex < b->layerIndex;
    if (a->zOrder != b->zOrder) return a->zOrder < b->zOrder;
    return a->id < b->id;
  };
  std::sort(cubes.begin(), cubes.end(), order);
  std::sort(sprites.begin(), sprites.end(), order);
  for (auto* o : cubes) {  // opacos primeiro (escrevem profundidade)
    CubeDraw c;
    c.tex = getTexture(o->image).id;
    c.x = (float)o->x; c.y = (float)o->y; c.z = (float)o->z;
    c.w = (float)o->w; c.h = (float)o->h; c.d = (float)o->d;
    c.rotX = (float)o->rotX; c.rotY = (float)o->rotY; c.rotZ = (float)o->angle;
    c.r = c.g = 0.85f; c.b = 0.9f;
    renderer_->drawCube(c);
  }
  for (auto* o : sprites) {
    SpriteDraw s;
    s.tex = getTexture(o->image).id;
    s.x = (float)o->x; s.y = (float)o->y; s.z = (float)o->z;
    s.w = (float)o->w; s.h = (float)o->h; s.angle = (float)o->angle;
    if (s.tex < 0) { s.r = 0.8f; s.g = 0.2f; s.b = 0.8f; } else { s.r = s.g = s.b = 1.f; }
    s.a = 1.f;
    renderer_->drawSprite(s);
  }
  renderer_->endFrame();
}

}  // namespace gdcpp
