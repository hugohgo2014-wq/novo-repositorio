#pragma once
#include <map>
#include <string>
#include <vector>
#include "gdcpp/json.h"
#include "gdcpp/variable.h"

namespace gdcpp {

struct ObjectDef {
  std::string name, type;
  VariablesContainer vars;
  double width = 0, height = 0, depth = 0;
  std::string image;  // nome do recurso (1º frame do Sprite / face do Cube3D)
};

struct InstanceDef {
  std::string name, layer;
  double x = 0, y = 0, z = 0, angle = 0, rotX = 0, rotY = 0;
  double width = 0, height = 0, depth = 0;
  int zOrder = 0;
  bool customSize = false;
  VariablesContainer vars;
};

struct LayerDef {
  std::string name;
  bool visible = true;
};

struct LayoutDef {
  std::string name;
  int bgR = 209, bgG = 209, bgB = 209;
  std::vector<ObjectDef> objects;
  std::vector<InstanceDef> instances;
  std::vector<LayerDef> layers;
  std::map<std::string, std::vector<std::string>> groups;
  VariablesContainer vars;
  Json events;
};

struct Project {
  std::string name, firstLayout, gdVersion;
  int windowWidth = 800, windowHeight = 600;
  VariablesContainer globalVars;
  std::vector<ObjectDef> globalObjects;
  std::vector<LayoutDef> layouts;
  std::map<std::string, std::string> resources;  // nome -> arquivo

  bool load(const std::string& jsonText, std::string& error);
  int findLayout(const std::string& name) const;
};

}  // namespace gdcpp
