#pragma once
//
// pb::json — a tiny, dependency-free JSON value type with parse + dump.
//
// PerfBuddy deliberately ships no third-party libraries in its core so that the
// analysis modules and CLI build with nothing but a C++17 compiler and CMake.
// This is enough JSON for our needs: emitting reports and reading simple inputs.
//
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace pb::json {

class Value;
using Array = std::vector<Value>;
// Ordered map keeps object key order stable for readable, diffable output.
using Object = std::vector<std::pair<std::string, Value>>;

enum class Type { Null, Bool, Number, String, Array, Object };

class Value {
public:
  Value() : type_(Type::Null) {}
  Value(std::nullptr_t) : type_(Type::Null) {}
  Value(bool b) : type_(Type::Bool), bool_(b) {}
  Value(int n) : type_(Type::Number), num_(static_cast<double>(n)) {}
  Value(long n) : type_(Type::Number), num_(static_cast<double>(n)) {}
  Value(long long n) : type_(Type::Number), num_(static_cast<double>(n)) {}
  Value(unsigned n) : type_(Type::Number), num_(static_cast<double>(n)) {}
  Value(unsigned long n) : type_(Type::Number), num_(static_cast<double>(n)) {}
  Value(unsigned long long n) : type_(Type::Number), num_(static_cast<double>(n)) {}
  Value(double d) : type_(Type::Number), num_(d) {}
  Value(const char* s) : type_(Type::String), str_(s) {}
  Value(std::string s) : type_(Type::String), str_(std::move(s)) {}
  Value(Array a) : type_(Type::Array), arr_(std::move(a)) {}
  Value(Object o) : type_(Type::Object), obj_(std::move(o)) {}

  Type type() const { return type_; }
  bool is_null() const { return type_ == Type::Null; }
  bool is_object() const { return type_ == Type::Object; }
  bool is_array() const { return type_ == Type::Array; }

  bool as_bool(bool def = false) const { return type_ == Type::Bool ? bool_ : def; }
  double as_number(double def = 0.0) const { return type_ == Type::Number ? num_ : def; }
  const std::string& as_string() const { return str_; }
  std::string as_string(const std::string& def) const {
    return type_ == Type::String ? str_ : def;
  }
  const Array& as_array() const { return arr_; }
  const Object& as_object() const { return obj_; }

  // Object access helpers (return Null Value when absent).
  const Value& operator[](const std::string& key) const {
    static const Value null_value;
    for (const auto& kv : obj_)
      if (kv.first == key) return kv.second;
    return null_value;
  }
  bool contains(const std::string& key) const {
    for (const auto& kv : obj_)
      if (kv.first == key) return true;
    return false;
  }

  // Mutating object builder.
  void set(std::string key, Value v) {
    type_ = Type::Object;
    for (auto& kv : obj_) {
      if (kv.first == key) { kv.second = std::move(v); return; }
    }
    obj_.emplace_back(std::move(key), std::move(v));
  }
  void push_back(Value v) {
    type_ = Type::Array;
    arr_.push_back(std::move(v));
  }

  // Serialize. indent < 0 => compact; indent >= 0 => pretty with that many spaces.
  std::string dump(int indent = 2) const;

  // Parse. Throws std::runtime_error on malformed input.
  static Value parse(const std::string& text);

private:
  void dump_to(std::string& out, int indent, int depth) const;

  Type type_;
  bool bool_ = false;
  double num_ = 0.0;
  std::string str_;
  Array arr_;
  Object obj_;
};

}  // namespace pb::json
