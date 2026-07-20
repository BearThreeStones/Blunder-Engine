#include "runtime/resource/asset/asset_yaml.h"

#include <cassert>
#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void parseMeshWithArchivedSource() {
  using namespace Blunder;
  const eastl::string yaml =
      "type: Mesh\n"
      "guid: 11111111-1111-1111-1111-111111111111\n"
      "source: Resources/Models/Cube.gltf\n"
      "archived_source: Source/Models/Cube.fbx\n"
      "import:\n"
      "  materials: true\n"
      "  animations: false\n"
      "  scale: 1.0\n";

  MeshAssetDescriptor desc;
  expect_true("parse mesh with archived_source",
              AssetYaml::parseMeshDescriptor(yaml, desc));
  expect_true("mesh source preserved",
              desc.source == "Resources/Models/Cube.gltf");
  expect_true("mesh archived_source set",
              desc.archived_source == "Source/Models/Cube.fbx");
}

void parseMeshWithoutArchivedSourceLegacy() {
  using namespace Blunder;
  const eastl::string yaml =
      "type: Mesh\n"
      "guid: 22222222-2222-2222-2222-222222222222\n"
      "source: Resources/Models/Cube.gltf\n"
      "import:\n"
      "  materials: true\n"
      "  animations: true\n"
      "  scale: 1.0\n";

  MeshAssetDescriptor desc;
  expect_true("parse legacy mesh without archived_source",
              AssetYaml::parseMeshDescriptor(yaml, desc));
  expect_true("legacy mesh source set",
              desc.source == "Resources/Models/Cube.gltf");
  expect_true("legacy archived_source empty", desc.archived_source.empty());
}

void roundTripMeshArchivedSource() {
  using namespace Blunder;
  MeshAssetDescriptor in;
  in.guid = "33333333-3333-3333-3333-333333333333";
  in.source = "Resources/Models/Hero.gltf";
  in.archived_source = "Source/Models/Hero.fbx";
  in.import.materials = true;
  in.import.animations = false;
  in.import.scale = 2.0f;

  const eastl::string yaml = AssetYaml::serializeMeshDescriptor(in);
  expect_true("mesh serialize contains archived_source key",
              yaml.find("archived_source:") != eastl::string::npos);

  MeshAssetDescriptor out;
  expect_true("mesh round-trip parse",
              AssetYaml::parseMeshDescriptor(yaml, out));
  expect_true("mesh round-trip guid", out.guid == in.guid);
  expect_true("mesh round-trip source", out.source == in.source);
  expect_true("mesh round-trip archived_source",
              out.archived_source == in.archived_source);
}

void roundTripTextureArchivedSource() {
  using namespace Blunder;
  TextureAssetDescriptor in;
  in.guid = "44444444-4444-4444-4444-444444444444";
  in.source = "Resources/Textures/Albedo.png";
  in.archived_source = "Source/Textures/Albedo.tga";
  in.import.srgb = true;
  in.import.generate_mips = true;

  const eastl::string yaml = AssetYaml::serializeTextureDescriptor(in);
  expect_true("texture serialize contains archived_source key",
              yaml.find("archived_source:") != eastl::string::npos);

  TextureAssetDescriptor out;
  expect_true("texture round-trip parse",
              AssetYaml::parseTextureDescriptor(yaml, out));
  expect_true("texture round-trip guid", out.guid == in.guid);
  expect_true("texture round-trip source", out.source == in.source);
  expect_true("texture round-trip archived_source",
              out.archived_source == in.archived_source);
}

void omitEmptyArchivedSourceFromSerializedYaml() {
  using namespace Blunder;
  MeshAssetDescriptor mesh;
  mesh.guid = "55555555-5555-5555-5555-555555555555";
  mesh.source = "Resources/Models/Cube.gltf";
  mesh.archived_source.clear();

  const eastl::string mesh_yaml = AssetYaml::serializeMeshDescriptor(mesh);
  expect_true("mesh omits empty archived_source",
              mesh_yaml.find("archived_source") == eastl::string::npos);

  TextureAssetDescriptor tex;
  tex.guid = "66666666-6666-6666-6666-666666666666";
  tex.source = "Resources/Textures/Albedo.png";
  tex.archived_source.clear();

  const eastl::string tex_yaml = AssetYaml::serializeTextureDescriptor(tex);
  expect_true("texture omits empty archived_source",
              tex_yaml.find("archived_source") == eastl::string::npos);
}

}  // namespace

int main() {
  parseMeshWithArchivedSource();
  parseMeshWithoutArchivedSourceLegacy();
  roundTripMeshArchivedSource();
  roundTripTextureArchivedSource();
  omitEmptyArchivedSourceFromSerializedYaml();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
