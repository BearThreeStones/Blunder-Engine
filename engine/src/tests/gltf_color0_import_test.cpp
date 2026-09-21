#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <glm/vec4.hpp>

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

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

void writeBytes(const fs::path& path, const std::vector<uint8_t>& bytes) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

fs::path makeTempProject() {
  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_gltf_color0_import_test_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Resources" / "Models");
  return root;
}

bool colorNear(const glm::vec4& value, const glm::vec4& expected, float eps) {
  return std::fabs(value.x - expected.x) <= eps &&
         std::fabs(value.y - expected.y) <= eps &&
         std::fabs(value.z - expected.z) <= eps &&
         std::fabs(value.w - expected.w) <= eps;
}

void writeTriangleBin(const fs::path& path, bool with_color) {
  // indices u16[3] at 0 (6 bytes) + 2 pad, positions float3[3] at 8,
  // optional COLOR_0 float4[3] at 44.
  std::vector<uint8_t> bytes(with_color ? 92u : 44u, 0);
  const uint16_t indices[3] = {0, 1, 2};
  std::memcpy(bytes.data(), indices, sizeof(indices));
  const float positions[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                              0.0f, 1.0f, 0.0f};
  std::memcpy(bytes.data() + 8, positions, sizeof(positions));
  if (with_color) {
    const float colors[12] = {0.25f, 0.50f, 0.75f, 1.0f, 1.00f, 0.20f,
                              0.10f, 1.0f,  0.10f, 1.00f, 0.20f, 1.0f};
    std::memcpy(bytes.data() + 44, colors, sizeof(colors));
  }
  writeBytes(path, bytes);
}

constexpr char kColor0Gltf[] = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0 }],
  "meshes": [{
    "primitives": [{
      "attributes": { "POSITION": 1, "COLOR_0": 2 },
      "indices": 0
    }]
  }],
  "accessors": [
    { "bufferView": 0, "componentType": 5123, "count": 3, "type": "SCALAR" },
    {
      "bufferView": 1,
      "componentType": 5126,
      "count": 3,
      "type": "VEC3",
      "max": [1.0, 1.0, 0.0],
      "min": [0.0, 0.0, 0.0]
    },
    { "bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC4" }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 6 },
    { "buffer": 0, "byteOffset": 8, "byteLength": 36 },
    { "buffer": 0, "byteOffset": 44, "byteLength": 48 }
  ],
  "buffers": [{ "byteLength": 92, "uri": "patch.bin" }]
}
)";

constexpr char kNoColorGltf[] = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0 }],
  "meshes": [{
    "primitives": [{
      "attributes": { "POSITION": 1 },
      "indices": 0
    }]
  }],
  "accessors": [
    { "bufferView": 0, "componentType": 5123, "count": 3, "type": "SCALAR" },
    {
      "bufferView": 1,
      "componentType": 5126,
      "count": 3,
      "type": "VEC3",
      "max": [1.0, 1.0, 0.0],
      "min": [0.0, 0.0, 0.0]
    }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 6 },
    { "buffer": 0, "byteOffset": 8, "byteLength": 36 }
  ],
  "buffers": [{ "byteLength": 44, "uri": "plain.bin" }]
}
)";

eastl::shared_ptr<Blunder::MeshAsset> loadMeshFromProject(
    const fs::path& project, const char* descriptor) {
  using namespace Blunder;
  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  eastl::shared_ptr<MeshAsset> mesh = manager.loadMesh(eastl::string(descriptor));
  manager.shutdown();
  file_system.shutdown();
  return mesh;
}

void color0IsImportedIntoMeshVertex() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempProject();
  writeTriangleBin(project / "Resources" / "Models" / "patch.bin", true);
  writeTextFile(project / "Resources" / "Models" / "patch.gltf", kColor0Gltf);
  writeTextFile(project / "Assets" / "Meshes" / "patch.mesh.yaml",
                "type: Mesh\n"
                "guid: aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee40\n"
                "source: resources/Models/patch.gltf\n"
                "import:\n  materials: true\n  animations: false\n"
                "  scale: 1\n");

  const eastl::shared_ptr<MeshAsset> mesh =
      loadMeshFromProject(project, "assets/Meshes/patch.mesh.yaml");
  expect_true("COLOR_0 mesh loads", mesh != nullptr);
  if (mesh) {
    expect_true("COLOR_0 vertex count", mesh->getVertexCount() == 3u);
    const eastl::vector<MeshVertex>& vertices = mesh->getVertices();
    expect_true("COLOR_0 v0",
                colorNear(vertices[0].color, glm::vec4(0.25f, 0.50f, 0.75f, 1.0f),
                          1e-5f));
    expect_true("COLOR_0 v1",
                colorNear(vertices[1].color, glm::vec4(1.00f, 0.20f, 0.10f, 1.0f),
                          1e-5f));
    expect_true("COLOR_0 v2",
                colorNear(vertices[2].color, glm::vec4(0.10f, 1.00f, 0.20f, 1.0f),
                          1e-5f));
  }

  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void missingColor0DefaultsWhite() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempProject();
  writeTriangleBin(project / "Resources" / "Models" / "plain.bin", false);
  writeTextFile(project / "Resources" / "Models" / "plain.gltf", kNoColorGltf);
  writeTextFile(project / "Assets" / "Meshes" / "plain.mesh.yaml",
                "type: Mesh\n"
                "guid: aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee41\n"
                "source: resources/Models/plain.gltf\n"
                "import:\n  materials: true\n  animations: false\n"
                "  scale: 1\n");

  const eastl::shared_ptr<MeshAsset> mesh =
      loadMeshFromProject(project, "assets/Meshes/plain.mesh.yaml");
  expect_true("plain mesh loads", mesh != nullptr);
  if (mesh && mesh->getVertexCount() > 0) {
    expect_true("missing COLOR_0 is white",
                colorNear(mesh->getVertices()[0].color, glm::vec4(1.0f), 1e-5f));
  }

  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

}  // namespace

int main() {
  color0IsImportedIntoMeshVertex();
  missingColor0DefaultsWhite();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "gltf_color0_import_test: all passed\n");
  return 0;
}
