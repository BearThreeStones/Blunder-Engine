#include "runtime/function/ui/docking/dock_widget.h"

namespace Blunder {

DockWidget::DockWidget(DockId id, eastl::string title, DockPanelKind panel_kind)
    : m_id(id), m_title(std::move(title)), m_panel_kind(panel_kind) {}

}
