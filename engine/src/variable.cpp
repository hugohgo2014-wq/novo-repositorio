#include "gdcpp/variable.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gdcpp {

std::string formatNumber(double v) {
  if (std::isnan(v)) return "NaN";
  if (std::isinf(v)) return v > 0 ? "Infinity" : "-Infinity";
  char buf[64];
  if (v == std::floor(v) && std::fabs(v) < 1e15) std::snprintf(buf, sizeof buf, "%.0f", v);
  else std::snprintf(buf, sizeof buf, "%.15g", v);
  return buf;
}

double Variable::asNumber() const {
  switch (type) {
    case Number: return num;
    case Boolean: return boolean ? 1 : 0;
    case String: {
      char* end = nullptr;
      double v = std::strtod(str.c_str(), &end);
      return end == str.c_str() ? 0 : v;
    }
    default: return 0;
  }
}
std::string Variable::asString() const {
  switch (type) {
    case Number: return formatNumber(num);
    case Boolean: return boolean ? "true" : "false";
    case String: return str;
    default: return "";
  }
}
Variable& Variable::child(const std::string& key) {
  if (type == Array) {
    size_t idx = (size_t)std::atoll(key.c_str());
    if (idx >= children.size()) children.resize(idx + 1);
    return children[idx];
  }
  if (type != Structure) { type = Structure; keys.clear(); children.clear(); }
  for (size_t i = 0; i < keys.size(); i++)
    if (keys[i] == key) return children[i];
  keys.push_back(key);
  children.emplace_back();
  return children.back();
}

Variable Variable::fromJson(const Json& j) {
  Variable v;
  std::string t = j["type"].str("number");
  const Json& val = j["value"];
  if (t == "string") { v.type = String; v.str = val.type == Json::String ? val.s : formatNumber(val.num()); }
  else if (t == "boolean") { v.type = Boolean; v.boolean = val.type == Json::String ? val.s == "true" : val.boolean(); }
  else if (t == "structure") {
    v.type = Structure;
    const Json& ch = j["children"];
    for (size_t i = 0; i < ch.size(); i++) {
      v.keys.push_back(ch.at(i)["name"].str());
      v.children.push_back(fromJson(ch.at(i)));
    }
  } else if (t == "array") {
    v.type = Array;
    const Json& ch = j["children"];
    for (size_t i = 0; i < ch.size(); i++) v.children.push_back(fromJson(ch.at(i)));
  } else { v.type = Number; v.num = val.num(); }
  return v;
}

Variable& VariablesContainer::get(const std::string& path) {
  size_t dot = path.find('.');
  Variable* v = &vars[dot == std::string::npos ? path : path.substr(0, dot)];
  while (dot != std::string::npos) {
    size_t next = path.find('.', dot + 1);
    v = &v->child(path.substr(dot + 1, next == std::string::npos ? std::string::npos : next - dot - 1));
    dot = next;
  }
  return *v;
}
const Variable* VariablesContainer::find(const std::string& path) const {
  size_t dot = path.find('.');
  auto it = vars.find(dot == std::string::npos ? path : path.substr(0, dot));
  if (it == vars.end()) return nullptr;
  const Variable* v = &it->second;
  while (dot != std::string::npos) {
    size_t next = path.find('.', dot + 1);
    std::string key = path.substr(dot + 1, next == std::string::npos ? std::string::npos : next - dot - 1);
    const Variable* found = nullptr;
    if (v->type == Variable::Structure) {
      for (size_t i = 0; i < v->keys.size(); i++) if (v->keys[i] == key) found = &v->children[i];
    } else if (v->type == Variable::Array) {
      size_t idx = (size_t)std::atoll(key.c_str());
      if (idx < v->children.size()) found = &v->children[idx];
    }
    if (!found) return nullptr;
    v = found;
    dot = next;
  }
  return v;
}
void VariablesContainer::loadFromJson(const Json& arr) {
  for (size_t i = 0; i < arr.size(); i++) vars[arr.at(i)["name"].str()] = Variable::fromJson(arr.at(i));
}

}  // namespace gdcpp
