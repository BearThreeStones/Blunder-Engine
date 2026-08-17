#pragma once

#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include "runtime/core/object/skeleton_modifier.h"

namespace Blunder {

using SkeletonModifierFactory = eastl::unique_ptr<SkeletonModifier> (*)();

class SkeletonModifierCatalog;

/// Unregisters one catalog row when reset or destroyed.
class SkeletonModifierTypeRegistration final {
 public:
  SkeletonModifierTypeRegistration() = default;
  ~SkeletonModifierTypeRegistration();
  SkeletonModifierTypeRegistration(const SkeletonModifierTypeRegistration&) = delete;
  SkeletonModifierTypeRegistration& operator=(
      const SkeletonModifierTypeRegistration&) = delete;
  SkeletonModifierTypeRegistration(SkeletonModifierTypeRegistration&& other) noexcept;
  SkeletonModifierTypeRegistration& operator=(
      SkeletonModifierTypeRegistration&& other) noexcept;

  void reset();
  explicit operator bool() const { return m_id != 0; }

 private:
  friend class SkeletonModifierCatalog;
  eastl::string m_name;
  uint64_t m_id{0};
};

class SkeletonModifierCatalog final {
 public:
  static SkeletonModifierTypeRegistration registerType(
      const char* name, SkeletonModifierFactory factory, bool show_in_add_menu);
  static eastl::unique_ptr<SkeletonModifier> construct(const char* name);
  static bool hasType(const char* name);
  static void listAddMenuTypes(eastl::vector<eastl::string>& out);
  static void clear();
  static void registerBuiltins();

 private:
  static void unregister(const char* name, uint64_t id);
  friend class SkeletonModifierTypeRegistration;
};

}  // namespace Blunder
