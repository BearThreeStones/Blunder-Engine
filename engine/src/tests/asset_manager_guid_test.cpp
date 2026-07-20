#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

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
      ("blunder_asset_manager_guid_test_" +
       std::to_string(
           static_cast<unsigned long long>(
               std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Resources" / "Models");
  return root;
}

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

// Minimal single-triangle COLLADA Intermediate (Assimp-readable).
constexpr char kMinimalTriangleDae[] = R"(<?xml version="1.0" encoding="utf-8"?>
<COLLADA xmlns="http://www.collada.org/2005/11/COLLADASchema" version="1.4.1">
  <asset>
    <up_axis>Y_UP</up_axis>
  </asset>
  <library_geometries>
    <geometry id="tri-mesh" name="tri">
      <mesh>
        <source id="tri-positions">
          <float_array id="tri-positions-array" count="9">0 0 0 1 0 0 0 1 0</float_array>
          <technique_common>
            <accessor source="#tri-positions-array" count="3" stride="3">
              <param name="X" type="float"/>
              <param name="Y" type="float"/>
              <param name="Z" type="float"/>
            </accessor>
          </technique_common>
        </source>
        <vertices id="tri-vertices">
          <input semantic="POSITION" source="#tri-positions"/>
        </vertices>
        <triangles count="1">
          <input semantic="VERTEX" source="#tri-vertices" offset="0"/>
          <p>0 1 2</p>
        </triangles>
      </mesh>
    </geometry>
  </library_geometries>
  <library_visual_scenes>
    <visual_scene id="Scene" name="Scene">
      <node id="tri-node" name="tri">
        <instance_geometry url="#tri-mesh"/>
      </node>
    </visual_scene>
  </library_visual_scenes>
  <scene>
    <instance_visual_scene url="#Scene"/>
  </scene>
</COLLADA>
)";

void loadMeshByGuidLoadsRegisteredDescriptor() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";
  const char* kDescriptorPath = "assets/Meshes/tri.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "tri.dae",
                kMinimalTriangleDae);
  writeTextFile(project / "Assets" / "Meshes" / "tri.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/tri.dae\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register mesh descriptor",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));
  expect_true("resolveGuid returns descriptor path",
              registry.resolveGuid(eastl::string(kGuid)) == kDescriptorPath);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  const eastl::shared_ptr<MeshAsset> by_guid =
      manager.loadMeshByGuid(eastl::string(kGuid), registry);
  expect_true("loadMeshByGuid returns mesh", by_guid != nullptr);
  expect_true("loadMeshByGuid mesh has vertices",
              by_guid && by_guid->getVertexCount() == 3);
  expect_true("loadMeshByGuid mesh has indices",
              by_guid && by_guid->getIndexCount() == 3);

  const eastl::shared_ptr<MeshAsset> by_path =
      manager.loadMesh(eastl::string(kDescriptorPath));
  expect_true("path load also succeeds", by_path != nullptr);
  expect_true("guid and path loads agree on vertex count",
              by_guid && by_path &&
                  by_guid->getVertexCount() == by_path->getVertexCount());

  const eastl::string resolved =
      manager.resolveGuidPath(eastl::string(kGuid), registry);
  expect_true("resolveGuidPath returns descriptor virtual path",
              resolved == kDescriptorPath);

  expect_true("unknown guid load returns null",
              manager.loadMeshByGuid(
                  eastl::string("00000000-0000-4000-8000-000000000000"),
                  registry) == nullptr);

  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

}  // namespace

int main() {
  loadMeshByGuidLoadsRegisteredDescriptor();

  const int exit_code = g_failures != 0 ? 1 : 0;
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
  } else {
    std::fprintf(stdout, "asset_manager_guid_test: all passed\n");
  }

  Blunder::g_runtime_global_context.m_logger_system.reset();
  return exit_code;
}
