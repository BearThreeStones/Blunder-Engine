#include "runtime/project/project_create.h"
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
      fs::temp_directory_path() / ("blunder_project_create_" + std::string(tag));
  fs::remove_all(root);
  fs::create_directories(root);
  return root;
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    const fs::path parent = makeTempDir("empty_ok");
    const fs::path target = parent / "MyGame";
    CreateProjectRequest request;
    request.name = "My Game";
    request.target_path = target;
    request.create_folder = false;

    ProjectInfo info;
    eastl::string error;
    expect_true("create in new empty path",
                createProject(request, info, error));
    expect_true("created is project", isProjectDirectory(target));
    expect_true("scaffold Assets", fs::is_directory(target / "Assets"));
    expect_true("scaffold Resources", fs::is_directory(target / "Resources"));
    expect_true("scaffold .blunder", fs::is_directory(target / ".blunder"));
    expect_true("scaffold Scripts", fs::is_directory(target / "Scripts"));
    const fs::path csproj = target / "Scripts" / "My-Game.csproj";
    expect_true("scaffold Scripts csproj", fs::is_regular_file(csproj));
    {
      std::ifstream in(csproj);
      std::string contents((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
      expect_true("csproj targets net10.0",
                  contents.find("net10.0") != std::string::npos);
      expect_true("csproj HintPath Blunder.Api",
                  contents.find("Blunder.Api") != std::string::npos &&
                      contents.find("HintPath") != std::string::npos);
      expect_true("csproj BlunderEngineDir",
                  contents.find("BlunderEngineDir") != std::string::npos);
    }
    expect_true("scaffold HelloBehaviour",
                fs::is_regular_file(target / "Scripts" / "HelloBehaviour.cs"));
    expect_true("created name", info.name == "My Game");
  }

  {
    const fs::path parent = makeTempDir("create_folder");
    CreateProjectRequest request;
    request.name = "Demo";
    request.target_path = parent;
    request.create_folder = true;

    ProjectInfo info;
    eastl::string error;
    expect_true("create with folder", createProject(request, info, error));
    const fs::path expected = parent / "Demo";
    expect_true("subdir created", isProjectDirectory(expected));
    expect_true("info root is subdir", info.root == fs::weakly_canonical(expected));
  }

  {
    const fs::path target = makeTempDir("non_empty");
    {
      std::ofstream((target / "junk.txt").string()) << "x";
    }
    CreateProjectRequest request;
    request.name = "Nope";
    request.target_path = target;
    request.create_folder = false;

    ProjectInfo info;
    eastl::string error;
    expect_true("reject non-empty", !createProject(request, info, error));
    expect_true("error message set", !error.empty());
    expect_true("no project file written",
                !fs::exists(target / k_project_file_name));
  }

  {
    const fs::path target = makeTempDir("overwrite");
    expect_true("seed project", writeProjectFile(target, "Old"));
    CreateProjectRequest request;
    request.name = "New";
    request.target_path = target;
    request.create_folder = false;

    ProjectInfo info;
    eastl::string error;
    expect_true("reject existing project file",
                !createProject(request, info, error));
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
