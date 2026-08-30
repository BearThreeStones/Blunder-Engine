#include "runtime/function/ui/active_scene_display.h"

#include "runtime/project/project_file.h"

#include <system_error>

namespace Blunder {
namespace {

namespace fs = std::filesystem;

constexpr char k_scene_suffix[] = ".scene.asset";
constexpr size_t k_scene_suffix_len = sizeof(k_scene_suffix) - 1u;

eastl::string folderNameFromRoot(const fs::path& root) {
  if (root.empty()) {
    return {};
  }
  std::error_code ec;
  fs::path normalized = fs::weakly_canonical(root, ec);
  if (ec) {
    normalized = root;
  }
  fs::path name = normalized.filename();
  if (name.empty()) {
    name = normalized.parent_path().filename();
  }
  if (name.empty()) {
    return {};
  }
  return eastl::string(name.string().c_str());
}

}  // namespace

eastl::string sceneShortNameFromVirtualPath(const eastl::string& virtual_path) {
  if (virtual_path.empty()) {
    return {};
  }
  const size_t slash = virtual_path.find_last_of('/');
  eastl::string file =
      slash == eastl::string::npos
          ? virtual_path
          : virtual_path.substr(slash + 1, virtual_path.size() - (slash + 1));
  if (file.size() >= k_scene_suffix_len &&
      file.compare(file.size() - k_scene_suffix_len, k_scene_suffix_len,
                   k_scene_suffix) == 0) {
    return file.substr(0, file.size() - k_scene_suffix_len);
  }
  return file;
}

eastl::string formatHierarchySceneLabel(const eastl::string& virtual_path,
                                        bool dirty) {
  if (virtual_path.empty()) {
    return eastl::string("(No Scene)");
  }
  eastl::string label = sceneShortNameFromVirtualPath(virtual_path);
  if (dirty) {
    label.push_back('*');
  }
  return label;
}

eastl::string formatApplicationBarWordmark(
    const eastl::string& project_display_name) {
  if (project_display_name.empty()) {
    return eastl::string(k_editor_product_name);
  }
  eastl::string wordmark(k_editor_product_name);
  wordmark += k_editor_title_separator;
  wordmark += project_display_name;
  return wordmark;
}

eastl::string formatEditorWindowTitle(
    const eastl::string& project_display_name,
    const eastl::string& virtual_path, bool dirty) {
  eastl::string title;
  if (!project_display_name.empty()) {
    title = project_display_name;
  }
  const eastl::string scene = sceneShortNameFromVirtualPath(virtual_path);
  if (!scene.empty()) {
    if (!title.empty()) {
      title += k_editor_title_separator;
    }
    title += scene;
    if (dirty) {
      title.push_back('*');
    }
  }
  if (!title.empty()) {
    title += k_editor_title_separator;
  }
  title += k_editor_product_name;
  return title;
}

eastl::string projectDisplayNameFromRoot(const fs::path& root) {
  ProjectInfo info;
  if (readProjectFile(root, info) && !info.name.empty()) {
    return info.name;
  }
  return folderNameFromRoot(root);
}

}  // namespace Blunder
