// SPDX-License-Identifier: MIT
// Minimal JSON parser/serializer used for Gecko's configuration files.
// Supports the full JSON grammar (objects, arrays, strings with escapes,
// numbers, booleans, null) with a hard recursion depth limit.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace gecko::json {

class Value {
public:
    enum class Type { Null, Boolean, Number, String, Array, Object };

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : type_(Type::Boolean), bool_(b) {}
    Value(double n) : type_(Type::Number), number_(n) {}
    Value(int n) : type_(Type::Number), number_(static_cast<double>(n)) {}
    Value(int64_t n) : type_(Type::Number), number_(static_cast<double>(n)) {}
    Value(const char* s) : type_(Type::String), string_(s) {}
    Value(std::string s) : type_(Type::String), string_(std::move(s)) {}

    static Value array() {
        Value v;
        v.type_ = Type::Array;
        return v;
    }
    static Value object() {
        Value v;
        v.type_ = Type::Object;
        return v;
    }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBoolean() const { return type_ == Type::Boolean; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBool(bool fallback = false) const { return isBoolean() ? bool_ : fallback; }
    double asNumber(double fallback = 0.0) const { return isNumber() ? number_ : fallback; }
    int64_t asInteger(int64_t fallback = 0) const {
        return isNumber() ? static_cast<int64_t>(number_) : fallback;
    }
    const std::string& asString() const { return string_; }

    // Array access -----------------------------------------------------------
    std::vector<Value>& items() { return array_; }
    const std::vector<Value>& items() const { return array_; }
    void append(Value v) { array_.push_back(std::move(v)); }
    size_t size() const { return type_ == Type::Array ? array_.size() : (type_ == Type::Object ? object_.size() : 0); }

    // Object access ----------------------------------------------------------
    // Keeps insertion order for stable, diff-friendly config files.
    std::vector<std::pair<std::string, Value>>& members() { return object_; }
    const std::vector<std::pair<std::string, Value>>& members() const { return object_; }

    const Value* find(const std::string& key) const;
    // Returns the member `key`, creating (and converting to an object) when
    // missing. Used to walk/create nested config trees without wiping
    // existing siblings.
    Value& getOrCreate(const std::string& key);
    Value& set(const std::string& key, Value v); // insert or overwrite
    bool has(const std::string& key) const { return find(key) != nullptr; }

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Value> array_;
    std::vector<std::pair<std::string, Value>> object_;
};

// Parses `text`. On failure returns a Null value and fills `error`.
Value parse(const std::string& text, std::string* error = nullptr);

// Serializes with 2-space indentation (pretty) or as a single line.
std::string serialize(const Value& value, bool pretty = true);

} // namespace gecko::json
