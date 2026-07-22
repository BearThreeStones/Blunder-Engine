#include "runtime/project/project_manager_controller.h"
#include "runtime/project/project_file.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
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
  const fs::path root = fs::temp_directory_path() /
                        ("blunder_pm_ctrl_" + std::string(tag));
  fs::remove_all(root);
  fs::create_directories(root);
  return root;
}

}  // namespace

int main() {
  using namespace Blunder;

  const fs::path base = makeTempDir("main");
  const fs::path store = base / "list.yaml";

  ProjectManagerController ctrl(store);
  expect_true("load empty", ctrl.load());
  expect_true("starts empty", ctrl.entries().empty());

  {
    CreateProjectRequest req;
    req.name = "Alpha";
    req.target_path = base / "Alpha";
    req.create_folder = false;
    eastl::string error;
    expect_true("create ok", ctrl.createProject(req, error));
    expect_true("create no error", error.empty());
    expect_true("one entry", ctrl.entries().size() == 1);
    expect_true("pending open after create",
                ctrl.pendingOpenRoot() == fs::weakly_canonical(base / "Alpha"));
  }

  ctrl.clearPendingOpen();
  expect_true("cleared pending", ctrl.pendingOpenRoot().empty());

  {
    const fs::path other = base / "Beta";
    expect_true("seed beta", writeProjectFile(other, "Beta"));
    eastl::string error;
    expect_true("import ok", ctrl.importProject(other, error));
    expect_true("two entries", ctrl.entries().size() == 2);
    expect_true("pending open after import",
                !ctrl.pendingOpenRoot().empty());
  }

  ctrl.clearPendingOpen();
  expect_true("open index 0", ctrl.openEntry(0).empty());
  expect_true("pending after open", !ctrl.pendingOpenRoot().empty());

  const size_t before_remove = ctrl.entries().size();
  expect_true("remove 0", ctrl.removeEntry(0));
  expect_true("count decreased", ctrl.entries().size() == before_remove - 1);
  expect_true("disk project remains",
              isProjectDirectory(base / "Alpha") ||
                  isProjectDirectory(base / "Beta"));

  {
    const fs::path gone = base / "Ghost";
    expect_true("seed ghost", writeProjectFile(gone, "Ghost"));
    eastl::string error;
    expect_true("import ghost", ctrl.importProject(gone, error));
    ctrl.clearPendingOpen();
    fs::remove_all(gone);
    ctrl.refreshMissing();
    bool found_missing = false;
    for (const auto& e : ctrl.entries()) {
      if (e.name == "Ghost") {
        found_missing = e.missing;
      }
    }
    expect_true("ghost missing", found_missing);
    eastl::string open_err = ctrl.openEntryByName("Ghost");
    expect_true("open missing fails", !open_err.empty());
    expect_true("no pending on missing open", ctrl.pendingOpenRoot().empty());
  }

  expect_true("save", ctrl.save());
  ProjectManagerController ctrl2(store);
  expect_true("reload", ctrl2.load());
  expect_true("persisted", !ctrl2.entries().empty());

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
