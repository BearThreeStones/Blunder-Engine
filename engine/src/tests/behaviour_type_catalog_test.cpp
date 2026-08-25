#include "runtime/function/script/behaviour_type_catalog.h"

#include <cstdio>

#ifndef BEHAVIOUR_CATALOG_FIXTURE
#define BEHAVIOUR_CATALOG_FIXTURE "fixtures/behaviour_catalog_sample.json"
#endif

namespace {
int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

const Blunder::BehaviourCatalogType* findType(
    const eastl::vector<Blunder::BehaviourCatalogType>& types,
    const char* clr_name) {
  for (const Blunder::BehaviourCatalogType& type : types) {
    if (type.clr_name == clr_name) {
      return &type;
    }
  }
  return nullptr;
}

const Blunder::BehaviourCatalogMember* findMember(
    const Blunder::BehaviourCatalogType& type, const char* name) {
  for (const Blunder::BehaviourCatalogMember& member : type.members) {
    if (member.name == name) {
      return &member;
    }
  }
  return nullptr;
}
}  // namespace

int main() {
  using namespace Blunder;

  eastl::vector<BehaviourCatalogType> types;
  eastl::string err;
  const std::filesystem::path fixture = BEHAVIOUR_CATALOG_FIXTURE;

  expect_true("load", loadBehaviourTypeCatalog(fixture, types, err));
  const BehaviourCatalogType* motor = findType(types, "Game.Motor");
  expect_true("has motor", motor != nullptr);
  if (motor != nullptr) {
    const BehaviourCatalogMember* speed = findMember(*motor, "Speed");
    expect_true("has speed", speed != nullptr);
    if (speed != nullptr) {
      expect_true("speed is number",
                  speed->kind == BehaviourCatalogMember::Kind::Number);
    }
    const BehaviourCatalogMember* label = findMember(*motor, "Label");
    expect_true("has label", label != nullptr);
    if (label != nullptr) {
      expect_true("label is string",
                  label->kind == BehaviourCatalogMember::Kind::String);
    }
    const BehaviourCatalogMember* flag = findMember(*motor, "EnabledFlag");
    expect_true("has flag", flag != nullptr);
    if (flag != nullptr) {
      expect_true("flag is bool",
                  flag->kind == BehaviourCatalogMember::Kind::Bool);
    }
    const BehaviourCatalogMember* idle = findMember(*motor, "IdleClip");
    expect_true("has IdleClip", idle != nullptr);
    if (idle != nullptr) {
      expect_true("IdleClip is clip_name",
                  idle->kind == BehaviourCatalogMember::Kind::ClipName);
    }
    const BehaviourCatalogMember* walk = findMember(*motor, "WalkClip");
    expect_true("has WalkClip", walk != nullptr);
    if (walk != nullptr) {
      expect_true("WalkClip is clip_name",
                  walk->kind == BehaviourCatalogMember::Kind::ClipName);
    }
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("behaviour_type_catalog_test: OK\n");
  return 0;
}
