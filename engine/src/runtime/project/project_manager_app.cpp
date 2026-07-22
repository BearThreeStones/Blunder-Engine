#include "runtime/project/project_manager_app.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/slint/slint_system.h"
#include "runtime/platform/window/window_system.h"
#include "runtime/project/project_list.h"
#include "runtime/project/project_relaunch.h"

#include <SDL3/SDL_dialog.h>

#include <string>

namespace Blunder {

namespace {

struct FolderBrowseUserData {
  ProjectManagerApp* app{nullptr};
  bool for_create{true};
};

}  // namespace

ProjectManagerApp::ProjectManagerApp() = default;

ProjectManagerApp::~ProjectManagerApp() { shutdown(); }

bool ProjectManagerApp::start() {
  // LOG_* macros resolve through g_runtime_global_context — register before any
  // WindowSystem / SlintSystem call that may log.
  m_logger = eastl::make_shared<LogSystem>();
  g_runtime_global_context.m_logger_system = m_logger;

  WindowCreateInfo window_info{};
  window_info.width = 960;
  window_info.height = 640;
  window_info.title = "Blunder Project Manager";
  m_window_system = eastl::make_shared<WindowSystem>();
  m_window_system->initialize(window_info);

  m_controller = eastl::make_unique<ProjectManagerController>(
      defaultProjectListStorePath());
  m_controller->load();

  m_slint = eastl::make_shared<SlintSystem>();
  SlintSystemInitInfo slint_init{};
  slint_init.window_system = m_window_system.get();
  slint_init.project_manager_mode = true;
  m_slint->initialize(slint_init);
  if (!m_slint->isProjectManagerMode()) {
    LOG_ERROR("[ProjectManagerApp] Slint Project Manager UI failed to start");
    return false;
  }

  wireCallbacks();
  refreshListUi();
  setStatus("");
  return true;
}

void ProjectManagerApp::shutdown() {
  if (m_slint) {
    m_slint->shutdown();
    m_slint.reset();
  }
  if (m_window_system) {
    m_window_system->shutdown();
    m_window_system.reset();
  }
  m_controller.reset();
  g_runtime_global_context.m_logger_system.reset();
  m_logger.reset();
}

SDL_AppResult ProjectManagerApp::tick() {
  applyPendingBrowseResult();

  if (m_wants_relaunch) {
    const std::filesystem::path editor = resolveEditorExecutablePath();
    if (!relaunchEditorWithProject(m_relaunch_root)) {
      eastl::string msg = "Failed to start engine_editor";
      if (!editor.empty()) {
        msg += " at ";
        msg += editor.generic_string().c_str();
      }
      msg += " (build engine_editor into the same bin/<Config> folder)";
      setStatus(msg.c_str());
      m_wants_relaunch = false;
      m_relaunch_root.clear();
      return SDL_APP_CONTINUE;
    }
    return SDL_APP_SUCCESS;
  }

  if (m_window_system && m_window_system->shouldClose()) {
    return SDL_APP_SUCCESS;
  }

  if (m_slint) {
    // Check before update(): beginFrame timers may request redraw during update.
    const bool had_pending_redraw = m_slint->projectManagerNeedsRedraw();
    m_slint->update();
    if (!had_pending_redraw) {
      SDL_Delay(8);
    }
  }
  return SDL_APP_CONTINUE;
}

void ProjectManagerApp::processSdlEvent(const SDL_Event& event) {
  if (m_window_system) {
    m_window_system->dispatchApplicationEvent(event, false);
  }
}

void ProjectManagerApp::refreshListUi() {
  if (!m_slint || !m_controller) {
    return;
  }
  m_controller->refreshMissing();
  eastl::vector<slint::SharedString> names;
  eastl::vector<slint::SharedString> paths;
  eastl::vector<bool> missing;
  eastl::vector<slint::SharedString> last_opened;
  names.reserve(m_controller->entries().size());
  paths.reserve(m_controller->entries().size());
  missing.reserve(m_controller->entries().size());
  last_opened.reserve(m_controller->entries().size());
  for (const ProjectListEntry& entry : m_controller->entries()) {
    names.push_back(slint::SharedString(entry.name.c_str()));
    paths.push_back(slint::SharedString(entry.path.generic_string().c_str()));
    missing.push_back(entry.missing);
    last_opened.push_back(slint::SharedString(
        formatProjectLastOpenedDisplay(entry.last_opened_unix).c_str()));
  }
  m_slint->setProjectManagerRows(names, paths, missing, last_opened);
  if (m_selected_index >= 0 &&
      m_selected_index < static_cast<int>(m_controller->entries().size())) {
    m_slint->setProjectManagerSelectedIndex(m_selected_index);
  } else {
    m_selected_index = -1;
    m_slint->setProjectManagerSelectedIndex(-1);
  }
}

void ProjectManagerApp::setStatus(const char* text) {
  if (m_slint) {
    m_slint->setProjectManagerStatus(slint::SharedString(text ? text : ""));
  }
}

void ProjectManagerApp::wireCallbacks() {
  if (!m_slint) {
    return;
  }
  m_slint->setProjectManagerCallbacks(
      [this]() { onOpen(); }, [this]() { onRemove(); },
      [this]() { onShowCreate(); }, [this]() { onShowImport(); },
      [this]() { onCreateBrowse(); }, [this]() { onCreateConfirm(); },
      [this]() { onCreateCancel(); }, [this]() { onImportBrowse(); },
      [this]() { onImportConfirm(); }, [this]() { onImportCancel(); },
      [this](int index) { onSelection(index); });
}

void ProjectManagerApp::onOpen() {
  if (!m_controller) {
    return;
  }
  // Prefer UI selected-index (property) over cached C++ index — TouchArea may
  // update the property even if selection-changed was missed.
  if (m_slint) {
    const int ui_index = m_slint->projectManagerSelectedIndex();
    if (ui_index >= 0) {
      m_selected_index = ui_index;
    }
  }
  if (m_selected_index < 0) {
    setStatus("Select a project to open");
    return;
  }
  const eastl::string error =
      m_controller->openEntry(static_cast<size_t>(m_selected_index));
  if (!error.empty()) {
    setStatus(error.c_str());
    refreshListUi();
    return;
  }
  requestRelaunch(m_controller->pendingOpenRoot());
}

void ProjectManagerApp::onRemove() {
  if (!m_controller || m_selected_index < 0) {
    return;
  }
  if (!m_controller->removeEntry(static_cast<size_t>(m_selected_index))) {
    setStatus("Failed to remove project from list");
    return;
  }
  m_selected_index = -1;
  refreshListUi();
  setStatus("Removed from list (files kept on disk)");
}

void ProjectManagerApp::onShowCreate() {
  if (!m_slint) {
    return;
  }
  m_slint->setProjectManagerCreateName(slint::SharedString("New Project"));
  const std::string projects_dir =
      defaultProjectsDirectory().generic_string();
  m_slint->setProjectManagerCreatePath(
      slint::SharedString(projects_dir.c_str()));
  m_slint->setProjectManagerCreateDialogVisible(true);
}

void ProjectManagerApp::onShowImport() {
  if (!m_slint) {
    return;
  }
  m_slint->setProjectManagerImportPath(slint::SharedString(""));
  m_slint->setProjectManagerImportDialogVisible(true);
}

void ProjectManagerApp::onCreateBrowse() { browseFolder(true); }

void ProjectManagerApp::onCreateConfirm() {
  if (!m_slint || !m_controller) {
    return;
  }
  CreateProjectRequest request;
  request.name = m_slint->projectManagerCreateName().data();
  request.target_path =
      std::filesystem::path(m_slint->projectManagerCreatePath().data());
  request.create_folder = m_slint->projectManagerCreateFolder();
  eastl::string error;
  if (!m_controller->createProject(request, error)) {
    setStatus(error.empty() ? "Create failed" : error.c_str());
    return;
  }
  m_slint->setProjectManagerCreateDialogVisible(false);
  refreshListUi();
  requestRelaunch(m_controller->pendingOpenRoot());
}

void ProjectManagerApp::onCreateCancel() {
  if (m_slint) {
    m_slint->setProjectManagerCreateDialogVisible(false);
  }
}

void ProjectManagerApp::onImportBrowse() { browseFolder(false); }

void ProjectManagerApp::onImportConfirm() {
  if (!m_slint || !m_controller) {
    return;
  }
  const std::filesystem::path path(m_slint->projectManagerImportPath().data());
  eastl::string error;
  if (!m_controller->importProject(path, error)) {
    setStatus(error.empty() ? "Import failed" : error.c_str());
    return;
  }
  m_slint->setProjectManagerImportDialogVisible(false);
  refreshListUi();
  requestRelaunch(m_controller->pendingOpenRoot());
}

void ProjectManagerApp::onImportCancel() {
  if (m_slint) {
    m_slint->setProjectManagerImportDialogVisible(false);
  }
}

void ProjectManagerApp::onSelection(int index) { m_selected_index = index; }

void ProjectManagerApp::requestRelaunch(const std::filesystem::path& root) {
  m_relaunch_root = root;
  m_wants_relaunch = true;
}

void ProjectManagerApp::browseFolder(bool for_create) {
  if (!m_window_system) {
    return;
  }
  auto* userdata = new FolderBrowseUserData{this, for_create};
  m_browse_default_location = defaultProjectsDirectory().string();
  SDL_ShowOpenFolderDialog(
      [](void* ud, const char* const* filelist, int /*filter*/) {
        auto* data = static_cast<FolderBrowseUserData*>(ud);
        if (data == nullptr) {
          return;
        }
        ProjectManagerApp* app = data->app;
        const bool create = data->for_create;
        delete data;
        if (app == nullptr) {
          return;
        }
        // Callback runs on SDL's dialog worker thread — do not touch Slint here.
        if (filelist == nullptr || filelist[0] == nullptr) {
          return;
        }
        std::lock_guard<std::mutex> lock(app->m_browse_mutex);
        app->m_browse_for_create = create;
        app->m_browse_path = filelist[0];
        app->m_browse_result_pending = true;
      },
      userdata, m_window_system->getNativeWindow(),
      m_browse_default_location.c_str(), false);
}

void ProjectManagerApp::applyPendingBrowseResult() {
  bool for_create = true;
  std::string path;
  {
    std::lock_guard<std::mutex> lock(m_browse_mutex);
    if (!m_browse_result_pending) {
      return;
    }
    for_create = m_browse_for_create;
    path = m_browse_path;
    m_browse_result_pending = false;
    m_browse_path.clear();
  }
  if (!m_slint || path.empty()) {
    return;
  }
  const slint::SharedString shared(path.c_str());
  if (for_create) {
    m_slint->setProjectManagerCreatePath(shared);
  } else {
    m_slint->setProjectManagerImportPath(shared);
  }
}

}  // namespace Blunder
