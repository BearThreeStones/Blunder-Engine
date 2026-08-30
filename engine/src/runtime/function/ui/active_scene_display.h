#pragma once

#include "EASTL/string.h"

#include <filesystem>

namespace Blunder {

inline constexpr char k_editor_product_name[] = "Blunder Editor";
inline constexpr char k_editor_title_separator[] = " - ";

eastl::string sceneShortNameFromVirtualPath(const eastl::string& virtual_path);
eastl::string formatHierarchySceneLabel(const eastl::string& virtual_path,
                                        bool dirty);
eastl::string formatApplicationBarWordmark(
    const eastl::string& project_display_name);
eastl::string formatEditorWindowTitle(
    const eastl::string& project_display_name,
    const eastl::string& virtual_path, bool dirty);
eastl::string projectDisplayNameFromRoot(const std::filesystem::path& root);

}  // namespace Blunder
