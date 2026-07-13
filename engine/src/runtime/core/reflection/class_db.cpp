#include "runtime/core/reflection/class_db.h"

#include "EASTL/unique_ptr.h"
#include "EASTL/unordered_map.h"
#include "EASTL/vector.h"

#include "runtime/core/reflection/generated/register_generated.h"

namespace Blunder {
namespace {

struct ClassEntry {
  TypeInfo info;
  eastl::vector<PropertyBinding> properties;
  eastl::vector<eastl::unique_ptr<MethodBind>> methods;
};

eastl::unordered_map<eastl::string, ClassEntry>& classes() {
  static eastl::unordered_map<eastl::string, ClassEntry> s_classes;
  return s_classes;
}

}  // namespace

void ClassDB::clear() { classes().clear(); }

void ClassDB::registerClass(const char* name, const char* parent_name) {
  if (name == nullptr || name[0] == '\0') {
    return;
  }
  ClassEntry& entry = classes()[name];
  entry.info.name = name;
  entry.info.parent_name = parent_name != nullptr ? parent_name : "";
}

bool ClassDB::hasClass(const char* name) {
  if (name == nullptr) {
    return false;
  }
  return classes().find(name) != classes().end();
}

const TypeInfo* ClassDB::getClass(const char* name) {
  if (name == nullptr) {
    return nullptr;
  }
  const auto it = classes().find(name);
  if (it == classes().end()) {
    return nullptr;
  }
  return &it->second.info;
}

void ClassDB::addProperty(const char* class_name, PropertyInfo info,
                          PropertySetter setter, PropertyGetter getter) {
  if (class_name == nullptr) {
    return;
  }
  auto it = classes().find(class_name);
  if (it == classes().end()) {
    return;
  }
  PropertyBinding binding;
  binding.info = info;
  binding.setter = setter;
  binding.getter = getter;
  it->second.info.properties.push_back(info);
  it->second.properties.push_back(eastl::move(binding));
}

void ClassDB::addMethod(const char* class_name, MethodBind* method) {
  if (class_name == nullptr || method == nullptr) {
    return;
  }
  auto it = classes().find(class_name);
  if (it == classes().end()) {
    return;
  }
  it->second.methods.emplace_back(method);
}

MethodBind* ClassDB::getMethod(const char* class_name,
                               const char* method_name) {
  if (class_name == nullptr || method_name == nullptr) {
    return nullptr;
  }
  auto it = classes().find(class_name);
  if (it == classes().end()) {
    return nullptr;
  }
  for (eastl::unique_ptr<MethodBind>& method : it->second.methods) {
    if (method != nullptr && method->getName() != nullptr &&
        eastl::string(method->getName()) == method_name) {
      return method.get();
    }
  }
  return nullptr;
}

bool ClassDB::setProperty(void* instance, const char* class_name,
                          const char* property_name, const Variant& value) {
  if (instance == nullptr || class_name == nullptr || property_name == nullptr) {
    return false;
  }
  auto it = classes().find(class_name);
  if (it == classes().end()) {
    return false;
  }
  for (PropertyBinding& binding : it->second.properties) {
    if (binding.info.name == property_name && binding.setter != nullptr) {
      binding.setter(instance, value);
      return true;
    }
  }
  return false;
}

bool ClassDB::getProperty(const void* instance, const char* class_name,
                          const char* property_name, Variant& out_value) {
  if (instance == nullptr || class_name == nullptr || property_name == nullptr) {
    return false;
  }
  auto it = classes().find(class_name);
  if (it == classes().end()) {
    return false;
  }
  for (const PropertyBinding& binding : it->second.properties) {
    if (binding.info.name == property_name && binding.getter != nullptr) {
      out_value = binding.getter(instance);
      return true;
    }
  }
  return false;
}

void ClassDB::initialize() {
  clear();
  register_reflection_generated();
}

void ClassDB::shutdown() { clear(); }

}  // namespace Blunder
