#include "runtime/function/ui/docking/dock_widget.h"

namespace Blunder {

DockWidgetFeature DockWidget::defaultFeaturesFor(DockPanelKind panel_kind) {
  if (panel_kind == DockPanelKind::viewport) {
    return DockWidgetFeature::none;
  }
  return DockWidgetFeature::pinnable;
}

DockWidget::DockWidget(DockId id, eastl::string title, DockPanelKind panel_kind)
    : m_id(id),
      m_title(std::move(title)),
      m_panel_kind(panel_kind),
      m_features(defaultFeaturesFor(panel_kind)) {}

void DockWidget::setAutoHideState(bool auto_hide, DockEdge edge) {
  m_auto_hide = auto_hide;
  m_auto_hide_edge = edge;
}

}
