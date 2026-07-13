#pragma once

#include "EASTL/string.h"

#include "runtime/core/reflection/type_info.h"

namespace Blunder {

class ClassDB {
 public:
  static void clear();
  static void registerClass(const char* name, const char* parent_name = nullptr);
  static bool hasClass(const char* name);
  static const TypeInfo* getClass(const char* name);

  static void addProperty(const char* class_name, PropertyInfo info,
                          PropertySetter setter = nullptr,
                          PropertyGetter getter = nullptr);
  static void addMethod(const char* class_name, MethodBind* method);
  static MethodBind* getMethod(const char* class_name, const char* method_name);

  static bool setProperty(void* instance, const char* class_name,
                          const char* property_name, const Variant& value);
  static bool getProperty(const void* instance, const char* class_name,
                          const char* property_name, Variant& out_value);

  /// Clears and registers built-in pilot types. Safe to call multiple times.
  static void initialize();
  static void shutdown();
};

}  // namespace Blunder
