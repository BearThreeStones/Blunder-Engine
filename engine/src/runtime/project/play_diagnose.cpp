#include "runtime/project/play_diagnose.h"

#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/project/play_preflight.h"

namespace Blunder {
namespace {

Issue makeIssue(const char* code, IssueSeverity severity,
                const char* explanation) {
  Issue issue;
  issue.code = code;
  issue.severity = severity;
  issue.explanation = explanation;
  return issue;
}

}  // namespace

void appendPlayCameraIssues(const Scene& scene,
                            eastl::vector<Issue>& out_issues) {
  if (sceneAssetHasPlayCamera(scene)) {
    return;
  }
  out_issues.push_back(makeIssue(k_issue_play_missing_camera,
                                 IssueSeverity::error,
                                 "play entry scene has no Camera"));
}

void appendPlayCameraIssues(const SceneInstance& scene,
                            eastl::vector<Issue>& out_issues) {
  bool has_camera = false;
  scene.forEachCamera([&](EntityId id, const CameraComponent&) {
    if (!scene.isOmittedFromDocument(id) && scene.isActiveInHierarchy(id)) {
      has_camera = true;
    }
  });
  if (has_camera) {
    return;
  }
  out_issues.push_back(makeIssue(k_issue_play_missing_camera,
                                 IssueSeverity::error,
                                 "play entry scene has no Camera"));
}

void appendPlayScriptsIssues(const std::filesystem::path& project_root,
                             eastl::vector<Issue>& out_issues) {
  if (project_root.empty() || !projectHasScriptsCsproj(project_root)) {
    return;
  }
  if (!projectHasGameAssemblyOutput(project_root)) {
    out_issues.push_back(makeIssue(
        k_issue_scripts_missing_output, IssueSeverity::warning,
        "Scripts exist but game assembly output is missing"));
    return;
  }
  if (areProjectScriptsDirty(project_root)) {
    out_issues.push_back(makeIssue(
        k_issue_scripts_dirty, IssueSeverity::warning,
        "Scripts sources are newer than scripts output"));
  }
}

void diagnosePlayRuleSet(const Scene& scene,
                         const std::filesystem::path& project_root,
                         eastl::vector<Issue>& out_issues) {
  appendPlayCameraIssues(scene, out_issues);
  appendPlayScriptsIssues(project_root, out_issues);
}

void diagnosePlayRuleSet(const SceneInstance& scene,
                         const std::filesystem::path& project_root,
                         eastl::vector<Issue>& out_issues) {
  appendPlayCameraIssues(scene, out_issues);
  appendPlayScriptsIssues(project_root, out_issues);
}

}  // namespace Blunder
