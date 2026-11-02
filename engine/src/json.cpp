#include "gdcpp/json.h"
#include <cstdlib>
#include <stdexcept>

namespace gdcpp {

static const Json& nullJson() {
  static const Json j;
  return j;
}

const Json& Json::at(size_t i) const {
  if (type != Array || i >= items.size()) return nullJson();
  return items[i];
}
bool Json::has(const std::string& key) const {
  if (type != Object) return false;
  for (const auto& k : keys) if (k == key) return true;
  return false;
}
const Json& Json::operator[](const std::string& key) const {
  if (type != Object) return nullJson();
  for (size_t i = 0; i < keys.size(); i++)
    if (keys[i] == key) return items[i];
  return nullJson();
}
double Json::num(double def) const {
  if (type == Number) return n;
  if (type == Bool) return b ? 1 : 0;
  if (type == String && !s.empty()) {
    char* end = nullptr;
    double v = std::strtod(s.c_str(), &end);
    if (end != s.c_str()) return v;
  }
  return def;
}
std::string Json::str(const std::string& def) const { return type == String ? s : def; }
bool Json::boolean(bool def) const {
  if (type == Bool) return b;
  if (type == Number) return n != 0;
  return def;
}

namespace {
struct Parser {
  const std::string& t;
  size_t i = 0;
  explicit Parser(const std::string& text) : t(text) {}

  [[noreturn]] void fail(const char* msg) {
    throw std::runtime_error(std::string("JSON inválido: ") + msg + " na posição " + std::to_string(i));
  }
  void ws() {
    while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) i++;
  }
  static void utf8(std::string& o, unsigned cp) {
    if (cp < 0x80) o += (char)cp;
    else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
    else { o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 0x3F)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
  }
  unsigned hex4() {
    if (i + 4 > t.size()) fail("escape \\u incompleto");
    unsigned v = 0;
    for (int k = 0; k < 4; k++) {
      char c = t[i++];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= c - '0';
      else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
      else fail("hex inválido");
    }
    return v;
  }
  std::string string() {
    std::string o;
    i++;  // aspas de abertura
    while (true) {
      if (i >= t.size()) fail("string não terminada");
      char c = t[i++];
      if (c == '"') break;
      if (c != '\\') { o += c; continue; }
      if (i >= t.size()) fail("escape inválido");
      char e = t[i++];
      switch (e) {
        case '"': o += '"'; break;
        case '\\': o += '\\'; break;
        case '/': o += '/'; break;
        case 'b': o += '\b'; break;
        case 'f': o += '\f'; break;
        case 'n': o += '\n'; break;
        case 'r': o += '\r'; break;
        case 't': o += '\t'; break;
        case 'u': {
          unsigned cp = hex4();
          if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < t.size() && t[i] == '\\' && t[i + 1] == 'u') {
            i += 2;
            unsigned lo = hex4();
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          }
          utf8(o, cp);
          break;
        }
        default: fail("escape desconhecido");
      }
    }
    return o;
  }
  Json value(int depth) {
    if (depth > 512) fail("aninhamento profundo demais");
    ws();
    if (i >= t.size()) fail("fim inesperado");
    Json j;
    char c = t[i];
    if (c == '{') {
      j.type = Json::Object;
      i++; ws();
      if (i < t.size() && t[i] == '}') { i++; return j; }
      while (true) {
        ws();
        if (i >= t.size() || t[i] != '"') fail("chave esperada");
        std::string k = string();
        ws();
        if (i >= t.size() || t[i] != ':') fail("':' esperado");
        i++;
        j.keys.push_back(std::move(k));
        j.items.push_back(value(depth + 1));
        ws();
        if (i < t.size() && t[i] == ',') { i++; continue; }
        if (i < t.size() && t[i] == '}') { i++; break; }
        fail("',' ou '}' esperado");
      }
    } else if (c == '[') {
      j.type = Json::Array;
      i++; ws();
      if (i < t.size() && t[i] == ']') { i++; return j; }
      while (true) {
        j.items.push_back(value(depth + 1));
        ws();
        if (i < t.size() && t[i] == ',') { i++; continue; }
        if (i < t.size() && t[i] == ']') { i++; break; }
        fail("',' ou ']' esperado");
      }
    } else if (c == '"') {
      j.type = Json::String;
      j.s = string();
    } else if (t.compare(i, 4, "true") == 0) { j.type = Json::Bool; j.b = true; i += 4; }
    else if (t.compare(i, 5, "false") == 0) { j.type = Json::Bool; j.b = false; i += 5; }
    else if (t.compare(i, 4, "null") == 0) { i += 4; }
    else {
      char* end = nullptr;
      double v = std::strtod(t.c_str() + i, &end);
      if (end == t.c_str() + i) fail("valor inesperado");
      j.type = Json::Number;
      j.n = v;
      i = (size_t)(end - t.c_str());
    }
    return j;
  }
};
}  // namespace

Json Json::parse(const std::string& text) {
  Parser p(text);
  if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) p.i = 3;
  Json j = p.value(0);
  p.ws();
  if (p.i != text.size()) p.fail("conteúdo extra após o JSON");
  return j;
}

}  // namespace gdcpp
