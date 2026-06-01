#pragma once

#include <functional>
#include <memory>

#include <slint.h>

#include "docking_panel.h"

#include "runtime/function/ui/docking/dock_layout_model.h"

namespace Blunder {

class DockManager;

class DockSlintBridge final {
 public:
  static std::shared_ptr<slint::VectorModel<DockTile>> buildTileModel(
      const DockLayoutModel& model);
  static std::shared_ptr<slint::VectorModel<DockTab>> buildTabModel(
      const DockLayoutModel& model);
  static std::shared_ptr<slint::VectorModel<DockSplitterHandle>> buildSplitterModel(
      const DockLayoutModel& model);
  static std::shared_ptr<slint::VectorModel<DockGuide>> buildGuideModel(
      const DockLayoutModel& model);
  static DockPreview buildPreview(const DockLayoutModel& model);

  static void applyLayout(const slint::ComponentHandle<DockingHost>& host,
                          const DockLayoutModel& model);
  static void wireCallbacks(const slint::ComponentHandle<DockingHost>& host,
                            DockManager& manager, std::function<void()> on_changed);
};

}
