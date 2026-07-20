#include "runtime/project/project_create.h"

#include <cctype>
#include <fstream>
#include <system_error>

namespace Blunder {

namespace {

namespace fs = std::filesystem;

bool isDirectoryEmptyOrMissing(const fs::path& path, eastl::string& out_error) {
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    return true;
  }
  if (ec) {
    out_error = "cannot access target path";
    return false;
  }
  if (!fs::is_directory(path, ec) || ec) {
    out_error = "target path is not a directory";
    return false;
  }
  const fs::directory_iterator end;
  fs::directory_iterator it(path, ec);
  if (ec) {
    out_error = "cannot read target directory";
    return false;
  }
  if (it != end) {
    out_error = "target directory is not empty";
    return false;
  }
  return true;
}

eastl::string folderNameFromProjectName(const eastl::string& name) {
  eastl::string out;
  out.reserve(name.size());
  for (const char ch : name) {
    if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' ||
        ch == '_') {
      out.push_back(ch);
    } else if (ch == ' ' || ch == '.') {
      if (!out.empty() && out.back() != '-') {
        out.push_back('-');
      }
    }
  }
  while (!out.empty() && out.back() == '-') {
    out.pop_back();
  }
  if (out.empty()) {
    out = "NewProject";
  }
  return out;
}

eastl::string csharpIdentifierFromFolderName(const eastl::string& folder) {
  eastl::string out;
  out.reserve(folder.size());
  for (const char ch : folder) {
    if (std::isalnum(static_cast<unsigned char>(ch))) {
      out.push_back(ch);
    } else if (ch == '-' || ch == '_') {
      out.push_back('_');
    }
  }
  if (out.empty()) {
    out = "NewProject";
  }
  if (std::isdigit(static_cast<unsigned char>(out[0]))) {
    out = eastl::string("P_") + out;
  }
  return out;
}

bool writeTextFile(const fs::path& path, const eastl::string& text,
                   eastl::string& out_error) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  if (ec) {
    out_error = "failed to create Scripts directory";
    return false;
  }
  std::ofstream stream(path, std::ios::trunc);
  if (!stream) {
    out_error = "failed to write Scripts scaffold";
    return false;
  }
  stream << text.c_str();
  if (!stream) {
    out_error = "failed to write Scripts scaffold";
    return false;
  }
  return true;
}

bool writeScriptsScaffold(const fs::path& root, const eastl::string& name,
                          eastl::string& out_error) {
  const eastl::string folder = folderNameFromProjectName(name);
  const eastl::string ns = csharpIdentifierFromFolderName(folder);

  eastl::string csproj;
  csproj += "<Project Sdk=\"Microsoft.NET.Sdk\">\n";
  csproj += "  <PropertyGroup>\n";
  csproj += "    <TargetFramework>net10.0</TargetFramework>\n";
  csproj += "    <ImplicitUsings>enable</ImplicitUsings>\n";
  csproj += "    <Nullable>enable</Nullable>\n";
  csproj += "    <EnableDefaultCompileItems>true</EnableDefaultCompileItems>\n";
  csproj += "    <AssemblyName>";
  csproj += folder;
  csproj += "</AssemblyName>\n";
  csproj += "    <RootNamespace>";
  csproj += ns;
  csproj += "</RootNamespace>\n";
  csproj +=
      "    <!-- Directory containing Blunder.Api.dll (beside the editor). "
      "ScriptsBuilder passes -p:BlunderEngineDir=... -->\n";
  csproj +=
      "    <BlunderEngineDir Condition=\"'$(BlunderEngineDir)' == "
      "''\"></BlunderEngineDir>\n";
  csproj += "  </PropertyGroup>\n";
  csproj += "  <ItemGroup>\n";
  csproj += "    <Reference Include=\"Blunder.Api\">\n";
  csproj +=
      "      <HintPath>$(BlunderEngineDir)Blunder.Api.dll</HintPath>\n";
  csproj += "      <Private>false</Private>\n";
  csproj += "    </Reference>\n";
  csproj += "  </ItemGroup>\n";
  csproj += "</Project>\n";

  eastl::string hello;
  hello += "using Blunder;\n\n";
  hello += "namespace ";
  hello += ns;
  hello += ";\n\n";
  hello += "public sealed class HelloBehaviour : Behaviour\n";
  hello += "{\n";
  hello += "    public override void Ready() { }\n\n";
  hello += "    public override void Tick(float deltaTime) { }\n";
  hello += "}\n";

  const fs::path scripts = root / "Scripts";
  if (!writeTextFile(scripts / (folder + ".csproj").c_str(), csproj,
                     out_error)) {
    return false;
  }
  if (!writeTextFile(scripts / "HelloBehaviour.cs", hello, out_error)) {
    return false;
  }
  return true;
}

}  // namespace

bool createProject(const CreateProjectRequest& request, ProjectInfo& out_info,
                   eastl::string& out_error) {
  out_error.clear();
  if (request.name.empty()) {
    out_error = "project name is required";
    return false;
  }
  if (request.target_path.empty()) {
    out_error = "project path is required";
    return false;
  }

  fs::path root = request.target_path;
  if (request.create_folder) {
    root = request.target_path / folderNameFromProjectName(request.name).c_str();
  }

  if (fs::exists(root / k_project_file_name)) {
    out_error = "project file already exists";
    return false;
  }

  if (!isDirectoryEmptyOrMissing(root, out_error)) {
    return false;
  }

  std::error_code ec;
  fs::create_directories(root, ec);
  if (ec) {
    out_error = "failed to create project directory";
    return false;
  }

  for (const char* sub : {"Assets", "Resources", ".blunder", "Scripts"}) {
    fs::create_directories(root / sub, ec);
    if (ec) {
      out_error = "failed to create project scaffold";
      return false;
    }
  }

  if (!writeScriptsScaffold(root, request.name, out_error)) {
    return false;
  }

  if (!writeProjectFile(root, request.name)) {
    out_error = "failed to write project file";
    return false;
  }

  return readProjectFile(root, out_info);
}

}  // namespace Blunder
