#include "runtime/core/log/console_ring.h"

#include <cstdio>
#include <optional>
#include <string>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

/// Mirrors LogSystem::mapToConsoleSeverity (0=debug … 4=fatal).
std::optional<Blunder::ConsoleSeverity> mapLevel(int level) {
  switch (level) {
    case 1:
      return Blunder::ConsoleSeverity::Log;
    case 2:
      return Blunder::ConsoleSeverity::Warning;
    case 3:
    case 4:
      return Blunder::ConsoleSeverity::Error;
    default:
      return std::nullopt;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  ConsoleRing& ring = ConsoleRing::instance();
  ring.clear();
  ring.setForwardEnabled(false);

  expect_true("debug maps to none", !mapLevel(0).has_value());
  expect_true("info maps to Log", mapLevel(1) == ConsoleSeverity::Log);
  expect_true("warn maps to Warning",
              mapLevel(2) == ConsoleSeverity::Warning);
  expect_true("error maps to Error", mapLevel(3) == ConsoleSeverity::Error);
  expect_true("fatal maps to Error", mapLevel(4) == ConsoleSeverity::Error);

  ring.append(ConsoleSeverity::Log, ConsoleOrigin::EditorSession, "hello");
  expect_true("info append size 1", ring.size() == 1);
  {
    const auto snap = ring.snapshot();
    expect_true("info severity Log",
                !snap.empty() && snap[0].severity == ConsoleSeverity::Log);
    expect_true("info text", !snap.empty() && snap[0].text == "hello");
  }

  ring.clear();
  for (size_t i = 0; i < kConsoleCapacity + 1; ++i) {
    ring.append(ConsoleSeverity::Log, ConsoleOrigin::EditorSession,
                "msg-" + std::to_string(i));
  }
  expect_true("cap size 10000", ring.size() == kConsoleCapacity);
  {
    const auto snap = ring.snapshot();
    expect_true("oldest dropped",
                !snap.empty() && snap.front().text == "msg-1");
    expect_true(
        "newest kept",
        !snap.empty() &&
            snap.back().text == "msg-" + std::to_string(kConsoleCapacity));
  }

  ring.clear();
  ring.append(ConsoleSeverity::Log, ConsoleOrigin::EditorSession, "editor");
  ring.append(ConsoleSeverity::Warning, ConsoleOrigin::PlayProcess, "player");
  expect_true("both origins present", ring.size() == 2);
  ring.clear();
  expect_true("clear empties", ring.size() == 0);

  {
    ConsoleViewSettings settings;
    std::vector<ConsoleMessage> input;
    ConsoleMessage a;
    a.severity = ConsoleSeverity::Log;
    a.origin = ConsoleOrigin::EditorSession;
    a.text = "hello";
    a.unix_ms = 1;
    ConsoleMessage b = a;
    b.severity = ConsoleSeverity::Warning;
    b.text = "warn-me";
    ConsoleMessage c = a;
    c.severity = ConsoleSeverity::Error;
    c.text = "err-me";
    input.push_back(a);
    input.push_back(b);
    input.push_back(c);
    settings.show_warning = false;
    settings.show_error = false;
    const auto rows = buildConsoleVisibleRows(input, settings);
    expect_true("filter keeps log only",
                rows.size() == 1 && rows[0].message.text == "hello");
  }

  {
    ConsoleViewSettings settings;
    settings.search = "WARN";
    std::vector<ConsoleMessage> input;
    ConsoleMessage a;
    a.text = "hello";
    ConsoleMessage b;
    b.text = "Please Warn The Author";
    input.push_back(a);
    input.push_back(b);
    const auto rows = buildConsoleVisibleRows(input, settings);
    expect_true("search case-insensitive",
                rows.size() == 1 && rows[0].message.text == b.text);
  }

  {
    ConsoleViewSettings settings;
    settings.collapse = true;
    std::vector<ConsoleMessage> input;
    ConsoleMessage a;
    a.severity = ConsoleSeverity::Log;
    a.origin = ConsoleOrigin::EditorSession;
    a.text = "same";
    a.stack = "s";
    a.unix_ms = 10;
    ConsoleMessage b = a;
    b.unix_ms = 20;
    ConsoleMessage other = a;
    other.origin = ConsoleOrigin::PlayProcess;
    other.unix_ms = 30;
    input.push_back(a);
    input.push_back(b);
    input.push_back(other);
    const auto rows = buildConsoleVisibleRows(input, settings);
    expect_true("collapse merges same origin key", rows.size() == 2);
    expect_true("collapse latest time",
                rows[0].count == 2 && rows[0].message.unix_ms == 20);
    expect_true("collapse does not merge origins",
                rows[1].message.origin == ConsoleOrigin::PlayProcess &&
                    rows[1].count == 1);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("console_ring_test: all passed\n");
  return 0;
}
