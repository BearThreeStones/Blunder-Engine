#include "runtime/function/ui/active_scene_display.h"

#include "EASTL/string.h"
#include "runtime/project/project_file.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

namespace fs = std::filesystem;

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

fs::path makeTempDir(const char* tag) {
  const fs::path root =
      fs::temp_directory_path() / ("blunder_scene_display_" + std::string(tag));
  fs::remove_all(root);
  fs::create_directories(root);
  return root;
}

eastl::string expectedWordmark(const char* project_name) {
  eastl::string s(Blunder::k_editor_product_name);
  s += Blunder::k_editor_title_separator;
  s += project_name;
  return s;
}

}  // namespace

int main() {
  using namespace Blunder;

  expect_true("wordmark uses project display name",
              formatApplicationBarWordmark("Test") == expectedWordmark("Test"));
  expect_true("empty name keeps product wordmark",
              formatApplicationBarWordmark("") == k_editor_product_name);

  eastl::string dirty_title("Test");
  dirty_title += k_editor_title_separator;
  dirty_title += "pick_test*";
  dirty_title += k_editor_title_separator;
  dirty_title += k_editor_product_name;
  expect_true(
      "title with scene and dirty star",
      formatEditorWindowTitle("Test", "assets/Scenes/pick_test.scene.asset",
                              true) == dirty_title);

  eastl::string clean_title("Test");
  clean_title += k_editor_title_separator;
  clean_title += "pick_test";
  clean_title += k_editor_title_separator;
  clean_title += k_editor_product_name;
  expect_true(
      "title with scene clean",
      formatEditorWindowTitle("Test", "assets/Scenes/pick_test.scene.asset",
                              false) == clean_title);

  eastl::string no_scene("Test");
  no_scene += k_editor_title_separator;
  no_scene += k_editor_product_name;
  expect_true("title without scene",
              formatEditorWindowTitle("Test", "", false) == no_scene);
  expect_true("title without project or scene",
              formatEditorWindowTitle("", "", false) == k_editor_product_name);
  expect_true(
      "hierarchy still scene-only",
      formatHierarchySceneLabel("assets/Scenes/pick_test.scene.asset", true) ==
          "pick_test*");

  {
    const fs::path root = makeTempDir("named");
    expect_true("write project file", writeProjectFile(root, "Demo Game"));
    expect_true("display name from Project File",
                projectDisplayNameFromRoot(root) == "Demo Game");
  }

  {
    const fs::path root = makeTempDir("missing_file") / "FallbackFolder";
    fs::create_directories(root);
    expect_true("missing Project File uses folder name",
                projectDisplayNameFromRoot(root) == "FallbackFolder");
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
