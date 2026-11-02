#include "gdcpp/project.h"
#include <exception>

namespace gdcpp {

static void loadObject(const Json& j, ObjectDef& o) {
  o.name = j["name"].str();
  o.type = j["type"].str();
  o.vars.loadFromJson(j["variables"]);
  if (o.type == "Sprite") {
    const Json& dirs = j["animations"].at(0)["directions"];
    o.image = dirs.at(0)["sprites"].at(0)["image"].str();
  } else if (o.type == "Scene3D::Cube3DObject") {
    const Json& c = j["content"];
    o.width = c["width"].num(100);
    o.height = c["height"].num(100);
    o.depth = c["depth"].num(100);
    o.image = c["frontFaceResourceName"].str();
  }
}

bool Project::load(const std::string& text, std::string& error) {
  try {
    Json j = Json::parse(text);
    const Json& props = j["properties"];
    name = props["name"].str(j["name"].str("Game"));
    windowWidth = (int)props["windowWidth"].num(j["windowWidth"].num(800));
    windowHeight = (int)props["windowHeight"].num(j["windowHeight"].num(600));
    gdVersion = j["gdVersion"].type == Json::Object
                    ? std::to_string((int)j["gdVersion"]["major"].num()) + "." + std::to_string((int)j["gdVersion"]["minor"].num())
                    : "";
    firstLayout = j["firstLayout"].str();
    globalVars.loadFromJson(j["variables"]);

    const Json& res = j["resources"]["resources"];
    for (size_t i = 0; i < res.size(); i++) resources[res.at(i)["name"].str()] = res.at(i)["file"].str();

    const Json& gobjs = j["objects"];
    for (size_t i = 0; i < gobjs.size(); i++) { globalObjects.emplace_back(); loadObject(gobjs.at(i), globalObjects.back()); }

    const Json& lays = j["layouts"];
    for (size_t i = 0; i < lays.size(); i++) {
      const Json& l = lays.at(i);
      layouts.emplace_back();
      LayoutDef& L = layouts.back();
      L.name = l["name"].str();
      L.bgR = (int)l["r"].num(209); L.bgG = (int)l["g"].num(209); L.bgB = (int)l["b"].num(209);
      L.vars.loadFromJson(l["variables"]);
      L.events = l["events"];
      const Json& ls = l["layers"];
      for (size_t k = 0; k < ls.size(); k++) {
        LayerDef d; d.name = ls.at(k)["name"].str(); d.visible = ls.at(k)["visibility"].boolean(true);
        L.layers.push_back(d);
      }
      if (L.layers.empty()) L.layers.push_back(LayerDef());
      const Json& objs = l["objects"];
      for (size_t k = 0; k < objs.size(); k++) { L.objects.emplace_back(); loadObject(objs.at(k), L.objects.back()); }
      const Json& groups = l["objectsGroups"];
      for (size_t k = 0; k < groups.size(); k++) {
        std::vector<std::string> members;
        const Json& m = groups.at(k)["objects"];
        for (size_t q = 0; q < m.size(); q++) members.push_back(m.at(q)["name"].str());
        L.groups[groups.at(k)["name"].str()] = members;
      }
      const Json& ins = l["instances"];
      for (size_t k = 0; k < ins.size(); k++) {
        const Json& n = ins.at(k);
        InstanceDef d;
        d.name = n["name"].str(); d.layer = n["layer"].str();
        d.x = n["x"].num(); d.y = n["y"].num(); d.z = n["z"].num();
        d.angle = n["angle"].num(); d.rotX = n["rotationX"].num(); d.rotY = n["rotationY"].num();
        d.zOrder = (int)n["zOrder"].num();
        d.customSize = n["customSize"].boolean();
        d.width = n["width"].num(); d.height = n["height"].num(); d.depth = n["depth"].num();
        d.vars.loadFromJson(n["initialVariables"]);
        L.instances.push_back(std::move(d));
      }
    }
    if (layouts.empty()) { error = "o projeto não tem cenas"; return false; }
    return true;
  } catch (const std::exception& e) {
    error = e.what();
    return false;
  }
}

int Project::findLayout(const std::string& n) const {
  for (size_t i = 0; i < layouts.size(); i++) if (layouts[i].name == n) return (int)i;
  return -1;
}

}  // namespace gdcpp
