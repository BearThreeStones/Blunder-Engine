#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/ui/docking/dock_manager.h"
#include "runtime/function/ui/docking/dock_node.h"
#include "runtime/function/ui/docking/dock_widget.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/project/editor_session_restore.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

bool nearly_equal(float a, float b) { return std::fabs(a - b) < 0.001f; }

fs::path makeTempRoot() {
  const fs::path root = fs::temp_directory_path() / "blunder_editor_session_restore_test";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root, ec);
  return root;
}

bool hasPanel(const Blunder::DockManager& manager, Blunder::DockPanelKind kind) {
  return manager.findWidgetByPanelKind(kind) != nullptr;
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();

  expect_true(
      "--scene wins over remembered GUID",
      resolveWindowedLiveScenePath("assets/Scenes/cli.scene.asset", "guid-a",
                                   "assets/Scenes/remembered.scene.asset",
                                   "assets/Scenes/env.scene.asset",
                                   "assets/Scenes/pick_test.scene.asset") ==
          "assets/Scenes/cli.scene.asset");
  expect_true(
      "remembered GUID beats env and compiled default",
      resolveWindowedLiveScenePath("", "guid-a", "assets/Scenes/remembered.scene.asset",
                                   "assets/Scenes/env.scene.asset",
                                   "assets/Scenes/pick_test.scene.asset") ==
          "assets/Scenes/remembered.scene.asset");
  expect_true(
      "missing GUID falls through to env",
      resolveWindowedLiveScenePath("", "guid-a", "", "assets/Scenes/env.scene.asset",
                                   "assets/Scenes/pick_test.scene.asset") ==
          "assets/Scenes/env.scene.asset");
  expect_true("compiled default is last",
              resolveWindowedLiveScenePath("", "", "", "",
                                           "assets/Scenes/pick_test.scene.asset") ==
                  "assets/Scenes/pick_test.scene.asset");

  {
    DockManager manager;
    manager.injectMissingDefaultPanels();
    expect_true("default inject has viewport",
                hasPanel(manager, DockPanelKind::viewport));
    expect_true("default inject has hierarchy",
                hasPanel(manager, DockPanelKind::hierarchy));
    expect_true("default inject has inspector",
                hasPanel(manager, DockPanelKind::inspector));
    expect_true("default inject has browser",
                hasPanel(manager, DockPanelKind::content_browser));
    expect_true("default inject has console", hasPanel(manager, DockPanelKind::console));
    expect_true("default inject has animation",
                hasPanel(manager, DockPanelKind::animation));
    expect_true("animation split ratio is 0.86",
                manager.root() && manager.root()->isSplit() &&
                    nearly_equal(manager.root()->splitRatio(), 0.86f));
  }

  {
    DockManager source;
    source.injectMissingDefaultPanels();
    if (source.root() && source.root()->isSplit()) {
      source.root()->setSplitRatio(0.42f);
    }
    const auto browser = source.findWidgetByPanelKind(DockPanelKind::content_browser);
    expect_true("browser exists before activate", browser != nullptr);
    if (browser) {
      source.activateWidget(browser->id());
    }
    const DockLayoutSnapshot snap = source.captureLayoutSnapshot();
    DockManager restored;
    expect_true("apply snapshot", restored.applyLayoutSnapshot(snap));
    expect_true("restored split ratio",
                restored.root() && restored.root()->isSplit() &&
                    nearly_equal(restored.root()->splitRatio(), 0.42f));
    const auto restored_browser =
        restored.findWidgetByPanelKind(DockPanelKind::content_browser);
    const auto restored_console = restored.findWidgetByPanelKind(DockPanelKind::console);
    expect_true("restored browser+console share a container",
                restored_browser && restored_console &&
                    restored_browser->ownerContainer() ==
                        restored_console->ownerContainer());
    if (restored_browser && restored_browser->ownerContainer()) {
      expect_true("active tab is content browser",
                  restored_browser->ownerContainer()->activeWidget() == restored_browser);
    }
  }

  {
    DockManager source;
    auto viewport = source.createWidget("Viewport", DockPanelKind::viewport);
    source.dockToRoot(viewport, DockSlot::center);
    source.injectMissingDefaultPanels();
    expect_true("inject keeps existing viewport",
                source.findWidgetByPanelKind(DockPanelKind::viewport) == viewport);
    expect_true("inject adds inspector beside viewport-only layout",
                hasPanel(source, DockPanelKind::inspector));
  }

  {
    DockLayoutSnapshot snap;
    snap.root.kind = DockNodeKind::container;
    snap.root.widgets.push_back(DockPanelKind::viewport);
    DockFloatingSnapshot floating;
    floating.rect = makeDockRect(12.0f, 24.0f, 320.0f, 220.0f);
    floating.native = false;
    floating.content.kind = DockNodeKind::container;
    floating.content.widgets.push_back(DockPanelKind::inspector);
    snap.floating.push_back(floating);
    DockAutoHideSnapshot hide;
    hide.kind = DockPanelKind::hierarchy;
    hide.edge = DockEdge::left;
    hide.expanded = true;
    hide.expanded_span = 310.0f;
    snap.auto_hide.push_back(hide);

    DockManager restored;
    expect_true("apply floating+auto-hide", restored.applyLayoutSnapshot(snap));
    expect_true("one floating node", restored.floatingNodes().size() == 1);
    if (!restored.floatingNodes().empty()) {
      expect_true("floating rect x",
                  nearly_equal(restored.floatingNodes()[0]->floatingRect().x, 12.0f));
    }
    const auto inspector = restored.findWidgetByPanelKind(DockPanelKind::inspector);
    const auto hierarchy = restored.findWidgetByPanelKind(DockPanelKind::hierarchy);
    expect_true("inspector is floating not auto-hide",
                inspector && !inspector->isAutoHide());
    expect_true("hierarchy is auto-hide on left",
                hierarchy && hierarchy->isAutoHide() &&
                    hierarchy->autoHideEdge() == DockEdge::left);
    restored.injectMissingDefaultPanels();
    expect_true("inject does not duplicate inspector",
                restored.findWidgetByPanelKind(DockPanelKind::inspector) == inspector);
    expect_true("inject adds browser into restored layout",
                hasPanel(restored, DockPanelKind::content_browser));
  }

  {
    const fs::path root = makeTempRoot();
    FileSystem file_system;
    FileSystemInitInfo fs_init;
    fs_init.project_root = root;
    file_system.initialize(fs_init);
    const eastl::string guid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    expect_true("persist guid", persistEditorSessionRestore(file_system, &guid, nullptr));

    DockManager manager;
    manager.injectMissingDefaultPanels();
    if (manager.root() && manager.root()->isSplit()) {
      manager.root()->setSplitRatio(0.33f);
    }
    const DockLayoutSnapshot dock = manager.captureLayoutSnapshot();
    expect_true("persist dock keeps guid",
                persistEditorSessionRestore(file_system, nullptr, &dock));

    EditorSessionRestoreRecord loaded;
    expect_true("load merged record",
                loadProjectEditorSessionRestore(file_system, loaded));
    expect_true("guid survived dock write", loaded.last_live_guid == guid);
    expect_true("dock present", loaded.has_dock);
    expect_true("loaded split ratio",
                loaded.dock.root.kind == DockNodeKind::split &&
                    nearly_equal(loaded.dock.root.split_ratio, 0.33f));

    const eastl::string later = "ffffffff-0000-1111-2222-333333333333";
    expect_true("last guid write wins",
                persistEditorSessionRestore(file_system, &later, nullptr));
    EditorSessionRestoreRecord after;
    expect_true("reload after last write",
                loadProjectEditorSessionRestore(file_system, after));
    expect_true("last guid stored", after.last_live_guid == later);
    expect_true("dock still present after guid-only write", after.has_dock);

    const fs::path yaml_path = editorSessionRestorePath(file_system);
    {
      std::ofstream junk(yaml_path, std::ios::trunc);
      junk << "not: [ yaml: : :\n";
    }
    EditorSessionRestoreRecord bad;
    expect_true("unreadable restore fails closed",
                !loadEditorSessionRestore(yaml_path, bad) && bad.last_live_guid.empty() &&
                    !bad.has_dock);

    std::error_code ec;
    fs::remove_all(root, ec);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
