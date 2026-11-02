#pragma once
// Parser JSON mínimo, sem dependências externas (o projeto GDevelop é um JSON).
#include <string>
#include <vector>

namespace gdcpp {

class Json {
 public:
  enum Type { Null, Bool, Number, String, Array, Object };
  Type type = Null;
  bool b = false;
  double n = 0;
  std::string s;
  std::vector<std::string> keys;  // só para Object
  std::vector<Json> items;        // valores de Array/Object

  bool isNull() const { return type == Null; }
  size_t size() const { return (type == Array || type == Object) ? items.size() : 0; }
  const Json& at(size_t i) const;
  bool has(const std::string& key) const;
  const Json& operator[](const std::string& key) const;
  const Json& operator[](const char* key) const { return (*this)[std::string(key)]; }

  double num(double def = 0) const;
  std::string str(const std::string& def = "") const;
  bool boolean(bool def = false) const;

  // Lança std::runtime_error em caso de JSON inválido.
  static Json parse(const std::string& text);
};

}  // namespace gdcpp
