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
  const fs::path root =
      fs::temp_directory_path() / ("blunder_project_file_" + std::string(tag));
  fs::remove_all(root);
  fs::create_directories(root);
  return root;
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    const fs::path root = makeTempDir("missing");
    expect_true("missing dir is not a project", !isProjectDirectory(root));
  }

  {
    const fs::path root = makeTempDir("assets_only");
    fs::create_directories(root / "Assets");
    expect_true("Assets alone is not a project", !isProjectDirectory(root));
  }

  {
    const fs::path root = makeTempDir("roundtrip");
    expect_true("write project file", writeProjectFile(root, "Demo Game"));
    expect_true("dir is project after write", isProjectDirectory(root));

    ProjectInfo info;
    expect_true("read project file", readProjectFile(root, info));
    expect_true("name matches", info.name == "Demo Game");
    expect_true("root matches", info.root == fs::weakly_canonical(root));
  }

  {
    const fs::path root = makeTempDir("by_file");
    expect_true("write for file path test", writeProjectFile(root, "Via File"));
    const fs::path file = root / k_project_file_name;
    ProjectInfo info;
    expect_true("read via project.blunder path", readProjectFile(file, info));
    expect_true("name via file path", info.name == "Via File");
  }

  {
    const fs::path root = makeTempDir("bad_yaml");
    {
      std::ofstream out(root / k_project_file_name);
      out << "not_a_name: true\n";
    }
    ProjectInfo info;
    expect_true("rejects yaml without name", !readProjectFile(root, info));
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
