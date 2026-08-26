#include "runtime/function/slint/slint_system.h"

#include <slint.h>

#include "editor_window.h"

#include <exception>
#include <memory>
#include <vector>

#include "EASTL/string.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/log/console_ring.h"

namespace Blunder {

void SlintSystem::syncConsolePanel() {
  if (!m_window_component) {
    return;
  }
  try {
    auto& ui = *m_window_component->operator->();
    ConsoleRing& ring = ConsoleRing::instance();
    ConsoleViewSettings& settings = consoleViewSettings();
    settings.collapse = ui.get_console_collapse();
    settings.clear_on_play = ui.get_console_clear_on_play();
    settings.error_pause = ui.get_console_error_pause();
    settings.show_log = ui.get_console_filter_log();
    settings.show_warning = ui.get_console_filter_warning();
    settings.show_error = ui.get_console_filter_error();
    settings.search = ui.get_console_search_text().data();
    settings.selected = ui.get_console_selected_index();

    const uint64_t generation = ring.generation();
    if (generation == m_console_panel_generation &&
        settings.collapse == m_console_panel_collapse &&
        settings.show_log == m_console_panel_filter_log &&
        settings.show_warning == m_console_panel_filter_warning &&
        settings.show_error == m_console_panel_filter_error &&
        eastl::string(settings.search.c_str()) == m_console_panel_search) {
      return;
    }
    m_console_panel_generation = generation;
    m_console_panel_collapse = settings.collapse;
    m_console_panel_filter_log = settings.show_log;
    m_console_panel_filter_warning = settings.show_warning;
    m_console_panel_filter_error = settings.show_error;
    m_console_panel_search = settings.search.c_str();

    size_t count_log = 0;
    size_t count_warning = 0;
    size_t count_error = 0;
    ring.severityCounts(count_log, count_warning, count_error);
    ui.set_console_count_log(static_cast<int>(count_log));
    ui.set_console_count_warning(static_cast<int>(count_warning));
    ui.set_console_count_error(static_cast<int>(count_error));

    const std::vector<ConsoleVisibleRow> visible =
        buildConsoleVisibleRows(ring.snapshot(), settings);
    auto model = std::make_shared<slint::VectorModel<ConsoleRow>>();
    for (size_t i = 0; i < visible.size(); ++i) {
      const ConsoleVisibleRow& row = visible[i];
      ConsoleRow slint_row{};
      slint_row.time =
          slint::SharedString(formatConsoleTime(row.message.unix_ms).c_str());
      slint_row.severity = static_cast<int>(row.message.severity);
      slint_row.text = slint::SharedString(row.message.text.c_str());
      slint_row.stack = slint::SharedString(row.message.stack.c_str());
      slint_row.origin = static_cast<int>(row.message.origin);
      slint_row.count = static_cast<int>(row.count);
      slint_row.ring_index = static_cast<int>(i);
      model->push_back(slint_row);
    }
    ui.set_console_rows(model);

    int selected = settings.selected;
    if (selected < 0 || static_cast<size_t>(selected) >= visible.size()) {
      selected = -1;
      ui.set_console_selected_index(-1);
      ui.set_console_detail_text(slint::SharedString());
      ui.set_console_detail_stack(slint::SharedString());
    } else {
      ui.set_console_selected_index(selected);
      ui.set_console_detail_text(slint::SharedString(
          visible[static_cast<size_t>(selected)].message.text.c_str()));
      ui.set_console_detail_stack(slint::SharedString(
          visible[static_cast<size_t>(selected)].message.stack.c_str()));
    }
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::syncConsolePanel] {}", e.what());
  } catch (...) {
    LOG_ERROR("[SlintSystem::syncConsolePanel] unknown exception");
  }
}

}  // namespace Blunder
