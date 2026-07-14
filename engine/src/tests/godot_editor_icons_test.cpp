#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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

fs::path repo_root() {
  return fs::path(BLUNDER_REPO_ROOT);
}

}  // namespace

int main() {
  const fs::path icons_dir = repo_root() / "engine" / "3rdparty" / "godot-icons";
  const fs::path slint_dir =
      repo_root() / "engine" / "src" / "runtime" / "function" / "slint";

  const char* required_svgs[] = {
      "GuiClose.svg",
      "Pin.svg",
      "Search.svg",
      "Reload.svg",
      "Folder.svg",
      "GuiTreeArrowRight.svg",
      "GuiTreeArrowDown.svg",
      "Instance.svg",
      "Unlinked.svg",
  };

  expect_true("godot-icons directory exists", fs::is_directory(icons_dir));
  expect_true("LICENSE exists", fs::is_regular_file(icons_dir / "LICENSE"));
  expect_true("README.md exists", fs::is_regular_file(icons_dir / "README.md"));

  for (const char* name : required_svgs) {
    const bool ok = fs::is_regular_file(icons_dir / name);
    if (!ok) {
      std::fprintf(stderr, "FAIL missing required SVG: %s\n", name);
      ++g_failures;
    }
  }

  int svg_count = 0;
  if (fs::is_directory(icons_dir)) {
    for (const auto& entry : fs::directory_iterator(icons_dir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".svg") {
        ++svg_count;
      }
    }
  }
  expect_true("vendored pack has at least 1000 SVGs", svg_count >= 1000);

  const std::string dock = read_file(slint_dir / "dock_chrome_icons.slint");
  expect_true("dock_chrome_icons.slint readable", !dock.empty());
  expect_true("DockChromeCloseIcon uses GuiClose.svg",
              contains(dock, "GuiClose.svg"));
  expect_true("DockChromePinIcon uses Pin.svg", contains(dock, "Pin.svg"));
  expect_true("dock chrome close/pin use colorize", contains(dock, "colorize:"));
  expect_true("DockChromeMinimizeIcon export remains",
              contains(dock, "export component DockChromeMinimizeIcon"));
  expect_true("minimize is not a Godot SVG image-url",
              !contains(dock, "DockChromeMinimizeIcon") ||
                  [&] {
                    const auto min_pos = dock.find("export component DockChromeMinimizeIcon");
                    if (min_pos == std::string::npos) {
                      return false;
                    }
                    const auto next_export = dock.find("export component", min_pos + 1);
                    const std::string_view min_body =
                        next_export == std::string::npos
                            ? std::string_view{dock}.substr(min_pos)
                            : std::string_view{dock}.substr(min_pos, next_export - min_pos);
                    return !contains(min_body, "@image-url");
                  }());

  const std::string editor_icons = read_file(slint_dir / "editor_icons.slint");
  expect_true("editor_icons.slint exists", !editor_icons.empty());
  for (const char* needle : {
           "export component EditorIconSearch",
           "export component EditorIconReload",
           "export component EditorIconFolder",
           "export component EditorIconTreeArrowRight",
           "export component EditorIconTreeArrowDown",
           "export component EditorIconInstance",
           "export component EditorIconUnlinked",
           "Search.svg",
           "Reload.svg",
           "Folder.svg",
           "GuiTreeArrowRight.svg",
           "GuiTreeArrowDown.svg",
           "Instance.svg",
           "Unlinked.svg",
           "colorize:",
       }) {
    if (!contains(editor_icons, needle)) {
      std::fprintf(stderr, "FAIL editor_icons.slint missing: %s\n", needle);
      ++g_failures;
    }
  }

  const std::string browser = read_file(slint_dir / "content_browser.slint");
  expect_true("content_browser imports editor_icons",
              contains(browser, "editor_icons.slint"));
  expect_true("content_browser uses EditorIconSearch",
              contains(browser, "EditorIconSearch"));
  expect_true("content_browser uses EditorIconReload",
              contains(browser, "EditorIconReload"));
  expect_true("content_browser uses EditorIconFolder",
              contains(browser, "EditorIconFolder"));
  expect_true("content_browser uses tree arrow icons",
              contains(browser, "EditorIconTreeArrowRight") &&
                  contains(browser, "EditorIconTreeArrowDown"));
  // UTF-8: U+1F50D search, U+27F3 refresh, U+1F4C1/U+1F4C2 folders, U+203A single right-pointing angle
  expect_true("content_browser has no search emoji",
              !contains(browser, "\xF0\x9F\x94\x8D"));
  expect_true("content_browser has no refresh unicode",
              !contains(browser, "\xE2\x9F\xB3"));
  expect_true("content_browser has no folder emoji",
              !contains(browser, "\xF0\x9F\x93\x81") &&
                  !contains(browser, "\xF0\x9F\x93\x82"));
  expect_true("content_browser has no breadcrumb angle quote",
              !contains(browser, "\xE2\x80\xBA"));

  const std::string inspector = read_file(slint_dir / "inspector_panel.slint");
  expect_true("inspector imports editor_icons",
              contains(inspector, "editor_icons.slint"));
  expect_true("inspector uses EditorIconInstance",
              contains(inspector, "EditorIconInstance"));
  expect_true("inspector uses EditorIconUnlinked",
              contains(inspector, "EditorIconUnlinked"));
  expect_true("inspector uses tree arrow icons for section",
              contains(inspector, "EditorIconTreeArrowRight") &&
                  contains(inspector, "EditorIconTreeArrowDown"));
  // UTF-8 U+26D3 chains
  expect_true("inspector has no scale-link chain emoji",
              !contains(inspector, "\xE2\x9B\x93"));

  if (g_failures == 0) {
    std::printf("godot_editor_icons_test: all passed (%d svgs)\n", svg_count);
    return 0;
  }
  std::fprintf(stderr, "godot_editor_icons_test: %d failure(s)\n", g_failures);
  return 1;
}
