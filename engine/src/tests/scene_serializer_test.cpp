#include "runtime/core/log/log_system.h"
#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/reflection/variant.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/guid.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <chrono>
#include <cmath>
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

bool float_near(float a, float b, float epsilon = 1e-5f) {
  return std::fabs(a - b) <= epsilon;
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
      ("blunder_scene_serializer_test_" +
       std::to_string(
           static_cast<unsigned long long>(
               std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Scenes");
  return root;
}

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

/// Round-trip scene top-level guid through SceneSerializer.
void serializeAndParseSceneGuid() {
  using namespace Blunder;
  ensureLogger();

  const char* kGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";

  Scene scene;
  scene.setGuid(eastl::string(kGuid));

  eastl::string json;
  expect_true("serialize scene with guid",
              SceneSerializer::serialize(scene, json));
  expect_true("serialized json contains guid key",
              json.find("\"guid\"") != eastl::string::npos);
  expect_true("serialized json contains guid value",
              json.find(kGuid) != eastl::string::npos);

  Scene loaded;
  expect_true("deserialize scene with guid",
              SceneSerializer::deserialize(json, loaded));
  expect_true("parsed guid matches", loaded.getGuid() == kGuid);
}

/// Entity mesh that is already a GUID is persisted as that GUID.
void serializeMeshAsGuid() {
  using namespace Blunder;
  ensureLogger();

  const char* kMeshGuid = "11111111-2222-4333-8444-555555555555";

  Scene scene;
  scene.setGuid("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");
  SceneEntityDefinition entity;
  entity.name = "Cube";
  entity.mesh_virtual_path = kMeshGuid;
  scene.getEntities().push_back(eastl::move(entity));

  eastl::string json;
  expect_true("serialize entity mesh guid",
              SceneSerializer::serialize(scene, json));
  const eastl::string mesh_field =
      eastl::string("\"mesh\": \"") + kMeshGuid + "\"";
  expect_true("json mesh field is guid",
              json.find(mesh_field) != eastl::string::npos);
  expect_true("json mesh field is not a path",
              json.find("assets/") == eastl::string::npos);

  Scene loaded;
  expect_true("deserialize mesh guid",
              SceneSerializer::deserialize(json, loaded));
  expect_true("one entity after deserialize",
              loaded.getEntities().size() == 1);
  expect_true("mesh field remains guid",
              loaded.getEntities()[0].mesh_virtual_path == kMeshGuid);
}

/// Legacy path mesh resolves via registry on load; save rewrites as GUID.
void loadLegacyPathMeshMigratesToGuidOnSave() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kMeshGuid = "abcdef01-2345-4789-8abc-def012345678";
  const char* kMeshPath = "assets/Meshes/Cube.mesh.yaml";

  writeTextFile(project / "Assets" / "Meshes" / "Cube.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kMeshGuid + "\n" +
                    "source: Resources/Models/Cube.gltf\n" +
                    "import:\n"
                    "  materials: true\n"
                    "  animations: false\n"
                    "  scale: 1.0\n");

  FileSystem file_system;
  FileSystemInitInfo init;
  init.project_root = project;
  file_system.initialize(init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();
  expect_true("mesh registered for path",
              registry.findGuidForPath(kMeshPath) == kMeshGuid);

  const std::string legacy_json =
      std::string("{\n") + "  \"type\": \"Scene\",\n" +
      "  \"guid\": \"aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee\",\n" +
      "  \"entities\": [\n" + "    {\n" + "      \"name\": \"Cube\",\n" +
      "      \"position\": [0, 0, 0],\n" + "      \"rotation\": [0, 0, 0],\n" +
      "      \"rotationMode\": \"euler_degrees\",\n" + "      \"mesh\": \"" +
      kMeshPath + "\"\n" + "    }\n" + "  ]\n" + "}\n";

  Scene loaded;
  expect_true("deserialize legacy path mesh with registry",
              SceneSerializer::deserialize(eastl::string(legacy_json.c_str()),
                                           loaded, &registry));
  expect_true("legacy mesh migrated to guid in memory",
              loaded.getEntities().size() == 1 &&
                  loaded.getEntities()[0].mesh_virtual_path == kMeshGuid);

  eastl::string saved;
  expect_true("serialize migrated scene with registry",
              SceneSerializer::serialize(loaded, saved, &registry));
  expect_true("saved mesh is guid not path",
              saved.find(kMeshGuid) != eastl::string::npos &&
                  saved.find(kMeshPath) == eastl::string::npos);

  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

/// Round-trip ordered Behaviour declarations (type, id, property bag).
void serializeAndParseBehaviours() {
  using namespace Blunder;
  ensureLogger();

  Scene scene;
  scene.setGuid("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");

  SceneEntityDefinition entity;
  entity.name = "Actor";

  SceneBehaviourDeclaration motor;
  motor.type = "Game.Motor";
  motor.id = static_cast<BehaviourId>(1);
  motor.properties.push_back({"speed", Variant(1.5f)});
  motor.properties.push_back({"enabled", Variant(true)});
  motor.properties.push_back({"tag", Variant(eastl::string("a"))});

  SceneBehaviourDeclaration bark;
  bark.type = "Game.Bark";
  bark.id = static_cast<BehaviourId>(2);

  entity.behaviours.push_back(eastl::move(motor));
  entity.behaviours.push_back(eastl::move(bark));
  scene.getEntities().push_back(eastl::move(entity));

  eastl::string json;
  expect_true("serialize scene with behaviours",
              SceneSerializer::serialize(scene, json));
  expect_true("json contains behaviours key",
              json.find("\"behaviours\"") != eastl::string::npos);
  expect_true("json contains Motor type",
              json.find("Game.Motor") != eastl::string::npos);
  expect_true("json contains Bark type",
              json.find("Game.Bark") != eastl::string::npos);
  expect_true("json contains property speed",
              json.find("\"speed\"") != eastl::string::npos);

  Scene loaded;
  expect_true("deserialize scene with behaviours",
              SceneSerializer::deserialize(json, loaded));
  expect_true("one entity after behaviour deserialize",
              loaded.getEntities().size() == 1);

  const SceneEntityDefinition& out = loaded.getEntities()[0];
  expect_true("two behaviours restored", out.behaviours.size() == 2);
  expect_true("behaviour 0 type", out.behaviours[0].type == "Game.Motor");
  expect_true("behaviour 0 id", out.behaviours[0].id == static_cast<BehaviourId>(1));
  expect_true("behaviour 0 property count",
              out.behaviours[0].properties.size() == 3);
  expect_true("behaviour 0 speed key",
              out.behaviours[0].properties[0].key == "speed");
  expect_true("behaviour 0 speed value",
              out.behaviours[0].properties[0].value == Variant(1.5f));
  expect_true("behaviour 0 enabled",
              out.behaviours[0].properties[1].value == Variant(true));
  expect_true("behaviour 0 tag",
              out.behaviours[0].properties[2].value ==
                  Variant(eastl::string("a")));
  expect_true("behaviour 1 type", out.behaviours[1].type == "Game.Bark");
  expect_true("behaviour 1 id", out.behaviours[1].id == static_cast<BehaviourId>(2));
  expect_true("behaviour 1 empty properties",
              out.behaviours[1].properties.empty());
}

/// Legacy entities without a behaviours key deserialize to an empty list.
void deserializeLegacyEntityWithoutBehaviours() {
  using namespace Blunder;
  ensureLogger();

  const char* kLegacy = R"({
  "type": "Scene",
  "guid": "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee",
  "entities": [
    {
      "name": "Cube",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees"
    }
  ]
}
)";

  Scene loaded;
  expect_true("deserialize legacy entity without behaviours",
              SceneSerializer::deserialize(eastl::string(kLegacy), loaded));
  expect_true("legacy entity present", loaded.getEntities().size() == 1);
  expect_true("legacy behaviours empty",
              loaded.getEntities()[0].behaviours.empty());
}

/// Hand-authored JSON: properties."id" before behaviour "id", and entity name
/// equal to "behaviours", must not steal first-match findKey hits.
void deserializeBehavioursScopedKeyLookup() {
  using namespace Blunder;
  ensureLogger();

  const char* kJson = R"({
  "type": "Scene",
  "guid": "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee",
  "entities": [
    {
      "name": "behaviours",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees",
      "behaviours": [
        {
          "type": "Game.Motor",
          "properties": { "id": 999, "tag": "nested" },
          "id": 42
        }
      ]
    }
  ]
}
)";

  Scene loaded;
  expect_true("deserialize scoped key lookup json",
              SceneSerializer::deserialize(eastl::string(kJson), loaded));
  expect_true("scoped: one entity", loaded.getEntities().size() == 1);
  expect_true("scoped: entity name is behaviours",
              loaded.getEntities()[0].name == "behaviours");
  expect_true("scoped: one behaviour",
              loaded.getEntities()[0].behaviours.size() == 1);
  if (!loaded.getEntities().empty() &&
      loaded.getEntities()[0].behaviours.size() == 1) {
    const SceneBehaviourDeclaration& b = loaded.getEntities()[0].behaviours[0];
    expect_true("scoped: behaviour id is 42 not nested 999",
                b.id == static_cast<BehaviourId>(42));
    expect_true("scoped: type preserved", b.type == "Game.Motor");
    expect_true("scoped: nested property id kept as property",
                b.properties.size() == 2 && b.properties[0].key == "id" &&
                    b.properties[0].value ==
                        Variant(static_cast<int64_t>(999)));
  }
}

/// Entity with camera round-trips verticalFovDegrees/nearClip/farClip/isMain.
void serializeAndParseCamera() {
  using namespace Blunder;
  ensureLogger();

  Scene scene;
  scene.setGuid("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");

  SceneEntityDefinition entity;
  entity.name = "MainCamera";
  entity.has_camera = true;
  entity.camera.vertical_fov_degrees = 60.0f;
  entity.camera.near_clip = 0.25f;
  entity.camera.far_clip = 500.0f;
  entity.camera.is_main = true;
  scene.getEntities().push_back(eastl::move(entity));

  eastl::string json;
  expect_true("serialize scene with camera",
              SceneSerializer::serialize(scene, json));
  expect_true("json contains camera key",
              json.find("\"camera\"") != eastl::string::npos);
  expect_true("json contains verticalFovDegrees",
              json.find("\"verticalFovDegrees\"") != eastl::string::npos);
  expect_true("json contains nearClip",
              json.find("\"nearClip\"") != eastl::string::npos);
  expect_true("json contains farClip",
              json.find("\"farClip\"") != eastl::string::npos);
  expect_true("json contains isMain",
              json.find("\"isMain\"") != eastl::string::npos);

  Scene loaded;
  expect_true("deserialize scene with camera",
              SceneSerializer::deserialize(json, loaded));
  expect_true("one entity after camera deserialize",
              loaded.getEntities().size() == 1);

  const SceneEntityDefinition& out = loaded.getEntities()[0];
  expect_true("has_camera restored", out.has_camera);
  expect_true("verticalFovDegrees restored",
              out.camera.vertical_fov_degrees == 60.0f);
  expect_true("nearClip restored", out.camera.near_clip == 0.25f);
  expect_true("farClip restored", out.camera.far_clip == 500.0f);
  expect_true("isMain restored", out.camera.is_main);
}

/// AnimationPlayer clip map + Skeleton presence round-trip; clip GUID list stays
/// consistent with the map for dependency-graph edges.
void serializeAndParseAnimationPlayerAndSkeleton() {
  using namespace Blunder;
  ensureLogger();

  const char* kIdleGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";
  const char* kWalkGuid = "11111111-2222-4333-8444-555555555555";

  Scene scene;
  scene.setGuid("cccccccc-cccc-4ccc-8ccc-cccccccccccc");

  SceneEntityDefinition entity;
  entity.name = "Dog";
  entity.has_skeleton = true;
  entity.animation_player_clips.push_back({"idle", eastl::string(kIdleGuid)});
  entity.animation_player_clips.push_back({"walk", eastl::string(kWalkGuid)});
  scene.getEntities().push_back(eastl::move(entity));

  eastl::string json;
  expect_true("serialize animation player + skeleton",
              SceneSerializer::serialize(scene, json));
  expect_true("json contains hasSkeleton",
              json.find("\"hasSkeleton\"") != eastl::string::npos);
  expect_true("json contains animationPlayer",
              json.find("\"animationPlayer\"") != eastl::string::npos);
  expect_true("json contains clips map idle",
              json.find("\"idle\"") != eastl::string::npos);
  expect_true("json contains animation_clip_guids",
              json.find("\"animation_clip_guids\"") != eastl::string::npos);
  expect_true("json clip guids include idle",
              json.find(kIdleGuid) != eastl::string::npos);
  expect_true("json clip guids include walk",
              json.find(kWalkGuid) != eastl::string::npos);

  Scene loaded;
  expect_true("deserialize animation player + skeleton",
              SceneSerializer::deserialize(json, loaded));
  expect_true("one entity after animation deserialize",
              loaded.getEntities().size() == 1);

  const SceneEntityDefinition& out = loaded.getEntities()[0];
  expect_true("has_skeleton restored", out.has_skeleton);
  expect_true("two clip bindings restored",
              out.animation_player_clips.size() == 2);
  if (out.animation_player_clips.size() == 2) {
    expect_true("idle binding name", out.animation_player_clips[0].name == "idle");
    expect_true("idle binding guid",
                out.animation_player_clips[0].guid == kIdleGuid);
    expect_true("walk binding name", out.animation_player_clips[1].name == "walk");
    expect_true("walk binding guid",
                out.animation_player_clips[1].guid == kWalkGuid);
  }
  expect_true("animation_clip_guids size matches map",
              out.animation_clip_guids.size() == 2);
  expect_true("animation_clip_guids include idle",
              out.animation_clip_guids.size() >= 1 &&
                  (out.animation_clip_guids[0] == kIdleGuid ||
                   out.animation_clip_guids[1] == kIdleGuid));
  expect_true("animation_clip_guids include walk",
              out.animation_clip_guids.size() >= 2 &&
                  (out.animation_clip_guids[0] == kWalkGuid ||
                   out.animation_clip_guids[1] == kWalkGuid));
}

/// Phase 2 defaults (TimeScale, slot0/slot1, blendWeight) round-trip with clips map.
void serializeAndParseAnimationPlayerPhase2Defaults() {
  using namespace Blunder;
  ensureLogger();

  const char* kIdleGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";
  const char* kWalkGuid = "11111111-2222-4333-8444-555555555555";

  Scene scene;
  scene.setGuid("cccccccc-cccc-4ccc-8ccc-cccccccccccc");

  SceneEntityDefinition entity;
  entity.name = "Dog";
  entity.has_skeleton = true;
  entity.animation_player_clips.push_back({"idle", eastl::string(kIdleGuid)});
  entity.animation_player_clips.push_back({"walk", eastl::string(kWalkGuid)});
  entity.animation_player_time_scale = 0.5f;
  entity.animation_player_slot0 = "idle";
  entity.animation_player_slot1 = "walk";
  entity.animation_player_blend_weight = 0.35f;
  scene.getEntities().push_back(eastl::move(entity));

  eastl::string json;
  expect_true("serialize animation player phase 2 defaults",
              SceneSerializer::serialize(scene, json));
  expect_true("json contains timeScale",
              json.find("\"timeScale\"") != eastl::string::npos);
  expect_true("json contains slot0",
              json.find("\"slot0\"") != eastl::string::npos);
  expect_true("json contains slot1",
              json.find("\"slot1\"") != eastl::string::npos);
  expect_true("json contains blendWeight",
              json.find("\"blendWeight\"") != eastl::string::npos);

  Scene loaded;
  expect_true("deserialize animation player phase 2 defaults",
              SceneSerializer::deserialize(json, loaded));
  expect_true("one entity after phase 2 defaults deserialize",
              loaded.getEntities().size() == 1);

  const SceneEntityDefinition& out = loaded.getEntities()[0];
  expect_true("timeScale restored",
              float_near(out.animation_player_time_scale, 0.5f));
  expect_true("slot0 restored", out.animation_player_slot0 == "idle");
  expect_true("slot1 restored", out.animation_player_slot1 == "walk");
  expect_true("blendWeight restored",
              float_near(out.animation_player_blend_weight, 0.35f));
  expect_true("clips map still restored",
              out.animation_player_clips.size() == 2);
}

/// Legacy entities without animation keys deserialize with defaults.
void deserializeLegacyEntityWithoutAnimation() {
  using namespace Blunder;
  ensureLogger();

  const char* kLegacy = R"({
  "type": "Scene",
  "guid": "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee",
  "entities": [
    {
      "name": "Cube",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees"
    }
  ]
}
)";

  Scene loaded;
  expect_true("deserialize legacy entity without animation",
              SceneSerializer::deserialize(eastl::string(kLegacy), loaded));
  expect_true("legacy entity present", loaded.getEntities().size() == 1);
  expect_true("legacy has_skeleton false",
              !loaded.getEntities()[0].has_skeleton);
  expect_true("legacy animation_player_clips empty",
              loaded.getEntities()[0].animation_player_clips.empty());
  expect_true("legacy animation_clip_guids empty",
              loaded.getEntities()[0].animation_clip_guids.empty());
  expect_true("legacy timeScale default",
              float_near(loaded.getEntities()[0].animation_player_time_scale, 1.0f));
  expect_true("legacy slot0 empty",
              loaded.getEntities()[0].animation_player_slot0.empty());
  expect_true("legacy slot1 empty",
              loaded.getEntities()[0].animation_player_slot1.empty());
  expect_true("legacy blendWeight default",
              float_near(loaded.getEntities()[0].animation_player_blend_weight, 0.0f));
}

/// Legacy entities without a camera key deserialize with has_camera false.
void deserializeLegacyEntityWithoutCamera() {
  using namespace Blunder;
  ensureLogger();

  const char* kLegacy = R"({
  "type": "Scene",
  "guid": "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee",
  "entities": [
    {
      "name": "Cube",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees"
    }
  ]
}
)";

  Scene loaded;
  expect_true("deserialize legacy entity without camera",
              SceneSerializer::deserialize(eastl::string(kLegacy), loaded));
  expect_true("legacy entity present", loaded.getEntities().size() == 1);
  expect_true("legacy has_camera false",
              !loaded.getEntities()[0].has_camera);
}

/// Quotes / backslashes in type names and string property values must round-trip.
void serializeAndParseEscapedBehaviourStrings() {
  using namespace Blunder;
  ensureLogger();

  Scene scene;
  scene.setGuid("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");

  SceneEntityDefinition entity;
  entity.name = "Actor";

  SceneBehaviourDeclaration quoted;
  quoted.type = "Game.\"Quoted\"";
  quoted.id = static_cast<BehaviourId>(7);
  quoted.properties.push_back(
      {"msg", Variant(eastl::string("say \"hi\"\\there"))});
  quoted.properties.push_back(
      {"path", Variant(eastl::string("C:\\temp\\file"))});
  entity.behaviours.push_back(eastl::move(quoted));
  scene.getEntities().push_back(eastl::move(entity));

  eastl::string json;
  expect_true("serialize escaped behaviour strings",
              SceneSerializer::serialize(scene, json));
  expect_true("json escapes quote in type",
              json.find("Game.\\\"Quoted\\\"") != eastl::string::npos);
  expect_true("json escapes quote in property",
              json.find("say \\\"hi\\\"") != eastl::string::npos);
  expect_true("json escapes backslash in property",
              json.find("C:\\\\temp\\\\file") != eastl::string::npos);

  Scene loaded;
  expect_true("deserialize escaped behaviour strings",
              SceneSerializer::deserialize(json, loaded));
  expect_true("escaped: one entity", loaded.getEntities().size() == 1);
  expect_true(
      "escaped: one behaviour",
      !loaded.getEntities().empty() &&
          loaded.getEntities()[0].behaviours.size() == 1);
  if (!loaded.getEntities().empty() &&
      loaded.getEntities()[0].behaviours.size() == 1) {
    const SceneBehaviourDeclaration& out = loaded.getEntities()[0].behaviours[0];
    expect_true("escaped: type unescaped", out.type == "Game.\"Quoted\"");
    expect_true("escaped: id", out.id == static_cast<BehaviourId>(7));
    expect_true("escaped: msg property",
                out.properties.size() >= 2 &&
                    out.properties[0].value ==
                        Variant(eastl::string("say \"hi\"\\there")));
    expect_true("escaped: path property",
                out.properties[1].value ==
                    Variant(eastl::string("C:\\temp\\file")));
  }
}

}  // namespace

int main() {
  serializeAndParseSceneGuid();
  serializeMeshAsGuid();
  loadLegacyPathMeshMigratesToGuidOnSave();
  serializeAndParseBehaviours();
  deserializeLegacyEntityWithoutBehaviours();
  deserializeBehavioursScopedKeyLookup();
  serializeAndParseEscapedBehaviourStrings();
  serializeAndParseCamera();
  serializeAndParseAnimationPlayerAndSkeleton();
  serializeAndParseAnimationPlayerPhase2Defaults();
  deserializeLegacyEntityWithoutAnimation();
  deserializeLegacyEntityWithoutCamera();

  const int exit_code = g_failures != 0 ? 1 : 0;
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
  } else {
    std::fprintf(stdout, "scene_serializer_test: all passed\n");
  }

  Blunder::g_runtime_global_context.m_logger_system.reset();
  return exit_code;
}
