#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

std::string read_file(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return {};
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

bool contains(std::string_view hay, std::string_view needle) {
  return hay.find(needle) != std::string_view::npos;
}

}  // namespace

int main() {
  const fs::path repo = fs::path(BLUNDER_REPO_ROOT);
  const fs::path slint_path = repo / "engine" / "src" / "runtime" / "function" /
                              "slint" / "camera_preview_panel.slint";
  const fs::path render_path = repo / "engine" / "src" / "runtime" / "function" /
                               "render" / "render_system.cpp";

  const std::string panel = read_file(slint_path);
  const std::string render = read_file(render_path);
  expect_true("camera_preview_panel.slint readable", !panel.empty());
  expect_true("render_system.cpp readable", !render.empty());

  // Selecting a Camera must not destroy/recreate the PiP chrome. Root already
  // has visible:; a nested `if preview-visible` rebuilds the panel every switch.
  expect_true("camera preview chrome stays in the tree while hidden",
              contains(panel, "panel := Rectangle"));
  expect_true("camera preview does not destroy chrome on select",
              !contains(panel,
                        "if root.preview-visible && !root.layout-drag-active: "
                        "panel := Rectangle"));

  // Selecting a Camera already requestViewportRedraws. Forcing the main
  // viewport every idle tick while preview is up double-renders the scene.
  expect_true("idle camera preview does not force every main viewport frame",
              !contains(render, "if (shouldForceViewportForCameraPreview())"));

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "camera_preview_pacing_test: all passed\n");
  return 0;
}
