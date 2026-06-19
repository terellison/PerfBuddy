#include "pb_core/json.hpp"

#include <cmath>
#include <cstdio>
#include <sstream>

namespace pb::json {

namespace {

void escape_string(const std::string& s, std::string& out) {
  out += '"';
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  out += '"';
}

void format_number(double d, std::string& out) {
  if (std::isnan(d) || std::isinf(d)) {
    out += "null";  // JSON has no NaN/Inf
    return;
  }
  // Integers print without a trailing ".0"; otherwise use a compact %g.
  if (d == static_cast<double>(static_cast<long long>(d)) &&
      std::abs(d) < 1e15) {
    out += std::to_string(static_cast<long long>(d));
  } else {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.10g", d);
    out += buf;
  }
}

void newline_indent(std::string& out, int indent, int depth) {
  if (indent < 0) return;
  out += '\n';
  out.append(static_cast<size_t>(indent) * depth, ' ');
}

}  // namespace

void Value::dump_to(std::string& out, int indent, int depth) const {
  switch (type_) {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += bool_ ? "true" : "false"; break;
    case Type::Number: format_number(num_, out); break;
    case Type::String: escape_string(str_, out); break;
    case Type::Array: {
      if (arr_.empty()) { out += "[]"; break; }
      out += '[';
      for (size_t i = 0; i < arr_.size(); ++i) {
        if (i) out += ',';
        newline_indent(out, indent, depth + 1);
        arr_[i].dump_to(out, indent, depth + 1);
      }
      newline_indent(out, indent, depth);
      out += ']';
      break;
    }
    case Type::Object: {
      if (obj_.empty()) { out += "{}"; break; }
      out += '{';
      for (size_t i = 0; i < obj_.size(); ++i) {
        if (i) out += ',';
        newline_indent(out, indent, depth + 1);
        escape_string(obj_[i].first, out);
        out += indent < 0 ? ":" : ": ";
        obj_[i].second.dump_to(out, indent, depth + 1);
      }
      newline_indent(out, indent, depth);
      out += '}';
      break;
    }
  }
}

std::string Value::dump(int indent) const {
  std::string out;
  dump_to(out, indent, 0);
  return out;
}

// --------------------------------------------------------------------------
// Parser (recursive descent).
// --------------------------------------------------------------------------
namespace {

struct Parser {
  const std::string& s;
  size_t i = 0;

  explicit Parser(const std::string& src) : s(src) {}

  [[noreturn]] void fail(const std::string& msg) {
    throw std::runtime_error("JSON parse error at offset " + std::to_string(i) +
                             ": " + msg);
  }

  void skip_ws() {
    while (i < s.size()) {
      char c = s[i];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i;
      else break;
    }
  }

  char peek() {
    if (i >= s.size()) fail("unexpected end of input");
    return s[i];
  }

  Value parse_value() {
    skip_ws();
    char c = peek();
    switch (c) {
      case '{': return parse_object();
      case '[': return parse_array();
      case '"': return Value(parse_string());
      case 't': case 'f': return parse_bool();
      case 'n': return parse_null();
      default: return parse_number();
    }
  }

  Value parse_object() {
    Object obj;
    ++i;  // {
    skip_ws();
    if (peek() == '}') { ++i; return Value(std::move(obj)); }
    while (true) {
      skip_ws();
      if (peek() != '"') fail("expected object key");
      std::string key = parse_string();
      skip_ws();
      if (peek() != ':') fail("expected ':'");
      ++i;
      obj.emplace_back(std::move(key), parse_value());
      skip_ws();
      char c = peek();
      if (c == ',') { ++i; continue; }
      if (c == '}') { ++i; break; }
      fail("expected ',' or '}'");
    }
    return Value(std::move(obj));
  }

  Value parse_array() {
    Array arr;
    ++i;  // [
    skip_ws();
    if (peek() == ']') { ++i; return Value(std::move(arr)); }
    while (true) {
      arr.push_back(parse_value());
      skip_ws();
      char c = peek();
      if (c == ',') { ++i; continue; }
      if (c == ']') { ++i; break; }
      fail("expected ',' or ']'");
    }
    return Value(std::move(arr));
  }

  std::string parse_string() {
    std::string out;
    ++i;  // opening quote
    while (true) {
      if (i >= s.size()) fail("unterminated string");
      char c = s[i++];
      if (c == '"') break;
      if (c == '\\') {
        if (i >= s.size()) fail("unterminated escape");
        char e = s[i++];
        switch (e) {
          case '"': out += '"'; break;
          case '\\': out += '\\'; break;
          case '/': out += '/'; break;
          case 'n': out += '\n'; break;
          case 't': out += '\t'; break;
          case 'r': out += '\r'; break;
          case 'b': out += '\b'; break;
          case 'f': out += '\f'; break;
          case 'u': {
            if (i + 4 > s.size()) fail("bad \\u escape");
            unsigned code = std::stoul(s.substr(i, 4), nullptr, 16);
            i += 4;
            // Minimal UTF-8 encoding (BMP only; good enough for our inputs).
            if (code < 0x80) {
              out += static_cast<char>(code);
            } else if (code < 0x800) {
              out += static_cast<char>(0xC0 | (code >> 6));
              out += static_cast<char>(0x80 | (code & 0x3F));
            } else {
              out += static_cast<char>(0xE0 | (code >> 12));
              out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
              out += static_cast<char>(0x80 | (code & 0x3F));
            }
            break;
          }
          default: fail("invalid escape character");
        }
      } else {
        out += c;
      }
    }
    return out;
  }

  Value parse_bool() {
    if (s.compare(i, 4, "true") == 0) { i += 4; return Value(true); }
    if (s.compare(i, 5, "false") == 0) { i += 5; return Value(false); }
    fail("invalid literal");
  }

  Value parse_null() {
    if (s.compare(i, 4, "null") == 0) { i += 4; return Value(nullptr); }
    fail("invalid literal");
  }

  Value parse_number() {
    size_t start = i;
    if (peek() == '-') ++i;
    while (i < s.size() &&
           (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.' ||
            s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) {
      ++i;
    }
    if (i == start) fail("invalid number");
    return Value(std::stod(s.substr(start, i - start)));
  }
};

}  // namespace

Value Value::parse(const std::string& text) {
  Parser p(text);
  Value v = p.parse_value();
  p.skip_ws();
  if (p.i != text.size())
    throw std::runtime_error("JSON parse error: trailing characters");
  return v;
}

}  // namespace pb::json
