#include "runtime/core/reflection/variant.h"

namespace Blunder {

Variant::Variant(bool value) : m_type(VariantType::Bool), m_bool(value) {}

Variant::Variant(int64_t value) : m_type(VariantType::Int), m_int(value) {}

Variant::Variant(float value) : m_type(VariantType::Float), m_float(value) {}

Variant::Variant(const Vec3& value) : m_type(VariantType::Vec3), m_vec3(value) {}

Variant::Variant(const Quat& value) : m_type(VariantType::Quat), m_quat(value) {}

Variant::Variant(eastl::string value)
    : m_type(VariantType::String), m_string(eastl::move(value)) {}

bool Variant::asBool() const { return m_bool; }
int64_t Variant::asInt() const { return m_int; }
float Variant::asFloat() const { return m_float; }
Vec3 Variant::asVec3() const { return m_vec3; }
Quat Variant::asQuat() const { return m_quat; }
eastl::string Variant::asString() const { return m_string; }

bool Variant::operator==(const Variant& other) const {
  if (m_type != other.m_type) {
    return false;
  }
  switch (m_type) {
    case VariantType::Nil:
      return true;
    case VariantType::Bool:
      return m_bool == other.m_bool;
    case VariantType::Int:
      return m_int == other.m_int;
    case VariantType::Float:
      return m_float == other.m_float;
    case VariantType::Vec3:
      return m_vec3 == other.m_vec3;
    case VariantType::Quat:
      return m_quat == other.m_quat;
    case VariantType::String:
      return m_string == other.m_string;
  }
  return false;
}

}  // namespace Blunder
