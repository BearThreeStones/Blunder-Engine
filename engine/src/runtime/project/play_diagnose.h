#pragma once

#include "EASTL/vector.h"

#include <filesystem>

#include "runtime/project/authorship_issue.h"

namespace Blunder {

class Scene;
class SceneInstance;

/// Play-rule Diagnose: Camera + Scripts dirtiness. Never compiles or Cooks.
void diagnosePlayRuleSet(const Scene& scene,
                         const std::filesystem::path& project_root,
                         eastl::vector<Issue>& out_issues);

void diagnosePlayRuleSet(const SceneInstance& scene,
                         const std::filesystem::path& project_root,
                         eastl::vector<Issue>& out_issues);

void appendPlayCameraIssues(const Scene& scene, eastl::vector<Issue>& out_issues);
void appendPlayCameraIssues(const SceneInstance& scene,
                            eastl::vector<Issue>& out_issues);
void appendPlayScriptsIssues(const std::filesystem::path& project_root,
                             eastl::vector<Issue>& out_issues);

}  // namespace Blunder
