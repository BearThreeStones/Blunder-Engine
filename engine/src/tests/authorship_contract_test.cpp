#include "runtime/core/log/log_system.h"
#include "runtime/function/editor/authorship_system.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/project/play_diagnose.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
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

}  // namespace

int main() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }

  expect_true("editor mounts authorship",
              authorshipSystemEnabled(EngineHostMode::Editor));
  expect_true("player skips authorship",
              !authorshipSystemEnabled(EngineHostMode::Player));

  {
    SceneInstance scene;
    DocumentHistory history;
    AuthorshipSystem authorship;
    authorship.setTestLiveDocument(&scene, &history);

    const EntityId id = scene.createEntity(
        "Player", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1, 1, 1));
    expect_true("created player", isValid(id));

    eastl::vector<eastl::string> names;
    AuthorshipStatus listed =
        authorship.queryNames(AuthorshipSubject::live, {}, names);
    expect_true("live list ok", listed.ok);
    expect_true("live list has Player",
                names.size() == 1 && names[0] == "Player");

    AuthorshipEntityQuery got;
    AuthorshipStatus fetched = authorship.queryEntity(
        AuthorshipSubject::live, {}, eastl::string("Player"), got);
    expect_true("live get ok", fetched.ok);
    expect_true("live get name", got.name == "Player");
    expect_true("live get root parent empty", got.parent_name.empty());

    AuthorshipStatus moved = authorship.setTransform(
        AuthorshipSubject::live, eastl::string("Player"), Vec3(1, 0, 0),
        glm::identity<Quat>(), Vec3(1, 1, 1));
    expect_true("transform op ok", moved.ok);
    expect_true("one command", history.commandCount() == 1);
    expect_true("pos applied",
                scene.getEntity(id)->getPosition() == Vec3(1, 0, 0));
    expect_true("undo transform", history.undo());
    expect_true("pos restored",
                scene.getEntity(id)->getPosition() == Vec3(0, 0, 0));

    AuthorshipStatus disk_op = authorship.setTransform(
        AuthorshipSubject::onDisk, eastl::string("Player"), Vec3(2, 0, 0),
        glm::identity<Quat>(), Vec3(1, 1, 1));
    expect_true("op on disk fails", !disk_op.ok);
    expect_true("op on disk live required",
                disk_op.failure_code == k_request_subject_live_required);
    expect_true("op on disk no extra command", history.commandCount() == 1);

    AuthorshipStatus unknown = authorship.queryEntity(
        AuthorshipSubject::live, {}, eastl::string("Nope"), got);
    expect_true("unknown name fails", !unknown.ok);
    expect_true("unknown code",
                unknown.failure_code == k_request_address_unknown);

    AuthorshipStatus empty_name = authorship.queryEntity(
        AuthorshipSubject::live, {}, eastl::string(), got);
    expect_true("empty name unknown",
                !empty_name.ok &&
                    empty_name.failure_code == k_request_address_unknown);

    scene.softDeleteEntity(id);
    names.clear();
    authorship.queryNames(AuthorshipSubject::live, {}, names);
    expect_true("tombstone omitted from list", names.empty());
    AuthorshipStatus tomb = authorship.queryEntity(
        AuthorshipSubject::live, {}, eastl::string("Player"), got);
    expect_true("tombstone unknown",
                !tomb.ok && tomb.failure_code == k_request_address_unknown);
  }

  {
    SceneInstance scene;
    DocumentHistory history;
    AuthorshipSystem authorship;
    authorship.setTestLiveDocument(&scene, &history);
    scene.createEntity("Cube", Vec3(0, 0, 0), glm::identity<Quat>(),
                       Vec3(1, 1, 1));
    eastl::vector<Issue> issues;
    AuthorshipStatus diagnosed =
        authorship.diagnosePlay(AuthorshipSubject::live, {}, issues);
    expect_true("live diagnose ok", diagnosed.ok);
    expect_true("live missing camera",
                issueListHasCode(issues, k_issue_play_missing_camera));
  }

  {
    SceneInstance scene;
    DocumentHistory history;
    AuthorshipSystem authorship;
    authorship.setTestLiveDocument(&scene, &history);
    const EntityId cam = scene.createEntity(
        "Main Camera", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1, 1, 1));
    CameraComponent camera{};
    camera.is_main = true;
    scene.setCamera(cam, camera);
    eastl::vector<Issue> issues;
    authorship.diagnosePlay(AuthorshipSubject::live, {}, issues);
    expect_true("live with camera no missing",
                !issueListHasCode(issues, k_issue_play_missing_camera));
  }

  {
    AuthorshipSystem authorship;
    eastl::vector<eastl::string> names;
    AuthorshipStatus listed =
        authorship.queryNames(AuthorshipSubject::live, {}, names);
    expect_true("no live document", !listed.ok);
    expect_true("no live code",
                listed.failure_code == k_request_subject_no_live_document);
  }

  {
    const auto stamp =
        static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path root =
        fs::temp_directory_path() /
        ("blunder_authorship_" + std::to_string(stamp));
    fs::create_directories(root / "Assets" / "Scenes");

    Scene scene;
    SceneEntityDefinition player;
    player.name = "Player";
    player.position = Vec3(1, 0, 0);
    scene.getEntities().push_back(player);

    eastl::string json;
    expect_true("serialize disk scene",
                SceneSerializer::serialize(scene, json, nullptr));

    FileSystem file_system;
    FileSystemInitInfo fs_init{};
    fs_init.project_root = root;
    file_system.initialize(fs_init);
    const eastl::string virtual_path = "assets/Scenes/t.scene.asset";
    const fs::path absolute = root / "Assets" / "Scenes" / "t.scene.asset";
    expect_true("write disk scene",
                file_system.writeText(absolute, json));

    eastl::vector<eastl::string> names;
    AuthorshipStatus listed =
        queryOnDiskNames(&file_system, virtual_path, names);
    expect_true("disk list ok", listed.ok);
    expect_true("disk list Player",
                names.size() == 1 && names[0] == "Player");

    AuthorshipEntityQuery got;
    AuthorshipStatus fetched =
        queryOnDiskEntity(&file_system, virtual_path, eastl::string("Player"),
                          got);
    expect_true("disk get ok", fetched.ok);
    expect_true("disk get translation", got.position == Vec3(1, 0, 0));

    eastl::vector<Issue> issues;
    AuthorshipStatus diagnosed =
        diagnoseOnDiskPlay(&file_system, virtual_path, root, issues);
    expect_true("disk diagnose ok", diagnosed.ok);
    expect_true("disk missing camera",
                issueListHasCode(issues, k_issue_play_missing_camera));

    AuthorshipStatus missing = queryOnDiskNames(
        &file_system, eastl::string("assets/Scenes/nope.scene.asset"), names);
    expect_true("unreadable scene", !missing.ok);
    expect_true("unreadable code",
                missing.failure_code == k_request_subject_scene_unreadable);

    file_system.shutdown();
    fs::remove_all(root);
  }

  g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
