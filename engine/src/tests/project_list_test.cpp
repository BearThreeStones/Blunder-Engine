#include "runtime/project/project_list.h"
#include "runtime/project/project_file.h"

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

namespace fs = std::filesystem;

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

fs::path makeTempDir(const char* tag) {
  const fs::path root =
      fs::temp_directory_path() / ("blunder_project_list_" + std::string(tag));
  fs::remove_all(root);
  fs::create_directories(root);
  return root;
}

}  // namespace

int main() {
  using namespace Blunder;

  const fs::path base = makeTempDir("store");
  const fs::path store = base / "project_list.yaml";

  const fs::path project_a = base / "A";
  const fs::path project_b = base / "B";
  expect_true("seed A", writeProjectFile(project_a, "Alpha"));
  expect_true("seed B", writeProjectFile(project_b, "Beta"));

  {
    ProjectList list;
    expect_true("add A", list.addOrUpdate(project_a));
    expect_true("add B", list.addOrUpdate(project_b));
    expect_true("two entries", list.entries().size() == 2);
    expect_true("save", list.save(store));
  }

  {
    ProjectList list;
    expect_true("load", list.load(store));
    expect_true("loaded two", list.entries().size() == 2);
  }

  {
    ProjectList list;
    expect_true("reload for dedupe", list.load(store));
    expect_true("re-add A updates", list.addOrUpdate(project_a));
    expect_true("still one A", list.entries().size() == 2);
  }

  {
    ProjectList list;
    expect_true("reload for remove", list.load(store));
    expect_true("remove A", list.remove(project_a));
    expect_true("one left", list.entries().size() == 1);
    expect_true("A still on disk", isProjectDirectory(project_a));
    expect_true("save after remove", list.save(store));
  }

  {
    const fs::path gone = base / "Gone";
    expect_true("seed Gone", writeProjectFile(gone, "Ghost"));
    ProjectList list;
    expect_true("add Gone", list.addOrUpdate(gone));
    expect_true("Gone listed", list.entries().size() == 1);
    const fs::path stored_path = list.entries()[0].path;
    fs::remove_all(gone);
    list.refreshMissing();
    expect_true("still listed after delete", list.entries().size() == 1);
    expect_true("path unchanged", list.entries()[0].path == stored_path);
    expect_true("missing marked", list.entries()[0].missing);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
