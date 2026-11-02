#include "gdcpp/expr.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include "gdcpp/variable.h"

namespace gdcpp {

double toNumber(const Value& v) {
  if (!v.isString) return v.num;
  char* end = nullptr;
  double r = std::strtod(v.str.c_str(), &end);
  return end == v.str.c_str() ? 0 : r;
}
std::string toText(const Value& v) { return v.isString ? v.str : formatNumber(v.num); }

namespace {

struct Node {
  enum Kind { Num, Str, Ident, Neg, Bin, Call } kind = Num;
  double num = 0;
  std::string str;  // texto / identificador / nome da função
  char op = 0;
  std::vector<std::shared_ptr<Node>> kids;
};
using NodeP = std::shared_ptr<Node>;

struct Parser {
  const std::string& t;
  size_t i = 0;
  explicit Parser(const std::string& s) : t(s) {}

  void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) i++; }
  static bool identStart(unsigned char c) { return std::isalpha(c) || c == '_' || c >= 0x80; }
  static bool identPart(unsigned char c) { return std::isalnum(c) || c == '_' || c == '.' || c >= 0x80; }

  NodeP make(Node::Kind k) { auto n = std::make_shared<Node>(); n->kind = k; return n; }

  NodeP parseAdd() {
    NodeP l = parseMul();
    while (true) {
      ws();
      if (i < t.size() && (t[i] == '+' || t[i] == '-')) {
        char op = t[i++];
        NodeP n = make(Node::Bin); n->op = op; n->kids = {l, parseMul()}; l = n;
      } else return l;
    }
  }
  NodeP parseMul() {
    NodeP l = parseUnary();
    while (true) {
      ws();
      if (i < t.size() && (t[i] == '*' || t[i] == '/' || t[i] == '%')) {
        char op = t[i++];
        NodeP n = make(Node::Bin); n->op = op; n->kids = {l, parseUnary()}; l = n;
      } else return l;
    }
  }
  NodeP parseUnary() {
    ws();
    if (i < t.size() && t[i] == '-') { i++; NodeP n = make(Node::Neg); n->kids = {parseUnary()}; return n; }
    if (i < t.size() && t[i] == '+') { i++; return parseUnary(); }
    return parsePrimary();
  }
  NodeP parsePrimary() {
    ws();
    if (i >= t.size()) throw std::runtime_error("fim inesperado");
    unsigned char c = t[i];
    if (c == '(') {
      i++;
      NodeP n = parseAdd();
      ws();
      if (i >= t.size() || t[i] != ')') throw std::runtime_error("')' esperado");
      i++;
      return n;
    }
    if (c == '"') {
      NodeP n = make(Node::Str);
      i++;
      while (i < t.size() && t[i] != '"') {
        if (t[i] == '\\' && i + 1 < t.size()) {
          i++;
          char e = t[i];
          n->str += e == 'n' ? '\n' : e == 't' ? '\t' : e;
          i++;
        } else n->str += t[i++];
      }
      if (i >= t.size()) throw std::runtime_error("string não terminada");
      i++;
      return n;
    }
    if (std::isdigit(c) || (c == '.' && i + 1 < t.size() && std::isdigit((unsigned char)t[i + 1]))) {
      char* end = nullptr;
      double v = std::strtod(t.c_str() + i, &end);
      NodeP n = make(Node::Num); n->num = v;
      i = (size_t)(end - t.c_str());
      return n;
    }
    if (identStart(c)) {
      size_t s = i;
      while (i < t.size() && identPart((unsigned char)t[i])) i++;
      std::string name = t.substr(s, i - s);
      ws();
      if (i < t.size() && t[i] == '(') {
        i++;
        NodeP n = make(Node::Call); n->str = name;
        while (true) {
          ws();
          if (i < t.size() && t[i] == ')') { i++; break; }
          n->kids.push_back(parseAdd());
          ws();
          if (i < t.size() && t[i] == ',') { i++; continue; }
          if (i < t.size() && t[i] == ')') { i++; break; }
          throw std::runtime_error("',' ou ')' esperado");
        }
        return n;
      }
      NodeP n = make(Node::Ident); n->str = name;  // identificador solto vira texto (ex.: Variable(score))
      return n;
    }
    throw std::runtime_error("token inesperado");
  }
};

Value run(const Node& n, ExprContext& ctx) {
  switch (n.kind) {
    case Node::Num: return Value::number(n.num);
    case Node::Str:
    case Node::Ident: return Value::text(n.str);
    case Node::Neg: return Value::number(-toNumber(run(*n.kids[0], ctx)));
    case Node::Bin: {
      Value a = run(*n.kids[0], ctx), b = run(*n.kids[1], ctx);
      if (n.op == '+' && (a.isString || b.isString)) return Value::text(toText(a) + toText(b));
      double x = toNumber(a), y = toNumber(b);
      switch (n.op) {
        case '+': return Value::number(x + y);
        case '-': return Value::number(x - y);
        case '*': return Value::number(x * y);
        case '/': return Value::number(x / y);
        default: return Value::number(std::fmod(x, y));
      }
    }
    case Node::Call: {
      std::vector<Value> args;
      args.reserve(n.kids.size());
      for (const auto& k : n.kids) args.push_back(run(*k, ctx));
      return ctx.call(n.str, args);
    }
  }
  return Value::number(0);
}

}  // namespace

Value evaluate(const std::string& expression, ExprContext& ctx) {
  static std::unordered_map<std::string, NodeP> cache;
  auto it = cache.find(expression);
  if (it == cache.end()) {
    NodeP root;
    try {
      Parser p(expression);
      root = p.parseAdd();
      p.ws();
      if (p.i != expression.size()) throw std::runtime_error("sobra texto");
    } catch (const std::exception&) {
      root = std::make_shared<Node>();  // Num 0
    }
    it = cache.emplace(expression, root).first;
  }
  return run(*it->second, ctx);
}

}  // namespace gdcpp
