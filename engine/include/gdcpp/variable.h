#pragma once
#include <map>
#include <string>
#include <vector>
#include "gdcpp/json.h"

namespace gdcpp {

std::string formatNumber(double v);

class Variable {
 public:
  enum Type { Number, String, Boolean, Structure, Array };
  Type type = Number;
  double num = 0;
  std::string str;
  bool boolean = false;
  std::vector<std::string> keys;  // Structure
  std::vector<Variable> children; // Structure/Array

  double asNumber() const;
  std::string asString() const;
  void setNumber(double v) { type = Number; num = v; }
  void setString(const std::string& v) { type = String; str = v; }
  Variable& child(const std::string& key);  // cria se necessário
  static Variable fromJson(const Json& j);
};

class VariablesContainer {
 public:
  std::map<std::string, Variable> vars;
  Variable& get(const std::string& path);  // "a.b.c" acessa estruturas
  const Variable* find(const std::string& path) const;
  void loadFromJson(const Json& arr);
};

}  // namespace gdcpp
