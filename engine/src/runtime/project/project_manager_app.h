#pragma once

#include "EASTL/shared_ptr.h"
#include "EASTL/unique_ptr.h"

#include "runtime/project/project_manager_controller.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <mutex>
#include <string>

namespace Blunder {

class LogSystem;
class WindowSystem;
class SlintSystem;

/// Lightweight Project Manager host: WindowSystem + Slint PM UI + controller.
/// Does not start cook/scene/thumbnail systems.
class ProjectManagerApp final {
 public:
  ProjectManagerApp();
  ~ProjectManagerApp();

  bool start();
  void shutdown();

  SDL_AppResult tick();
  void processSdlEvent(const SDL_Event& event);

  bool wantsRelaunch() const { return m_wants_relaunch; }
  const std::filesystem::path& relaunchRoot() const { return m_relaunch_root; }

 private:
  void refreshListUi();
  void setStatus(const char* text);
  void wireCallbacks();
  void onOpen();
  void onRemove();
  void onShowCreate();
  void onShowImport();
  void onCreateBrowse();
  void onCreateConfirm();
  void onCreateCancel();
  void onImportBrowse();
  void onImportConfirm();
  void onImportCancel();
  void onSelection(int index);
  void requestRelaunch(const std::filesystem::path& root);
  void browseFolder(bool for_create);
  void applyPendingBrowseResult();

  eastl::shared_ptr<LogSystem> m_logger;
  eastl::shared_ptr<WindowSystem> m_window_system;
  eastl::shared_ptr<SlintSystem> m_slint;
  eastl::unique_ptr<ProjectManagerController> m_controller;

  bool m_wants_relaunch{false};
  std::filesystem::path m_relaunch_root;
  int m_selected_index{-1};

  // SDL folder dialogs complete on a worker thread; apply path on the UI thread.
  std::mutex m_browse_mutex;
  bool m_browse_result_pending{false};
  bool m_browse_for_create{true};
  std::string m_browse_path;
  /// Kept alive for the async SDL dialog default-location pointer.
  std::string m_browse_default_location;
};

}  // namespace Blunder
