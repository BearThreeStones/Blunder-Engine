#include "runtime/core/object/skeleton_modifier_catalog.h"

#include <cstddef>

#include "EASTL/vector.h"

#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"

namespace Blunder {
namespace {

struct CatalogEntry {
  eastl::string name;
  SkeletonModifierFactory factory{nullptr};
  bool show_in_add_menu{false};
  uint64_t id{0};
};

eastl::vector<CatalogEntry>& entries() {
  static eastl::vector<CatalogEntry> s_entries;
  return s_entries;
}

uint64_t& next_id() {
  static uint64_t s_next_id = 1;
  return s_next_id;
}

eastl::unique_ptr<SkeletonModifier> makeBaseModifier() {
  return eastl::make_unique<SkeletonModifier>();
}

eastl::unique_ptr<SkeletonModifier> makePaperMouth() {
  return eastl::make_unique<SkeletonPaperMouthModifier>();
}

eastl::unique_ptr<SkeletonModifier> makeAttach() {
  return eastl::make_unique<SkeletonAttachModifier>();
}

eastl::unique_ptr<SkeletonModifier> makeLookAt() {
  return eastl::make_unique<SkeletonLookAtModifier>();
}

CatalogEntry* findEntry(const char* name) {
  if (name == nullptr || name[0] == '\0') {
    return nullptr;
  }
  eastl::vector<CatalogEntry>& list = entries();
  for (CatalogEntry& entry : list) {
    if (entry.name == name) {
      return &entry;
    }
  }
  return nullptr;
}

void addOrReplaceEntry(const char* name, SkeletonModifierFactory factory,
                       bool show_in_add_menu, uint64_t id) {
  CatalogEntry* existing = findEntry(name);
  if (existing != nullptr) {
    existing->factory = factory;
    existing->show_in_add_menu = show_in_add_menu;
    existing->id = id;
    return;
  }
  CatalogEntry entry;
  entry.name = name;
  entry.factory = factory;
  entry.show_in_add_menu = show_in_add_menu;
  entry.id = id;
  entries().push_back(eastl::move(entry));
}

}  // namespace

SkeletonModifierTypeRegistration::~SkeletonModifierTypeRegistration() {
  reset();
}

SkeletonModifierTypeRegistration::SkeletonModifierTypeRegistration(
    SkeletonModifierTypeRegistration&& other) noexcept
    : m_name(eastl::move(other.m_name)), m_id(other.m_id) {
  other.m_id = 0;
  other.m_name.clear();
}

SkeletonModifierTypeRegistration& SkeletonModifierTypeRegistration::operator=(
    SkeletonModifierTypeRegistration&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  reset();
  m_name = eastl::move(other.m_name);
  m_id = other.m_id;
  other.m_id = 0;
  other.m_name.clear();
  return *this;
}

void SkeletonModifierTypeRegistration::reset() {
  if (m_id != 0) {
    SkeletonModifierCatalog::unregister(m_name.c_str(), m_id);
    m_id = 0;
    m_name.clear();
  }
}

SkeletonModifierTypeRegistration SkeletonModifierCatalog::registerType(
    const char* name, SkeletonModifierFactory factory, bool show_in_add_menu) {
  SkeletonModifierTypeRegistration registration;
  if (name == nullptr || name[0] == '\0' || factory == nullptr) {
    return registration;
  }
  const uint64_t id = next_id()++;
  addOrReplaceEntry(name, factory, show_in_add_menu, id);
  registration.m_name = name;
  registration.m_id = id;
  return registration;
}

void SkeletonModifierCatalog::unregister(const char* name, uint64_t id) {
  if (name == nullptr || id == 0) {
    return;
  }
  eastl::vector<CatalogEntry>& list = entries();
  for (size_t i = 0; i < list.size(); ++i) {
    if (list[i].id == id && list[i].name == name) {
      list.erase(list.begin() + static_cast<ptrdiff_t>(i));
      return;
    }
  }
}

eastl::unique_ptr<SkeletonModifier> SkeletonModifierCatalog::construct(
    const char* name) {
  CatalogEntry* entry = findEntry(name);
  if (entry == nullptr || entry->factory == nullptr) {
    return nullptr;
  }
  return entry->factory();
}

bool SkeletonModifierCatalog::hasType(const char* name) {
  return findEntry(name) != nullptr;
}

void SkeletonModifierCatalog::listAddMenuTypes(eastl::vector<eastl::string>& out) {
  out.clear();
  for (const CatalogEntry& entry : entries()) {
    if (entry.show_in_add_menu) {
      out.push_back(entry.name);
    }
  }
}

void SkeletonModifierCatalog::clear() {
  entries().clear();
}

void SkeletonModifierCatalog::registerBuiltins() {
  addOrReplaceEntry("SkeletonModifier", &makeBaseModifier, false, next_id()++);
  addOrReplaceEntry("PaperMouth", &makePaperMouth, true, next_id()++);
  addOrReplaceEntry("SkeletonAttachModifier", &makeAttach, true, next_id()++);
  addOrReplaceEntry("SkeletonLookAtModifier", &makeLookAt, true, next_id()++);
}

}  // namespace Blunder
