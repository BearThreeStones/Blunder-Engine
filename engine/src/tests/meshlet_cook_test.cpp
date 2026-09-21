#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_cook/meshlet_builder.h"
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

eastl::vector<Blunder::MeshVertex> makeQuadVertices() {
  using namespace Blunder;
  eastl::vector<MeshVertex> vertices(4);
  vertices[0].position = glm::vec3(0.0f, 0.0f, 0.0f);
  vertices[1].position = glm::vec3(1.0f, 0.0f, 0.0f);
  vertices[2].position = glm::vec3(1.0f, 1.0f, 0.0f);
  vertices[3].position = glm::vec3(0.0f, 1.0f, 0.0f);
  vertices[0].color = glm::vec4(0.25f, 0.5f, 0.75f, 1.0f);
  vertices[1].color = glm::vec4(1.0f, 0.2f, 0.1f, 1.0f);
  vertices[2].color = glm::vec4(0.1f, 1.0f, 0.2f, 1.0f);
  vertices[3].color = glm::vec4(0.4f, 0.4f, 0.9f, 0.8f);
  return vertices;
}

Blunder::MeshSkinData makeSampleSkinData(size_t vertex_count) {
  Blunder::MeshSkinData skin_data;
  skin_data.joint_to_bone = {0};
  skin_data.influences.resize(vertex_count);
  for (size_t i = 0; i < vertex_count; ++i) {
    skin_data.influences[i].joint_indices = glm::ivec4(0, 0, 0, 0);
    skin_data.influences[i].weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
  }
  return skin_data;
}

void staticMeshletsRoundTripThroughCookFile() {
  using namespace Blunder;

  const fs::path temp = makeTempDir("blunder_meshlet_cook_rt_");
  const fs::path cooked_path = temp / "static.meshbin";

  const eastl::vector<MeshVertex> vertices = makeQuadVertices();
  const eastl::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};
  const MeshletPayload meshlets = buildStaticMeshlets(vertices, indices);
  expect_true("static meshlets built", !meshlets.empty());
  expect_true("static meshlet has sphere",
              !meshlets.meshlets.empty() && meshlets.meshlets[0].radius > 0.0f);

  expect_true("write meshlet cook file",
              writeMeshCookFile(cooked_path, vertices, indices, nullptr,
                                &meshlets));

  eastl::vector<MeshVertex> read_vertices;
  eastl::vector<uint32_t> read_indices;
  MeshSkinData read_skin;
  MeshletPayload read_meshlets;
  expect_true("read meshlet cook file",
              readMeshCookFile(cooked_path, read_vertices, read_indices,
                               &read_skin, &read_meshlets));

  expect_true("round-trip has no skin", !read_skin.isValid());
  expect_true("round-trip meshlet count",
              read_meshlets.meshlets.size() == meshlets.meshlets.size());
  expect_true("round-trip meshlet radius",
              !read_meshlets.meshlets.empty() &&
                  read_meshlets.meshlets[0].radius ==
                      meshlets.meshlets[0].radius);
  expect_true("round-trip cone bytes",
              !read_meshlets.meshlets.empty() &&
                  read_meshlets.meshlets[0].cone_axis[0] ==
                      meshlets.meshlets[0].cone_axis[0] &&
                  read_meshlets.meshlets[0].cone_cutoff ==
                      meshlets.meshlets[0].cone_cutoff);
  expect_true("round-trip COLOR_0",
              read_vertices.size() == vertices.size() &&
                  read_vertices[0].color.x == vertices[0].color.x &&
                  read_vertices[2].color.y == vertices[2].color.y &&
                  read_vertices[3].color.w == vertices[3].color.w);

  fs::remove_all(temp);
}

void skinnedCookOmitsMeshlets() {
  using namespace Blunder;

  const fs::path temp = makeTempDir("blunder_meshlet_cook_skin_");
  const fs::path cooked_path = temp / "skinned.meshbin";

  const eastl::vector<MeshVertex> vertices = makeQuadVertices();
  const eastl::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};
  const MeshSkinData skin = makeSampleSkinData(vertices.size());
  const MeshletPayload ignored = buildStaticMeshlets(vertices, indices);

  expect_true("write skinned ignores meshlets",
              writeMeshCookFile(cooked_path, vertices, indices, &skin,
                                &ignored));

  MeshletPayload read_meshlets;
  MeshSkinData read_skin;
  eastl::vector<MeshVertex> read_vertices;
  eastl::vector<uint32_t> read_indices;
  expect_true("read skinned cook",
              readMeshCookFile(cooked_path, read_vertices, read_indices,
                               &read_skin, &read_meshlets));
  expect_true("skinned still has skin", read_skin.isValid());
  expect_true("skinned has no meshlets", read_meshlets.empty());

  fs::remove_all(temp);
}

void cookStaticDescriptorWritesMeshlets() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempDir("blunder_meshlet_cook_e2e_");
  fs::create_directories(project / "Assets" / "Meshes");
  fs::create_directories(project / "Resources" / "Models");
  fs::create_directories(project / ".blunder" / "cooked");

  const char* kGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";
  const char* kDescriptorPath = "assets/Meshes/quad.mesh.yaml";

  writeTextFile(
      project / "Resources" / "Models" / "quad.gltf",
      R"({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{"mesh": 0}],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 1}, "indices": 0}]}],
  "accessors": [
    {"bufferView": 0, "componentType": 5123, "count": 6, "type": "SCALAR"},
    {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3",
     "max": [1.0, 1.0, 0.0], "min": [0.0, 0.0, 0.0]}
  ],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 12, "target": 34963},
    {"buffer": 0, "byteOffset": 12, "byteLength": 48, "target": 34962}
  ],
  "buffers": [{"byteLength": 60, "uri": "quad.bin"}]
}
)");

  {
    std::ofstream bin(project / "Resources" / "Models" / "quad.bin",
                      std::ios::binary | std::ios::trunc);
    const uint16_t idx[] = {0, 1, 2, 0, 2, 3};
    const float pos[] = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
    bin.write(reinterpret_cast<const char*>(idx), sizeof(idx));
    bin.write(reinterpret_cast<const char*>(pos), sizeof(pos));
  }

  writeTextFile(project / "Assets" / "Meshes" / "quad.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/quad.gltf\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register static mesh",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));

  AssetManager manager;
  AssetManagerInitInfo am_init{};
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  auto compiler = eastl::make_shared<AssetCompilerService>();
  compiler->initialize(&file_system, &manager, &registry);
  manager.setAssetCompiler(compiler);

  expect_true("cook static mesh descriptor",
              compiler->cookAsset(eastl::string(kGuid)));

  const fs::path cooked_path = cookedMeshPath(file_system, eastl::string(kGuid));
  expect_true("cooked meshbin exists", file_system.exists(cooked_path));

  eastl::vector<MeshVertex> cooked_vertices;
  eastl::vector<uint32_t> cooked_indices;
  MeshSkinData cooked_skin;
  MeshletPayload cooked_meshlets;
  expect_true("read cooked meshlets",
              readMeshCookFile(cooked_path, cooked_vertices, cooked_indices,
                               &cooked_skin, &cooked_meshlets));
  expect_true("cooked static has meshlets", !cooked_meshlets.empty());
  expect_true("cooked meshlet sphere",
              !cooked_meshlets.meshlets.empty() &&
                  cooked_meshlets.meshlets[0].radius > 0.0f);

  const eastl::shared_ptr<MeshAsset> loaded =
      manager.loadMesh(eastl::string(kDescriptorPath));
  expect_true("loadMesh prefers cooked Final", loaded != nullptr);
  expect_true("loaded cooked mesh has meshlets",
              loaded && loaded->hasMeshlets());

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
  staticMeshletsRoundTripThroughCookFile();
  skinnedCookOmitsMeshlets();
  cookStaticDescriptorWritesMeshlets();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d test(s) failed\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "meshlet_cook_test: all passed\n");
  return 0;
}
