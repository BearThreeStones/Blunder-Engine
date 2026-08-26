#pragma once

#include "runtime/core/log/console_ring.h"

#include <memory>
#include <string>

namespace Blunder {

template <typename Ui>
void readConsoleSettingsFromUi(Ui& ui) {
  ConsoleViewSettings& settings = consoleViewSettings();
  settings.collapse = ui.get_console_collapse();
  settings.clear_on_play = ui.get_console_clear_on_play();
  settings.error_pause = ui.get_console_error_pause();
  settings.show_log = ui.get_console_filter_log();
  settings.show_warning = ui.get_console_filter_warning();
  settings.show_error = ui.get_console_filter_error();
  settings.search = std::string(ui.get_console_search_text().data());
  settings.selected = ui.get_console_selected_index();
}

}  // namespace Blunder
