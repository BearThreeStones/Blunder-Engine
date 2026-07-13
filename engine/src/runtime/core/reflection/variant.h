#pragma once

#include <cstdint>

#include "EASTL/string.h"

#include "runtime/core/math/math_types.h"

namespace Blunder {

enum class VariantType : uint8_t {
  Nil = 0,
  Bool,
  Int,
  Float,
  Vec3,
  Quat,
  String,
};

class Variant {
 public:
  Variant() = default;
  explicit Variant(bool value);
  explicit Variant(int64_t value);
  explicit Variant(float value);
  explicit Variant(const Vec3& value);
  explicit Variant(const Quat& value);
  explicit Variant(eastl::string value);

  VariantType getType() const { return m_type; }

  bool asBool() const;
  int64_t asInt() const;
  float asFloat() const;
  Vec3 asVec3() const;
  Quat asQuat() const;
  eastl::string asString() const;

  bool operator==(const Variant& other) const;

 private:
  VariantType m_type{VariantType::Nil};
  bool m_bool{false};
  int64_t m_int{0};
  float m_float{0.0f};
  Vec3 m_vec3{0.0f};
  Quat m_quat{glm::identity<Quat>()};
  eastl::string m_string;
};

}  // namespace Blunder
