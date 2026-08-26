#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/global/engine_host_mode.h"
#include "runtime/project/authorship_issue.h"

#include <filesystem>

namespace Blunder {

class DocumentHistory;
class EditorSceneEditSystem;
class EditorSelectionSystem;
class FileSystem;
class Scene;
class SceneInstance;
class SceneSystem;

enum class AuthorshipSubject : uint8_t {
  live = 0,
  onDisk = 1,
};

struct AuthorshipStatus final {
  bool ok{true};
  eastl::string failure_code;
};

struct AuthorshipEntityQuery final {
  eastl::string name;
  eastl::string parent_name;
  Vec3 position{0.0f};
  Quat rotation{glm::identity<Quat>()};
  Vec3 scale{1.0f, 1.0f, 1.0f};
};

inline bool authorshipSystemEnabled(EngineHostMode host_mode) {
  return host_mode == EngineHostMode::Editor;
}

/// Load a Scene Asset from disk by Play-entry virtual path. No Authorship System.
AuthorshipStatus loadOnDiskScene(FileSystem* file_system,
                                 const eastl::string& scene_virtual_path,
                                 Scene& out_scene);

AuthorshipStatus queryOnDiskNames(FileSystem* file_system,
                                  const eastl::string& scene_virtual_path,
                                  eastl::vector<eastl::string>& out_names);

AuthorshipStatus queryOnDiskEntity(FileSystem* file_system,
                                   const eastl::string& scene_virtual_path,
                                   const eastl::string& name,
                                   AuthorshipEntityQuery& out_entity);

AuthorshipStatus diagnoseOnDiskPlay(FileSystem* file_system,
                                    const eastl::string& scene_virtual_path,
                                    const std::filesystem::path& project_root,
                                    eastl::vector<Issue>& out_issues);

/// Editor-only Registered System hosting the Authorship contract.
class AuthorshipSystem final {
 public:
  void initialize(SceneSystem* scenes, DocumentHistory* history, FileSystem* fs,
                  EditorSelectionSystem* selection,
                  EditorSceneEditSystem* scene_edit = nullptr);

  /// Unit tests: bypass SceneSystem and use a local SceneInstance + History.
  void setTestLiveDocument(SceneInstance* scene, DocumentHistory* history);

  AuthorshipStatus queryNames(AuthorshipSubject subject,
                              const eastl::string& scene_virtual_path,
                              eastl::vector<eastl::string>& out_names);

  AuthorshipStatus queryEntity(AuthorshipSubject subject,
                               const eastl::string& scene_virtual_path,
                               const eastl::string& name,
                               AuthorshipEntityQuery& out_entity);

  AuthorshipStatus setTransform(AuthorshipSubject subject,
                                const eastl::string& name, const Vec3& position,
                                const Quat& rotation, const Vec3& scale);

  AuthorshipStatus diagnosePlay(AuthorshipSubject subject,
                                const eastl::string& scene_virtual_path,
                                eastl::vector<Issue>& out_issues);

 private:
  SceneInstance* liveScene() const;
  DocumentHistory* history() const;
  AuthorshipStatus fail(const char* code) const;
  AuthorshipStatus queryLiveNames(eastl::vector<eastl::string>& out_names);
  AuthorshipStatus queryLiveEntity(const eastl::string& name,
                                   AuthorshipEntityQuery& out_entity);

  SceneSystem* m_scenes{nullptr};
  DocumentHistory* m_history{nullptr};
  FileSystem* m_file_system{nullptr};
  EditorSelectionSystem* m_selection{nullptr};
  EditorSceneEditSystem* m_scene_edit{nullptr};
  SceneInstance* m_test_scene{nullptr};
  DocumentHistory* m_test_history{nullptr};
};

}  // namespace Blunder
