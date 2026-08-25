#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset/asset_descriptor.h"

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

void roundTripMeshCompanionAnimationSources() {
  using namespace Blunder;
  MeshAssetDescriptor in;
  in.guid = "cdcdcdcd-cdcd-4cdc-8dcd-cdcdcdcdcdcd";
  in.source = "resources/Models/Hero/Hero.gltf";
  in.companion_animation_sources.push_back(
      "resources/Models/Hero/companions/LOOP-idle.gltf");
  in.companion_animation_sources.push_back(
      "resources/Models/Hero/companions/LOOP-walk.glb");

  const eastl::string yaml = AssetYaml::serializeMeshDescriptor(in);
  expect_true("serialize contains companion_animation_sources key",
              yaml.find("companion_animation_sources") != eastl::string::npos);

  MeshAssetDescriptor out;
  expect_true("companion_animation_sources round-trip parse",
              AssetYaml::parseMeshDescriptor(yaml, out));
  expect_true("companion_animation_sources round-trip size",
              out.companion_animation_sources.size() == 2);
  expect_true("companion_animation_sources round-trip [0]",
              out.companion_animation_sources[0] ==
                  in.companion_animation_sources[0]);
  expect_true("companion_animation_sources round-trip [1]",
              out.companion_animation_sources[1] ==
                  in.companion_animation_sources[1]);
}

void parseMeshWithoutCompanionAnimationSourcesLeavesEmpty() {
  using namespace Blunder;
  const eastl::string yaml =
      "type: Mesh\n"
      "guid: efefefef-efef-4efe-8efe-efefefefefef\n"
      "source: resources/Models/Cube/Cube.gltf\n"
      "import:\n"
      "  materials: true\n"
      "  animations: true\n"
      "  scale: 1.0\n";

  MeshAssetDescriptor desc;
  expect_true("parse mesh without companion_animation_sources",
              AssetYaml::parseMeshDescriptor(yaml, desc));
  expect_true("companion_animation_sources empty when omitted",
              desc.companion_animation_sources.empty());
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

void roundTripAnimationClipMethodKeys() {
  using namespace Blunder;
  AnimationClipData in;
  in.name = "Events";
  in.duration = 1.0f;

  AnimationTrack track;
  track.bone = "Hips";
  track.channel = AnimationChannel::Translation;
  track.interpolation = AnimationInterpolation::Constant;
  track.keys.push_back({0.0f, {0.0f, 0.0f, 0.0f}});
  track.keys.push_back({1.0f, {0.0f, 0.0f, 0.0f}});
  in.tracks.push_back(track);

  AnimationMethodKey key;
  key.name = "Footstep";
  key.time = 0.5f;
  key.args.push_back(1.5f);
  in.method_keys.push_back(key);

  const eastl::string yaml = AssetYaml::serializeAnimationClipData(in);
  expect_true("method_keys in yaml",
              yaml.find("method_keys") != eastl::string::npos);

  AnimationClipData out;
  expect_true("method_keys round-trip parse",
              AssetYaml::parseAnimationClipData(yaml, out));
  expect_true("method_keys round-trip size", out.method_keys.size() == 1);
  expect_true("method_keys round-trip name",
              out.method_keys[0].name == "Footstep");
  expect_true("method_keys round-trip time", out.method_keys[0].time == 0.5f);
  expect_true("method_keys round-trip arg",
              out.method_keys[0].args.size() == 1 &&
                  out.method_keys[0].args[0] == 1.5f);
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

void parseSparseMaterialOverrideAndEmptySlot() {
  using namespace Blunder;
  const eastl::string yaml =
      "type: Mesh\n"
      "guid: 10101010-1010-4010-8010-101010101010\n"
      "source: resources/Models/Hero.gltf\n"
      "texture_guids:\n"
      "  - aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa\n"
      "material_override:\n"
      "  shininess: 64\n"
      "  textures:\n"
      "    base_color: \"\"\n"
      "legacy_note: keep-me\n"
      "import:\n"
      "  materials: true\n"
      "  animations: true\n"
      "  scale: 1.0\n";

  MeshAssetDescriptor desc;
  expect_true("parse sparse material_override",
              AssetYaml::parseMeshDescriptor(yaml, desc));
  expect_true("shininess present", desc.material_override.shininess.present);
  expect_true("shininess 64", desc.material_override.shininess.value == 64.0f);
  expect_true("empty slot present",
              desc.material_override.base_color_texture.present);
  expect_true("empty slot guid empty",
              desc.material_override.base_color_texture.guid.empty());
  expect_true("metallic absent", !desc.material_override.metallic.present);
  expect_true("unknown root preserved",
              desc.unknown_root_fields.size() == 1 &&
                  desc.unknown_root_fields[0].first == "legacy_note");

  const eastl::string written = AssetYaml::serializeMeshDescriptor(desc);
  expect_true("serialize writes empty slot",
              written.find("base_color:") != eastl::string::npos);
  expect_true("serialize omits absent metallic",
              written.find("metallic:") == eastl::string::npos);
  expect_true("unknown key round-trips",
              written.find("legacy_note:") != eastl::string::npos);

  MeshAssetDescriptor round;
  expect_true("override round-trip parse",
              AssetYaml::parseMeshDescriptor(written, round));
  expect_true("round-trip shininess",
              round.material_override.shininess == desc.material_override.shininess);
  expect_true("round-trip empty slot",
              round.material_override.base_color_texture.present &&
                  round.material_override.base_color_texture.guid.empty());
  expect_true("round-trip unknown",
              round.unknown_root_fields.size() == 1 &&
                  round.unknown_root_fields[0].first == "legacy_note");
}

void rebuildTextureGuidsUnionAndEmptySlotKeepsImport() {
  using namespace Blunder;
  MeshAssetDescriptor desc;
  desc.guid = "20202020-2020-4020-8020-202020202020";
  desc.source = "resources/Models/Hero.gltf";
  desc.texture_guids.push_back("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
  desc.import_texture_guids = desc.texture_guids;
  desc.material_override.base_color_texture.present = true;
  desc.material_override.base_color_texture.guid.clear();
  rebuildMeshTextureGuids(desc);
  expect_true("empty slot keeps import guid",
              desc.texture_guids.size() == 1 &&
                  desc.texture_guids[0] ==
                      "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");

  desc.material_override.normal_texture.present = true;
  desc.material_override.normal_texture.guid =
      "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
  rebuildMeshTextureGuids(desc);
  expect_true("new slot guid unioned", desc.texture_guids.size() == 2);
  expect_true("union keeps import",
              desc.texture_guids[0] ==
                  "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
  expect_true("union adds override slot",
              desc.texture_guids[1] ==
                  "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb");
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
  roundTripMeshCompanionAnimationSources();
  parseMeshWithoutCompanionAnimationSourcesLeavesEmpty();
  roundTripAnimationClipDescriptor();
  roundTripAnimationClipDataConstantAndLinear();
  roundTripAnimationClipMethodKeys();
  rejectUnknownInterpolation();
  parseSparseMaterialOverrideAndEmptySlot();
  rebuildTextureGuidsUnionAndEmptySlotKeepsImport();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
