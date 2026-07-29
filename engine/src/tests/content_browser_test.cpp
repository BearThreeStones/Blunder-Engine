#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/content/content_entry.h"
#include "runtime/resource/content/content_index.h"

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
