#include "runtime/function/editor/authorship_system.h"

#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/project/play_diagnose.h"

namespace Blunder {
namespace {

std::filesystem::path resolveSceneAssetAbsolute(
    FileSystem* file_system, const eastl::string& virtual_path) {
  if (file_system == nullptr || virtual_path.empty()) {
    return {};
  }
  const eastl::string prefix_assets = "assets/";
  if (virtual_path.size() > prefix_assets.size() &&
      virtual_path.compare(0, prefix_assets.size(), prefix_assets) == 0) {
    const eastl::string relative =
        virtual_path.substr(prefix_assets.size(), virtual_path.size());
    return file_system->resolveAsset(std::filesystem::path(relative.c_str()));
  }
  return file_system->resolveAsset(std::filesystem::path(virtual_path.c_str()));
}

bool entityAddressable(const SceneInstance& scene, EntityId id) {
  const Entity* entity = scene.getEntity(id);
  if (entity == nullptr || entity->getName().empty()) {
    return false;
  }
  return !scene.isOmittedFromDocument(id);
}

void fillQueryFromEntity(const SceneInstance& scene, const Entity& entity,
                         AuthorshipEntityQuery& out) {
  out.name = entity.getName();
  out.parent_name.clear();
  const EntityId parent_id = entity.getParentId();
  if (isValid(parent_id)) {
    if (const Entity* parent = scene.getEntity(parent_id)) {
      out.parent_name = parent->getName();
    }
  }
  out.position = entity.getPosition();
  out.rotation = entity.getRotation();
  out.scale = entity.getScale();
}

void fillQueryFromDefinition(const SceneEntityDefinition& definition,
                             AuthorshipEntityQuery& out) {
  out.name = definition.name;
  out.parent_name = definition.parent_name;
  out.position = definition.position;
  out.rotation = definition.rotation;
  out.scale = definition.scale;
}

const SceneEntityDefinition* findNamedDefinition(const Scene& scene,
                                                 const eastl::string& name) {
  if (name.empty()) {
    return nullptr;
  }
  for (const SceneEntityDefinition& entity : scene.getEntities()) {
    if (entity.name == name) {
      return &entity;
    }
  }
  return nullptr;
}

}  // namespace

AuthorshipStatus AuthorshipSystem::fail(const char* code) const {
  AuthorshipStatus status;
  status.ok = false;
  status.failure_code = code;
  return status;
}

AuthorshipStatus loadOnDiskScene(FileSystem* file_system,
                                 const eastl::string& scene_virtual_path,
                                 Scene& out_scene) {
  AuthorshipStatus status;
  if (file_system == nullptr || scene_virtual_path.empty()) {
    status.ok = false;
    status.failure_code = k_request_subject_scene_unreadable;
    return status;
  }
  const std::filesystem::path absolute =
      resolveSceneAssetAbsolute(file_system, scene_virtual_path);
  eastl::string json_text;
  if (absolute.empty() || !file_system->readText(absolute, json_text) ||
      !SceneSerializer::deserialize(json_text, out_scene, nullptr)) {
    status.ok = false;
    status.failure_code = k_request_subject_scene_unreadable;
    return status;
  }
  return status;
}

AuthorshipStatus queryOnDiskNames(FileSystem* file_system,
                                  const eastl::string& scene_virtual_path,
                                  eastl::vector<eastl::string>& out_names) {
  out_names.clear();
  Scene scene;
  const AuthorshipStatus loaded =
      loadOnDiskScene(file_system, scene_virtual_path, scene);
  if (!loaded.ok) {
    return loaded;
  }
  for (const SceneEntityDefinition& entity : scene.getEntities()) {
    if (!entity.name.empty()) {
      out_names.push_back(entity.name);
    }
  }
  return {};
}

AuthorshipStatus queryOnDiskEntity(FileSystem* file_system,
                                   const eastl::string& scene_virtual_path,
                                   const eastl::string& name,
                                   AuthorshipEntityQuery& out_entity) {
  Scene scene;
  const AuthorshipStatus loaded =
      loadOnDiskScene(file_system, scene_virtual_path, scene);
  if (!loaded.ok) {
    return loaded;
  }
  const SceneEntityDefinition* definition = findNamedDefinition(scene, name);
  if (definition == nullptr) {
    AuthorshipStatus status;
    status.ok = false;
    status.failure_code = k_request_address_unknown;
    return status;
  }
  fillQueryFromDefinition(*definition, out_entity);
  return {};
}

AuthorshipStatus diagnoseOnDiskPlay(FileSystem* file_system,
                                    const eastl::string& scene_virtual_path,
                                    const std::filesystem::path& project_root,
                                    eastl::vector<Issue>& out_issues) {
  out_issues.clear();
  Scene scene;
  const AuthorshipStatus loaded =
      loadOnDiskScene(file_system, scene_virtual_path, scene);
  if (!loaded.ok) {
    return loaded;
  }
  diagnosePlayRuleSet(scene, project_root, out_issues);
  return {};
}

void AuthorshipSystem::initialize(SceneSystem* scenes, DocumentHistory* history,
                                  FileSystem* fs,
                                  EditorSelectionSystem* selection,
                                  EditorSceneEditSystem* scene_edit) {
  m_scenes = scenes;
  m_history = history;
  m_file_system = fs;
  m_selection = selection;
  m_scene_edit = scene_edit;
}

void AuthorshipSystem::setTestLiveDocument(SceneInstance* scene,
                                           DocumentHistory* history) {
  m_test_scene = scene;
  m_test_history = history;
}

SceneInstance* AuthorshipSystem::liveScene() const {
  if (m_test_scene != nullptr) {
    return m_test_scene;
  }
  return m_scenes != nullptr ? m_scenes->getActiveInstance() : nullptr;
}

DocumentHistory* AuthorshipSystem::history() const {
  if (m_test_history != nullptr) {
    return m_test_history;
  }
  return m_history;
}

AuthorshipStatus AuthorshipSystem::queryLiveNames(
    eastl::vector<eastl::string>& out_names) {
  out_names.clear();
  SceneInstance* scene = liveScene();
  if (scene == nullptr) {
    return fail(k_request_subject_no_live_document);
  }
  scene->forEachEntity([&](EntityId id, const Entity& entity) {
    if (entityAddressable(*scene, id)) {
      out_names.push_back(entity.getName());
    }
  });
  return {};
}

AuthorshipStatus AuthorshipSystem::queryLiveEntity(
    const eastl::string& name, AuthorshipEntityQuery& out_entity) {
  SceneInstance* scene = liveScene();
  if (scene == nullptr) {
    return fail(k_request_subject_no_live_document);
  }
  if (name.empty()) {
    return fail(k_request_address_unknown);
  }
  const EntityId id = scene->findEntityByName(name);
  if (!entityAddressable(*scene, id)) {
    return fail(k_request_address_unknown);
  }
  fillQueryFromEntity(*scene, *scene->getEntity(id), out_entity);
  return {};
}

AuthorshipStatus AuthorshipSystem::queryNames(
    AuthorshipSubject subject, const eastl::string& scene_virtual_path,
    eastl::vector<eastl::string>& out_names) {
  if (subject == AuthorshipSubject::onDisk) {
    return queryOnDiskNames(m_file_system, scene_virtual_path, out_names);
  }
  return queryLiveNames(out_names);
}

AuthorshipStatus AuthorshipSystem::queryEntity(
    AuthorshipSubject subject, const eastl::string& scene_virtual_path,
    const eastl::string& name, AuthorshipEntityQuery& out_entity) {
  if (subject == AuthorshipSubject::onDisk) {
    return queryOnDiskEntity(m_file_system, scene_virtual_path, name,
                             out_entity);
  }
  return queryLiveEntity(name, out_entity);
}

AuthorshipStatus AuthorshipSystem::setTransform(AuthorshipSubject subject,
                                                const eastl::string& name,
                                                const Vec3& position,
                                                const Quat& rotation,
                                                const Vec3& scale) {
  if (subject != AuthorshipSubject::live) {
    return fail(k_request_subject_live_required);
  }
  SceneInstance* scene = liveScene();
  DocumentHistory* document = history();
  if (scene == nullptr || document == nullptr) {
    return fail(k_request_subject_no_live_document);
  }
  if (name.empty()) {
    return fail(k_request_address_unknown);
  }
  const EntityId id = scene->findEntityByName(name);
  if (!entityAddressable(*scene, id)) {
    return fail(k_request_address_unknown);
  }
  Entity* entity = scene->getEntity(id);
  const Vec3 before_position = entity->getPosition();
  const Quat before_rotation = entity->getRotation();
  const Vec3 before_scale = entity->getScale();
  entity->setPosition(position);
  entity->setRotation(rotation);
  entity->setScale(scale);

  SelectionSnapshot selection{};
  if (m_selection != nullptr && m_selection->hasSelection()) {
    selection.primary = m_selection->getSelection();
  } else {
    selection.primary = id;
  }
  document->push(makeSetEntityTransformCommand(
      scene, id, before_position, before_rotation, before_scale, position,
      rotation, scale, selection, selection));
  if (m_scene_edit != nullptr) {
    m_scene_edit->markDirty();
  }
  return {};
}

AuthorshipStatus AuthorshipSystem::diagnosePlay(
    AuthorshipSubject subject, const eastl::string& scene_virtual_path,
    eastl::vector<Issue>& out_issues) {
  out_issues.clear();
  if (subject == AuthorshipSubject::onDisk) {
    const std::filesystem::path project_root =
        m_file_system != nullptr ? m_file_system->getProjectRoot()
                                 : std::filesystem::path{};
    return diagnoseOnDiskPlay(m_file_system, scene_virtual_path, project_root,
                              out_issues);
  }
  SceneInstance* scene = liveScene();
  if (scene == nullptr) {
    return fail(k_request_subject_no_live_document);
  }
  const std::filesystem::path project_root =
      m_file_system != nullptr ? m_file_system->getProjectRoot()
                               : std::filesystem::path{};
  diagnosePlayRuleSet(*scene, project_root, out_issues);
  return {};
}

}  // namespace Blunder
