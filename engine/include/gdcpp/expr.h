#pragma once
#include <string>
#include <vector>

namespace gdcpp {

struct Value {
  bool isString = false;
  double num = 0;
  std::string str;
  static Value number(double v) { Value r; r.num = v; return r; }
  static Value text(const std::string& s) { Value r; r.isString = true; r.str = s; return r; }
};

double toNumber(const Value& v);
std::string toText(const Value& v);

// Quem avalia funções/expressões do jogo (TimeDelta(), Player.X(), Variable(x)...).
class ExprContext {
 public:
  virtual ~ExprContext() {}
  virtual Value call(const std::string& fn, const std::vector<Value>& args) = 0;
};

// Avalia uma expressão do GDevelop. Erros de sintaxe resultam em 0.
Value evaluate(const std::string& expression, ExprContext& ctx);

}  // namespace gdcpp
