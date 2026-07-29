#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/mesh_skin_data.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <glm/glm.hpp>

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

fs::path makeTempDir(const char* prefix) {
  const fs::path root =
      fs::temp_directory_path() /
      (std::string(prefix) +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root);
  return root;
}

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

Blunder::MeshSkinData makeSampleSkinData(size_t vertex_count) {
  Blunder::MeshSkinData skin_data;
  skin_data.joint_to_bone = {0, 1};
  skin_data.influences.resize(vertex_count);
  for (size_t i = 0; i < vertex_count; ++i) {
    skin_data.influences[i].joint_indices = glm::ivec4(0, 1, 0, 0);
    skin_data.influences[i].weights = glm::vec4(0.75f, 0.25f, 0.0f, 0.0f);
  }
  return skin_data;
}

eastl::vector<Blunder::MeshVertex> makeTriangleVertices() {
  using namespace Blunder;
  eastl::vector<MeshVertex> vertices(3);
  vertices[0].position = glm::vec3(0.0f, 0.0f, 0.0f);
  vertices[1].position = glm::vec3(1.0f, 0.0f, 0.0f);
  vertices[2].position = glm::vec3(0.0f, 1.0f, 0.0f);
  return vertices;
}

constexpr char kSimpleSkinGltfRel[] =
    "engine/3rdparty/assimp/test/models/glTF2/simple_skin/simple_skin.gltf";

void skinPayloadRoundTripsThroughMeshCookFile() {
  using namespace Blunder;

  const fs::path temp = makeTempDir("blunder_mesh_skin_cook_rt_");
  const fs::path cooked_path = temp / "skinned.meshbin";

  const eastl::vector<MeshVertex> vertices = makeTriangleVertices();
  const eastl::vector<uint32_t> indices = {0, 1, 2};
  const MeshSkinData skin_data = makeSampleSkinData(vertices.size());

  expect_true("write skinned mesh cook file",
              writeMeshCookFile(cooked_path, vertices, indices, &skin_data));

  eastl::vector<MeshVertex> read_vertices;
  eastl::vector<uint32_t> read_indices;
  MeshSkinData read_skin;
  expect_true("read skinned mesh cook file",
              readMeshCookFile(cooked_path, read_vertices, read_indices,
                               &read_skin));

  expect_true("round-trip vertex count", read_vertices.size() == 3);
  expect_true("round-trip index count", read_indices.size() == 3);
  expect_true("round-trip skin valid", read_skin.isValid());
  expect_true("round-trip joint count", read_skin.joint_to_bone.size() == 2);
  expect_true("round-trip joint_to_bone[0]",
              read_skin.joint_to_bone[0] == 0 && read_skin.joint_to_bone[1] == 1);
  expect_true("round-trip influence count", read_skin.influences.size() == 3);
  expect_true("round-trip influence weights",
              read_skin.influences[0].weights.x == 0.75f &&
                  read_skin.influences[0].weights.y == 0.25f);

  fs::remove_all(temp);
}

void legacyV1MeshCookFileStillLoadsWithoutSkin() {
  using namespace Blunder;

  const fs::path temp = makeTempDir("blunder_mesh_skin_cook_v1_");
  const fs::path cooked_path = temp / "static.meshbin";

  const eastl::vector<MeshVertex> vertices = makeTriangleVertices();
  const eastl::vector<uint32_t> indices = {0, 1, 2};

  expect_true("write legacy v1 mesh cook file",
              writeMeshCookFile(cooked_path, vertices, indices, nullptr));

  eastl::vector<MeshVertex> read_vertices;
  eastl::vector<uint32_t> read_indices;
  MeshSkinData read_skin;
  expect_true("read legacy v1 mesh cook file",
              readMeshCookFile(cooked_path, read_vertices, read_indices,
                               &read_skin));

  expect_true("legacy mesh has no skin payload", !read_skin.isValid());

  fs::remove_all(temp);
}

void cookSkinnedDescriptorWritesFinalSkinPayload() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempDir("blunder_mesh_skin_cook_e2e_");
  fs::create_directories(project / "Assets" / "Meshes");
  fs::create_directories(project / "Resources" / "Models");
  fs::create_directories(project / ".blunder" / "cooked");

  const char* kGuid = "eeeeeeee-ffff-4aaa-8bbb-cccccccccccc";
  const char* kDescriptorPath = "assets/Meshes/skinned.mesh.yaml";

  const fs::path fixture_gltf =
      fs::path(BLUNDER_REPO_ROOT) / kSimpleSkinGltfRel;
  expect_true("simple_skin glTF fixture exists", fs::exists(fixture_gltf));
  fs::copy_file(fixture_gltf, project / "Resources" / "Models" / "skinned.gltf",
                fs::copy_options::overwrite_existing);
  writeTextFile(project / "Assets" / "Meshes" / "skinned.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/skinned.gltf\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register skinned mesh",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));

  AssetManager manager;
  AssetManagerInitInfo am_init{};
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  auto compiler = eastl::make_shared<AssetCompilerService>();
  compiler->initialize(&file_system, &manager, &registry);
  manager.setAssetCompiler(compiler);

  expect_true("cook skinned mesh descriptor",
              compiler->cookAsset(eastl::string(kGuid)));

  const fs::path cooked_path = cookedMeshPath(file_system, eastl::string(kGuid));
  expect_true("cooked meshbin exists", file_system.exists(cooked_path));

  eastl::vector<MeshVertex> cooked_vertices;
  eastl::vector<uint32_t> cooked_indices;
  MeshSkinData cooked_skin;
  expect_true("read cooked skin payload",
              readMeshCookFile(cooked_path, cooked_vertices, cooked_indices,
                               &cooked_skin));
  expect_true("cooked skin payload valid", cooked_skin.isValid());
  expect_true("cooked skin influence count matches vertices",
              cooked_skin.influences.size() == cooked_vertices.size());
  expect_true("cooked skin has joint map", !cooked_skin.joint_to_bone.empty());

  const eastl::shared_ptr<MeshAsset> loaded =
      manager.loadMesh(eastl::string(kDescriptorPath));
  expect_true("loadMesh prefers cooked Final", loaded != nullptr);
  expect_true("loaded cooked mesh is skinned",
              loaded && loaded->isSkinned());
  expect_true("loaded cooked skin matches vertex count",
              loaded && loaded->getSkinData().influences.size() ==
                             loaded->getVertexCount());

  manager.setAssetCompiler({});
  compiler->shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

}  // namespace

int main() {
  skinPayloadRoundTripsThroughMeshCookFile();
  legacyV1MeshCookFileStillLoadsWithoutSkin();
  cookSkinnedDescriptorWritesFinalSkinPayload();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d test(s) failed\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "mesh_skin_cook_test: all passed\n");
  return 0;
}
