#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include <filesystem>

namespace Blunder {

struct BehaviourCatalogMember {
  eastl::string name;
  enum class Kind { Bool, Number, String, ClipName } kind{Kind::String};
};

struct BehaviourCatalogType {
  eastl::string clr_name;
  eastl::vector<BehaviourCatalogMember> members;
};

/// Loads `<project>/.blunder/behaviour_catalog.json` produced by
/// Blunder.ScriptsCatalog after a successful Scripts build.
bool loadBehaviourTypeCatalog(const std::filesystem::path& json_path,
                              eastl::vector<BehaviourCatalogType>& out,
                              eastl::string& error);

}  // namespace Blunder
