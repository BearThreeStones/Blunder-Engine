#include "runtime/function/ui/docking/dock_layout_model.h"
#include "runtime/function/ui/docking/dock_manager.h"
#include "runtime/function/ui/docking/dock_node.h"
#include "runtime/function/ui/docking/dock_widget.h"

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

const Blunder::DockTileView* tileForPanel(const Blunder::DockLayoutModel& model,
                                          Blunder::DockPanelKind kind) {
  for (const Blunder::DockTileView& tile : model.tiles) {
    if (tile.active_panel_kind == kind) {
      return &tile;
    }
  }
  return nullptr;
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    DockManager manager;
    manager.setHostRect(makeDockRect(0.0f, 0.0f, 1920.0f, 1080.0f));

    auto viewport = manager.createWidget("Viewport", DockPanelKind::viewport);
    manager.dockToRoot(viewport, DockSlot::center);
    const auto center = viewport->ownerContainer();
    expect_true("viewport has container", center != nullptr);

    auto content =
        manager.createWidget("Content Browser", DockPanelKind::content_browser);
    manager.dockWidget(center->id(), DockSlot::bottom, content);

    auto console = manager.createWidget("Console", DockPanelKind::console);
    const auto browser_container = content->ownerContainer();
    expect_true("content browser has container", browser_container != nullptr);
    manager.dockWidget(browser_container->id(), DockSlot::center, console);

    expect_true("console shares content-browser container",
                console->ownerContainer() == content->ownerContainer());

    const DockLayoutModel before = manager.buildLayoutModel();
    expect_true("console tab is active after docking onto browser",
                tileForPanel(before, DockPanelKind::console) != nullptr);
    expect_true("content browser is not the active tab yet",
                tileForPanel(before, DockPanelKind::content_browser) == nullptr);

    manager.activateWidget(content->id());
    const DockLayoutModel after = manager.buildLayoutModel();
    expect_true("activating content browser switches the shared tile",
                tileForPanel(after, DockPanelKind::content_browser) != nullptr);
    expect_true("console is no longer the active tab",
                tileForPanel(after, DockPanelKind::console) == nullptr);
  }

  const fs::path slint_dir = fs::path(BLUNDER_REPO_ROOT) / "engine" / "src" /
                             "runtime" / "function" / "slint";
  const fs::path docking_dir = fs::path(BLUNDER_REPO_ROOT) / "engine" / "src" /
                               "runtime" / "function" / "ui" / "docking";
  const std::string editor = read_file(slint_dir / "editor_window.slint");
  const std::string floating = read_file(slint_dir / "floating_panel_window.slint");
  const std::string float_host =
      read_file(docking_dir / "dock_floating_window_host.cpp");
  expect_true("editor_window.slint readable", !editor.empty());
  expect_true("floating_panel_window.slint readable", !floating.empty());
  expect_true("dock_floating_window_host.cpp readable", !float_host.empty());

  // Switching the shared bottom tabs must hide panels, not destroy them.
  // Destroying ConsolePanel (search TextInput / two-way binds) to create
  // ContentBrowser (ContextMenuArea) crashes Slint.
  expect_true("tile keeps browser alive when console is showing",
              contains(editor, "visible: tile.active-panel-kind == 4"));
  expect_true("tile keeps console alive when browser is showing",
              contains(editor, "visible: tile.active-panel-kind == 6"));
  expect_true("overlay keeps browser alive when console is showing",
              contains(editor, "visible: overlay.panel-kind == 4"));
  expect_true("overlay keeps console alive when browser is showing",
              contains(editor, "visible: overlay.panel-kind == 6"));
  expect_true("tile does not destroy ConsolePanel on tab switch",
              !contains(editor, "if tile.active-panel-kind == 6: ConsolePanel"));
  expect_true("overlay does not destroy ConsolePanel on tab switch",
              !contains(editor, "if overlay.panel-kind == 6: ConsolePanel"));
  expect_true("float keeps browser alive when console is showing",
              contains(floating, "visible: root.panel-kind == 4"));
  expect_true("float keeps console alive when browser is showing",
              contains(floating, "visible: root.panel-kind == 6"));
  expect_true("float does not destroy ConsolePanel on tab switch",
              !contains(floating, "if root.panel-kind == 6 : ConsolePanel"));
  expect_true("native float sync updates panel-kind when the active tab changes",
              contains(float_host, "widget->panelKind() != entry->panel_kind") ||
                  contains(float_host, "widget->panelKind() != entry.panel_kind") ||
                  contains(float_host, "entry.panel_kind != widget->panelKind()") ||
                  contains(float_host, "entry->panel_kind != widget->panelKind()"));

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "dock_tab_activation_test: all passed\n");
  return 0;
}
