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

void parseMeshWithTextureGuids() {
  using namespace Blunder;
  const eastl::string yaml =
      "type: Mesh\n"
      "guid: 77777777-7777-7777-7777-777777777777\n"
      "source: Resources/Models/Hero.gltf\n"
      "texture_guids:\n"
      "  - aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa\n"
      "  - bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb\n"
      "import:\n"
      "  materials: true\n"
      "  animations: true\n"
      "  scale: 1.0\n";

  MeshAssetDescriptor desc;
  expect_true("parse mesh with texture_guids",
              AssetYaml::parseMeshDescriptor(yaml, desc));
  expect_true("texture_guids size 2", desc.texture_guids.size() == 2);
  expect_true("texture_guids[0]",
              desc.texture_guids[0] ==
                  "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
  expect_true("texture_guids[1]",
              desc.texture_guids[1] ==
                  "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb");
}

void parseMeshWithoutTextureGuidsLeavesEmpty() {
  using namespace Blunder;
  const eastl::string yaml =
      "type: Mesh\n"
      "guid: 88888888-8888-8888-8888-888888888888\n"
      "source: Resources/Models/Cube.gltf\n"
      "import:\n"
      "  materials: true\n"
      "  animations: true\n"
      "  scale: 1.0\n";

  MeshAssetDescriptor desc;
  expect_true("parse mesh without texture_guids",
              AssetYaml::parseMeshDescriptor(yaml, desc));
  expect_true("texture_guids empty when omitted",
              desc.texture_guids.empty());
}

void roundTripMeshTextureGuids() {
  using namespace Blunder;
  MeshAssetDescriptor in;
  in.guid = "99999999-9999-9999-9999-999999999999";
  in.source = "Resources/Models/Hero.gltf";
  in.texture_guids.push_back("cccccccc-cccc-4ccc-8ccc-cccccccccccc");
  in.texture_guids.push_back("dddddddd-dddd-4ddd-8ddd-dddddddddddd");

  const eastl::string yaml = AssetYaml::serializeMeshDescriptor(in);
  expect_true("serialize contains texture_guids key",
              yaml.find("texture_guids") != eastl::string::npos);

  MeshAssetDescriptor out;
  expect_true("texture_guids round-trip parse",
              AssetYaml::parseMeshDescriptor(yaml, out));
  expect_true("texture_guids round-trip size",
              out.texture_guids.size() == 2);
  expect_true("texture_guids round-trip [0]",
              out.texture_guids[0] == in.texture_guids[0]);
  expect_true("texture_guids round-trip [1]",
              out.texture_guids[1] == in.texture_guids[1]);
}

void omitEmptyTextureGuidsFromSerializedYaml() {
  using namespace Blunder;
  MeshAssetDescriptor mesh;
  mesh.guid = "abababab-abab-4aba-8bab-abababababab";
  mesh.source = "Resources/Models/Cube.gltf";
  mesh.texture_guids.clear();

  const eastl::string yaml = AssetYaml::serializeMeshDescriptor(mesh);
  expect_true("mesh omits empty texture_guids",
              yaml.find("texture_guids") == eastl::string::npos);
}

void roundTripAnimationClipDescriptor() {
  using namespace Blunder;
  AnimationClipAssetDescriptor in;
  in.guid = "a1a1a1a1-a1a1-4a1a-8a1a-a1a1a1a1a1a1";
  in.source = "resources/Animations/Walk/Walk.anim.yaml";
  in.archived_source.clear();

  const eastl::string yaml = AssetYaml::serializeAnimationClipDescriptor(in);
  AnimationClipAssetDescriptor out;
  expect_true("animation clip descriptor round-trip parse",
              AssetYaml::parseAnimationClipDescriptor(yaml, out));
  expect_true("animation clip descriptor round-trip guid",
              out.guid == in.guid);
  expect_true("animation clip descriptor round-trip source",
              out.source == in.source);
  expect_true("animation clip descriptor round-trip archived_source empty",
              out.archived_source.empty());
}

void roundTripAnimationClipDataConstantAndLinear() {
  using namespace Blunder;
  AnimationClipData in;
  in.name = "Walk";
  in.duration = 1.0f;

  AnimationTrack translation;
  translation.bone = "Hips";
  translation.channel = AnimationChannel::Translation;
  translation.interpolation = AnimationInterpolation::Constant;
  translation.keys.push_back({0.0f, {0.0f, 0.0f, 0.0f}});
  translation.keys.push_back({1.0f, {0.0f, 0.5f, 0.0f}});

  AnimationTrack rotation;
  rotation.bone = "Hips";
  rotation.channel = AnimationChannel::Rotation;
  rotation.interpolation = AnimationInterpolation::Linear;
  rotation.keys.push_back({0.0f, {0.0f, 0.0f, 0.0f, 1.0f}});
  rotation.keys.push_back({1.0f, {0.0f, 0.7071068f, 0.0f, 0.7071068f}});

  in.tracks.push_back(translation);
  in.tracks.push_back(rotation);

  const eastl::string yaml = AssetYaml::serializeAnimationClipData(in);
  AnimationClipData out;
  expect_true("animation clip data round-trip parse",
              AssetYaml::parseAnimationClipData(yaml, out));
  expect_true("animation clip data round-trip name", out.name == in.name);
  expect_true("animation clip data round-trip duration",
              out.duration == in.duration);
  expect_true("animation clip data round-trip track count",
              out.tracks.size() == 2);
  expect_true("animation clip data translation interpolation Constant",
              out.tracks[0].interpolation == AnimationInterpolation::Constant);
  expect_true("animation clip data rotation interpolation Linear",
              out.tracks[1].interpolation == AnimationInterpolation::Linear);
  expect_true("animation clip data translation key count",
              out.tracks[0].keys.size() == 2);
  expect_true("animation clip data rotation value length 4",
              out.tracks[1].keys[0].value.size() == 4);
}

void rejectUnknownInterpolation() {
  using namespace Blunder;
  const eastl::string yaml =
      "version: 1\n"
      "name: BadClip\n"
      "duration: 1.0\n"
      "tracks:\n"
      "  - bone: Hips\n"
      "    channel: translation\n"
      "    interpolation: Cubic\n"
      "    keys:\n"
      "      - time: 0.0\n"
      "        value: [0.0, 0.0, 0.0]\n";

  AnimationClipData data;
  expect_true("reject unknown interpolation Cubic",
              !AssetYaml::parseAnimationClipData(yaml, data));
}

}  // namespace

int main() {
  parseMeshWithArchivedSource();
  parseMeshWithoutArchivedSourceLegacy();
  roundTripMeshArchivedSource();
  roundTripTextureArchivedSource();
  omitEmptyArchivedSourceFromSerializedYaml();
  parseMeshWithTextureGuids();
  parseMeshWithoutTextureGuidsLeavesEmpty();
  roundTripMeshTextureGuids();
  omitEmptyTextureGuidsFromSerializedYaml();
  roundTripAnimationClipDescriptor();
  roundTripAnimationClipDataConstantAndLinear();
  rejectUnknownInterpolation();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
