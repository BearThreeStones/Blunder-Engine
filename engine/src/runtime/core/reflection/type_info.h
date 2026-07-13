#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/reflection/variant.h"

namespace Blunder {

struct PropertyInfo {
  eastl::string name;
  VariantType type{VariantType::Nil};
};

using PropertySetter = void (*)(void* instance, const Variant& value);
using PropertyGetter = Variant (*)(const void* instance);

struct PropertyBinding {
  PropertyInfo info;
  PropertySetter setter{nullptr};
  PropertyGetter getter{nullptr};
};

struct TypeInfo {
  eastl::string name;
  eastl::string parent_name;
  eastl::vector<PropertyInfo> properties;
};

class MethodBind {
 public:
  virtual ~MethodBind() = default;
  virtual void ptrcall(void* instance, const void** args, void* ret) = 0;
  virtual const char* getName() const = 0;
};

}  // namespace Blunder
