#include "runtime/function/script/behaviour_type_catalog.h"

#include <cctype>
#include <fstream>

namespace Blunder {

namespace {

namespace fs = std::filesystem;

class JsonCursor {
 public:
  explicit JsonCursor(eastl::string text) : m_text(eastl::move(text)) {}

  bool eof() const { return m_pos >= m_text.size(); }

  void skipWs() {
    while (!eof() && std::isspace(static_cast<unsigned char>(m_text[m_pos]))) {
      ++m_pos;
    }
  }

  bool consume(char c) {
    skipWs();
    if (eof() || m_text[m_pos] != c) {
      return false;
    }
    ++m_pos;
    return true;
  }

  bool parseString(eastl::string& out) {
    skipWs();
    if (eof() || m_text[m_pos] != '"') {
      return false;
    }
    ++m_pos;
    eastl::string value;
    while (!eof()) {
      const char c = m_text[m_pos++];
      if (c == '"') {
        out = eastl::move(value);
        return true;
      }
      if (c == '\\') {
        if (eof()) {
          return false;
        }
        const char esc = m_text[m_pos++];
        switch (esc) {
          case '"':
          case '\\':
          case '/':
            value.push_back(esc);
            break;
          case 'b':
            value.push_back('\b');
            break;
          case 'f':
            value.push_back('\f');
            break;
          case 'n':
            value.push_back('\n');
            break;
          case 'r':
            value.push_back('\r');
            break;
          case 't':
            value.push_back('\t');
            break;
          default:
            return false;
        }
        continue;
      }
      value.push_back(c);
    }
    return false;
  }

  bool parseMemberName(eastl::string& out) { return parseString(out); }

  bool parseObject(const char* const* required_keys, size_t required_count,
                   bool (*on_key)(JsonCursor&, const eastl::string&, void*),
                   void* user) {
    if (!consume('{')) {
      return false;
    }
    skipWs();
    if (consume('}')) {
      return true;
    }
  again:
    eastl::string key;
    if (!parseMemberName(key) || !consume(':')) {
      return false;
    }
    if (!on_key(*this, key, user)) {
      return false;
    }
    skipWs();
    if (consume(',')) {
      goto again;
    }
    return consume('}');
  }

  bool parseArray(bool (*on_item)(JsonCursor&, void*), void* user) {
    if (!consume('[')) {
      return false;
    }
    skipWs();
    if (consume(']')) {
      return true;
    }
  again:
    if (!on_item(*this, user)) {
      return false;
    }
    skipWs();
    if (consume(',')) {
      goto again;
    }
    return consume(']');
  }

  bool skipValue() {
    skipWs();
    if (eof()) {
      return false;
    }
    const char c = m_text[m_pos];
    if (c == '"') {
      eastl::string ignored;
      return parseString(ignored);
    }
    if (c == '{') {
      return parseObject(nullptr, 0,
                         [](JsonCursor& cur, const eastl::string&, void*) {
                           return cur.skipValue();
                         },
                         nullptr);
    }
    if (c == '[') {
      return parseArray(
          [](JsonCursor& cur, void*) { return cur.skipValue(); }, nullptr);
    }
    if (c == 't' || c == 'f' || c == 'n' ||
        c == '-' || (c >= '0' && c <= '9')) {
      while (!eof()) {
        const char ch = m_text[m_pos];
        if (ch == ',' || ch == '}' || ch == ']' ||
            std::isspace(static_cast<unsigned char>(ch))) {
          break;
        }
        ++m_pos;
      }
      return true;
    }
    return false;
  }

 private:
  eastl::string m_text;
  size_t m_pos{0};
};

struct MemberParseCtx {
  BehaviourCatalogMember* member{nullptr};
};

bool parseMemberObjectKey(JsonCursor& cur, const eastl::string& key, void* user) {
  auto* ctx = static_cast<MemberParseCtx*>(user);
  if (key == "name") {
    return cur.parseString(ctx->member->name);
  }
  if (key == "kind") {
    eastl::string kind;
    if (!cur.parseString(kind)) {
      return false;
    }
    if (kind == "bool") {
      ctx->member->kind = BehaviourCatalogMember::Kind::Bool;
      return true;
    }
    if (kind == "number") {
      ctx->member->kind = BehaviourCatalogMember::Kind::Number;
      return true;
    }
    if (kind == "string") {
      ctx->member->kind = BehaviourCatalogMember::Kind::String;
      return true;
    }
    if (kind == "clip_name") {
      ctx->member->kind = BehaviourCatalogMember::Kind::ClipName;
      return true;
    }
    return false;
  }
  return cur.skipValue();
}

struct TypeParseCtx {
  BehaviourCatalogType* type{nullptr};
};

bool parseTypeObjectKey(JsonCursor& cur, const eastl::string& key, void* user) {
  auto* ctx = static_cast<TypeParseCtx*>(user);
  if (key == "clr_name") {
    return cur.parseString(ctx->type->clr_name);
  }
  if (key == "members") {
    return cur.parseArray(
        [](JsonCursor& item_cur, void* type_user) {
          auto* type = static_cast<BehaviourCatalogType*>(type_user);
          BehaviourCatalogMember member;
          MemberParseCtx member_ctx{&member};
          if (!item_cur.parseObject(nullptr, 0, parseMemberObjectKey,
                                    &member_ctx)) {
            return false;
          }
          type->members.push_back(eastl::move(member));
          return true;
        },
        ctx->type);
  }
  return cur.skipValue();
}

struct RootParseCtx {
  eastl::vector<BehaviourCatalogType>* types{nullptr};
};

bool parseRootObjectKey(JsonCursor& cur, const eastl::string& key, void* user) {
  auto* ctx = static_cast<RootParseCtx*>(user);
  if (key != "types") {
    return cur.skipValue();
  }
  return cur.parseArray(
      [](JsonCursor& item_cur, void* root_user) {
        auto* types =
            static_cast<eastl::vector<BehaviourCatalogType>*>(root_user);
        BehaviourCatalogType type;
        TypeParseCtx type_ctx{&type};
        if (!item_cur.parseObject(nullptr, 0, parseTypeObjectKey, &type_ctx)) {
          return false;
        }
        types->push_back(eastl::move(type));
        return true;
      },
      ctx->types);
}

bool readTextFile(const fs::path& path, eastl::string& out_text,
                  eastl::string& error) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    error = "failed to open behaviour catalog";
    return false;
  }
  stream.seekg(0, std::ios::end);
  const std::streamoff size = stream.tellg();
  if (size < 0) {
    error = "failed to read behaviour catalog";
    return false;
  }
  stream.seekg(0, std::ios::beg);
  eastl::string text;
  text.resize(static_cast<size_t>(size));
  if (size > 0 && !stream.read(text.data(), size)) {
    error = "failed to read behaviour catalog";
    return false;
  }
  out_text = eastl::move(text);
  return true;
}

}  // namespace

bool loadBehaviourTypeCatalog(const fs::path& json_path,
                              eastl::vector<BehaviourCatalogType>& out,
                              eastl::string& error) {
  out.clear();
  error.clear();
  if (json_path.empty()) {
    error = "behaviour catalog path is required";
    return false;
  }

  eastl::string text;
  if (!readTextFile(json_path, text, error)) {
    return false;
  }

  JsonCursor cursor(eastl::move(text));
  RootParseCtx root_ctx{&out};
  if (!cursor.parseObject(nullptr, 0, parseRootObjectKey, &root_ctx)) {
    error = "invalid behaviour catalog JSON";
    out.clear();
    return false;
  }
  cursor.skipWs();
  if (!cursor.eof()) {
    error = "invalid behaviour catalog JSON";
    out.clear();
    return false;
  }
  return true;
}

}  // namespace Blunder
