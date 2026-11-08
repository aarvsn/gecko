// SPDX-License-Identifier: MIT
#include "gecko/json.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace gecko::json {

namespace {

constexpr int kMaxDepth = 64;

class Parser {
public:
    Parser(const char* begin, const char* end) : start_(begin), p_(begin), end_(end) {}

    bool parse(Value* out, std::string* error) {
        skipWs();
        if (!parseValue(out, 0)) {
            if (error) *error = makeError("unexpected token or truncated input");
            return false;
        }
        skipWs();
        if (p_ != end_) {
            if (error) *error = makeError("trailing characters after JSON value");
            return false;
        }
        return true;
    }

private:
    const char* start_;
    const char* p_;
    const char* end_;

    std::string makeError(const char* what) const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s near offset %ld", what,
                      static_cast<long>(p_ - start_));
        return buf;
    }

    void skipWs() {
        while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r')) ++p_;
    }

    bool consume(char c) {
        if (p_ < end_ && *p_ == c) {
            ++p_;
            return true;
        }
        return false;
    }

    bool parseValue(Value* out, int depth) {
        if (depth > kMaxDepth) return false;
        skipWs();
        if (p_ >= end_) return false;
        switch (*p_) {
            case '{': return parseObject(out, depth);
            case '[': return parseArray(out, depth);
            case '"': {
                std::string s;
                if (!parseString(&s)) return false;
                *out = Value(std::move(s));
                return true;
            }
            case 't':
                return consumeWord("true") ? (*out = Value(true), true) : false;
            case 'f':
                return consumeWord("false") ? (*out = Value(false), true) : false;
            case 'n':
                return consumeWord("null") ? (*out = Value(nullptr), true) : false;
            default: return parseNumber(out);
        }
    }

    bool consumeWord(const char* word) {
        const size_t len = std::strlen(word);
        if (static_cast<size_t>(end_ - p_) < len) return false;
        if (std::memcmp(p_, word, len) != 0) return false;
        p_ += len;
        return true;
    }

    bool parseObject(Value* out, int depth) {
        ++p_; // '{'
        *out = Value::object();
        skipWs();
        if (consume('}')) return true;
        for (;;) {
            skipWs();
            std::string key;
            if (p_ >= end_ || *p_ != '"' || !parseString(&key)) return false;
            skipWs();
            if (!consume(':')) return false;
            Value value;
            if (!parseValue(&value, depth + 1)) return false;
            out->set(key, std::move(value));
            skipWs();
            if (consume(',')) continue;
            if (consume('}')) return true;
            return false;
        }
    }

    bool parseArray(Value* out, int depth) {
        ++p_; // '['
        *out = Value::array();
        skipWs();
        if (consume(']')) return true;
        for (;;) {
            Value value;
            if (!parseValue(&value, depth + 1)) return false;
            out->append(std::move(value));
            skipWs();
            if (consume(',')) continue;
            if (consume(']')) return true;
            return false;
        }
    }

    bool parseString(std::string* out) {
        ++p_; // opening quote
        out->clear();
        for (;;) {
            if (p_ >= end_) return false;
            const unsigned char c = static_cast<unsigned char>(*p_);
            if (c == '"') {
                ++p_;
                return true;
            }
            if (c == '\\') {
                ++p_;
                if (p_ >= end_) return false;
                switch (*p_) {
                    case '"': out->push_back('"'); break;
                    case '\\': out->push_back('\\'); break;
                    case '/': out->push_back('/'); break;
                    case 'b': out->push_back('\b'); break;
                    case 'f': out->push_back('\f'); break;
                    case 'n': out->push_back('\n'); break;
                    case 'r': out->push_back('\r'); break;
                    case 't': out->push_back('\t'); break;
                    case 'u': {
                        uint32_t cp = 0;
                        if (!parseHex4(&cp)) return false;
                        // Surrogate pair handling.
                        if (cp >= 0xD800 && cp <= 0xDBFF && end_ - p_ >= 6 &&
                            p_[1] == '\\' && p_[2] == 'u') {
                            const char* saved = p_;
                            p_ += 2;
                            uint32_t lo = 0;
                            if (parseHex4(&lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            } else {
                                p_ = saved;
                            }
                        }
                        appendUtf8(out, cp);
                        // parseHex4 does not advance p_; consume the four hex
                        // digits here (the common ++p_ after the switch moves
                        // past the 'u' itself).
                        p_ += 4;
                        break;
                    }
                    default: return false;
                }
                ++p_;
                continue;
            }
            if (c < 0x20) return false; // raw control characters are invalid
            out->push_back(static_cast<char>(c));
            ++p_;
        }
    }

    bool parseHex4(uint32_t* out) {
        if (end_ - p_ < 5) return false; // 'u' + 4 digits (p_ points at 'u')
        uint32_t v = 0;
        for (int i = 1; i <= 4; ++i) {
            const char h = p_[i];
            v <<= 4;
            if (h >= '0' && h <= '9') v |= static_cast<uint32_t>(h - '0');
            else if (h >= 'a' && h <= 'f') v |= static_cast<uint32_t>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') v |= static_cast<uint32_t>(h - 'A' + 10);
            else return false;
        }
        *out = v;
        return true;
    }

    static void appendUtf8(std::string* out, uint32_t cp) {
        if (cp < 0x80) {
            out->push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out->push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out->push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out->push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool parseNumber(Value* out) {
        const char* start = p_;
        if (p_ < end_ && (*p_ == '-' || *p_ == '+')) ++p_;
        bool digits = false;
        while (p_ < end_ && std::isdigit(static_cast<unsigned char>(*p_))) {
            ++p_;
            digits = true;
        }
        if (p_ < end_ && *p_ == '.') {
            ++p_;
            while (p_ < end_ && std::isdigit(static_cast<unsigned char>(*p_))) {
                ++p_;
                digits = true;
            }
        }
        if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
            ++p_;
            if (p_ < end_ && (*p_ == '+' || *p_ == '-')) ++p_;
            while (p_ < end_ && std::isdigit(static_cast<unsigned char>(*p_))) ++p_;
        }
        if (!digits) return false;
        char* parseEnd = nullptr;
        const double value = std::strtod(start, &parseEnd);
        if (parseEnd != p_) return false;
        *out = Value(value);
        return true;
    }
};

void serializeString(const std::string& s, std::string* out) {
    out->push_back('"');
    for (char c : s) {
        switch (c) {
            case '"': *out += "\\\""; break;
            case '\\': *out += "\\\\"; break;
            case '\b': *out += "\\b"; break;
            case '\f': *out += "\\f"; break;
            case '\n': *out += "\\n"; break;
            case '\r': *out += "\\r"; break;
            case '\t': *out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    *out += buf;
                } else {
                    out->push_back(c);
                }
        }
    }
    out->push_back('"');
}

void serializeValue(const Value& v, std::string* out, int indent, bool pretty) {
    auto newline = [&](int level) {
        if (!pretty) return;
        out->push_back('\n');
        out->append(static_cast<size_t>(level) * 2, ' ');
    };

    switch (v.type()) {
        case Value::Type::Null: *out += "null"; break;
        case Value::Type::Boolean: *out += v.asBool() ? "true" : "false"; break;
        case Value::Type::Number: {
            double d = v.asNumber();
            if (std::floor(d) == d && std::fabs(d) < 1e15) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(d));
                *out += buf;
            } else {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.17g", d);
                *out += buf;
            }
            break;
        }
        case Value::Type::String: serializeString(v.asString(), out); break;
        case Value::Type::Array: {
            if (v.items().empty()) {
                *out += "[]";
                break;
            }
            *out += '[';
            bool first = true;
            for (const Value& item : v.items()) {
                if (!first) *out += ',';
                first = false;
                newline(indent + 1);
                serializeValue(item, out, indent + 1, pretty);
            }
            newline(indent);
            *out += ']';
            break;
        }
        case Value::Type::Object: {
            if (v.members().empty()) {
                *out += "{}";
                break;
            }
            *out += '{';
            bool first = true;
            for (const auto& [key, value] : v.members()) {
                if (!first) *out += ',';
                first = false;
                newline(indent + 1);
                serializeString(key, out);
                *out += pretty ? ": " : ":";
                serializeValue(value, out, indent + 1, pretty);
            }
            newline(indent);
            *out += '}';
            break;
        }
    }
}

} // namespace

const Value* Value::find(const std::string& key) const {
    for (const auto& [k, v] : object_)
        if (k == key) return &v;
    return nullptr;
}

Value& Value::getOrCreate(const std::string& key) {
    if (!isObject()) {
        type_ = Type::Object;
        object_.clear();
    }
    for (auto& [k, v] : object_) {
        if (k == key) {
            if (!v.isObject()) {
                v = Value::object();
            }
            return v;
        }
    }
    object_.emplace_back(key, Value::object());
    return object_.back().second;
}

Value& Value::set(const std::string& key, Value v) {
    for (auto& [k, val] : object_) {
        if (k == key) {
            val = std::move(v);
            return val;
        }
    }
    object_.emplace_back(key, std::move(v));
    return object_.back().second;
}

Value parse(const std::string& text, std::string* error) {
    Value result;
    Parser parser(text.data(), text.data() + text.size());
    if (!parser.parse(&result, error)) {
        return Value();
    }
    return result;
}

std::string serialize(const Value& value, bool pretty) {
    std::string out;
    out.reserve(256);
    serializeValue(value, &out, 0, pretty);
    if (pretty) out.push_back('\n');
    return out;
}

} // namespace gecko::json
