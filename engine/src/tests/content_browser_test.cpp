#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/content/content_entry.h"
#include "runtime/resource/content/content_index.h"
#include "runtime/resource/content_browser/content_browser_view.h"

#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_eq_string(const char* label, const eastl::string& got, const char* want) {
  if (got != want) {
    std::fprintf(stderr, "FAIL %s: got '%s' want '%s'\n", label, got.c_str(),
                 want);
    ++g_failures;
  }
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

fs::path makeTempProject() {
  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_content_browser_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Scenes");
  fs::create_directories(root / "Resources");
  {
    std::ofstream out(root / "Assets" / "README.md", std::ios::binary);
    out << "hello\n";
  }
  {
    std::ofstream out(root / "Assets" / "Meshes" / "note.txt", std::ios::binary);
    out << "cube\n";
  }
  return root;
}

bool hasVirtualPath(const eastl::vector<Blunder::ContentEntry>& entries,
                    const char* path) {
  for (const Blunder::ContentEntry& entry : entries) {
    if (entry.virtual_path == path) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  const eastl::vector<ContentEntry> entries = ContentIndex::scan(file_system);
  expect_true("scan includes synthetic assets/ root",
              hasVirtualPath(entries, "assets/"));
  expect_true("scan directories end with slash (Meshes)",
              hasVirtualPath(entries, "assets/Meshes/"));
  expect_true("scan directories end with slash (Scenes)",
              hasVirtualPath(entries, "assets/Scenes/"));
  expect_true("scan includes file under assets/",
              hasVirtualPath(entries, "assets/README.md"));
  expect_true("scan includes nested file",
              hasVirtualPath(entries, "assets/Meshes/note.txt"));

  bool meshes_is_dir = false;
  for (const ContentEntry& entry : entries) {
    if (entry.virtual_path == "assets/Meshes/") {
      meshes_is_dir = entry.is_directory;
      break;
    }
  }
  expect_true("Meshes entry is directory", meshes_is_dir);

  expect_true(
      "folder classifies as Folder",
      classifyBrowserEntry(true, eastl::string("assets/Meshes/")) ==
          BrowserEntryKind::folder);
  expect_true(
      "mesh.yaml classifies as Mesh",
      classifyBrowserEntry(false, eastl::string("assets/Meshes/a.mesh.yaml")) ==
          BrowserEntryKind::mesh);
  expect_true(
      "mesh.asset classifies as Mesh",
      classifyBrowserEntry(false,
                           eastl::string("assets/Meshes/a.mesh.asset")) ==
          BrowserEntryKind::mesh);
  expect_true(
      "scene.asset classifies as Scene",
      classifyBrowserEntry(false,
                           eastl::string("assets/Scenes/x.scene.asset")) ==
          BrowserEntryKind::scene);
  expect_true(
      "texture.yaml classifies as Texture",
      classifyBrowserEntry(
          false, eastl::string("assets/Textures/a.texture.yaml")) ==
          BrowserEntryKind::texture);
  expect_true(
      "animation.yaml classifies as AnimationClip",
      classifyBrowserEntry(
          false, eastl::string("assets/Animations/w.animation.yaml")) ==
          BrowserEntryKind::animation_clip);
  expect_true(
      "README classifies as File",
      classifyBrowserEntry(false, eastl::string("assets/README.md")) ==
          BrowserEntryKind::file);
  expect_true(
      "animationtree.yaml classifies as File",
      classifyBrowserEntry(
          false, eastl::string("assets/Trees/t.animationtree.yaml")) ==
          BrowserEntryKind::file);
  expect_eq_string("Folder label",
                   eastl::string(browserEntryTypeLabel(BrowserEntryKind::folder)),
                   "Folder");
  expect_eq_string(
      "AnimationClip label",
      eastl::string(browserEntryTypeLabel(BrowserEntryKind::animation_clip)),
      "AnimationClip");

  expect_eq_string("folder size blank", formatBrowserSize(4096, true), "");
  expect_eq_string("folder date blank", formatBrowserDate(1, true), "");
  expect_eq_string("zero date blank", formatBrowserDate(0, false), "");
  expect_eq_string("5 bytes", formatBrowserSize(5, false), "5 B");
  expect_eq_string("12.9 KB", formatBrowserSize(13210, false), "12.9 KB");

  {
    const auto now = std::chrono::file_clock::now();
    const uint64_t ticks =
        static_cast<uint64_t>(now.time_since_epoch().count());
    const eastl::string date_text = formatBrowserDate(ticks, false);
    expect_true("date formatted YYYY-MM-DD HH:MM", date_text.size() == 16);
    expect_true("date has dash", date_text.find("-") != eastl::string::npos);
  }

  {
    eastl::vector<ContentBrowserGridItem> items(4);
    items[0].display_name = "zeta.txt";
    items[0].is_directory = false;
    items[0].type_label = "File";
    items[0].size_bytes = 30;
    items[0].modified_time = 3;
    items[1].display_name = "Meshes";
    items[1].is_directory = true;
    items[1].type_label = "Folder";
    items[1].size_bytes = 0;
    items[1].modified_time = 9;
    items[2].display_name = "alpha.txt";
    items[2].is_directory = false;
    items[2].type_label = "File";
    items[2].size_bytes = 10;
    items[2].modified_time = 1;
    items[3].display_name = "Scenes";
    items[3].is_directory = true;
    items[3].type_label = "Folder";
    items[3].size_bytes = 0;
    items[3].modified_time = 8;

    sortBrowserGridItems(items, BrowserGridSortColumn::name, true);
    expect_eq_string("name folders first 0", items[0].display_name, "Meshes");
    expect_eq_string("name folders first 1", items[1].display_name, "Scenes");
    expect_eq_string("name files 2", items[2].display_name, "alpha.txt");
    expect_eq_string("name files 3", items[3].display_name, "zeta.txt");

    BrowserGridSortColumn column = BrowserGridSortColumn::name;
    bool ascending = true;
    toggleBrowserGridSort(column, ascending, BrowserGridSortColumn::size);
    expect_true("size sort starts ascending",
                column == BrowserGridSortColumn::size && ascending);
    sortBrowserGridItems(items, column, ascending);
    expect_true("size folders first", items[0].is_directory && items[1].is_directory);
    expect_eq_string("size files small first", items[2].display_name, "alpha.txt");
    expect_eq_string("size files large last", items[3].display_name, "zeta.txt");

    toggleBrowserGridSort(column, ascending, BrowserGridSortColumn::size);
    expect_true("second size click reverses",
                column == BrowserGridSortColumn::size && !ascending);
    sortBrowserGridItems(items, column, ascending);
    expect_true("reverse size folders first",
                items[0].is_directory && items[1].is_directory);
    expect_eq_string("reverse size files large first", items[2].display_name,
                     "zeta.txt");
    expect_eq_string("reverse size files small last", items[3].display_name,
                     "alpha.txt");

    toggleBrowserGridSort(column, ascending, BrowserGridSortColumn::date);
    expect_true("date click resets ascending",
                column == BrowserGridSortColumn::date && ascending);
    sortBrowserGridItems(items, column, ascending);
    expect_true("date folders first", items[0].is_directory && items[1].is_directory);
    expect_eq_string("date files old first", items[2].display_name, "alpha.txt");
    expect_eq_string("date files new last", items[3].display_name, "zeta.txt");

    sortBrowserGridItems(items, BrowserGridSortColumn::type, true);
    expect_true("type folders first", items[0].is_directory && items[1].is_directory);
    expect_eq_string("type folder Meshes", items[0].display_name, "Meshes");
    expect_eq_string("type folder Scenes", items[1].display_name, "Scenes");
  }

  file_system.shutdown();
  fs::remove_all(project);
  g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("content_browser_test: all passed\n");
  return 0;
}
