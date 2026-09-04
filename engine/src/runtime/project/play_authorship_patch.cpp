#include "runtime/project/play_authorship_patch.h"

#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/project/play_preflight.h"
#include "runtime/project/play_session_controller.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <string>
#include <string_view>

namespace Blunder {
namespace {

std::string jsonEscape(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

void appendJsonFloatArray(std::ostringstream& oss, const float* values,
                          size_t count) {
  oss << '[';
  for (size_t i = 0; i < count; ++i) {
    if (i > 0) {
      oss << ',';
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(values[i]));
    oss << buf;
  }
  oss << ']';
}

bool extractJsonStringField(const std::string& json, const char* key,
                            std::string& out) {
  const std::string needle = std::string("\"") + key + "\":";
  const size_t start = json.find(needle);
  if (start == std::string::npos) {
    return false;
  }
  size_t i = start + needle.size();
  while (i < json.size() &&
         std::isspace(static_cast<unsigned char>(json[i]))) {
    ++i;
  }
  if (i >= json.size() || json[i] != '"') {
    return false;
  }
  ++i;
  std::string value;
  while (i < json.size()) {
    const char c = json[i];
    if (c == '"') {
      out = std::move(value);
      return true;
    }
    if (c == '\\' && i + 1 < json.size()) {
      value += json[i + 1];
      i += 2;
      continue;
    }
    value += c;
    ++i;
  }
  return false;
}

bool extractJsonBoolField(const std::string& json, const char* key, bool& out) {
  const std::string needle = std::string("\"") + key + "\":";
  const size_t start = json.find(needle);
  if (start == std::string::npos) {
    return false;
  }
  size_t i = start + needle.size();
  while (i < json.size() &&
         std::isspace(static_cast<unsigned char>(json[i]))) {
    ++i;
  }
  if (i + 4 <= json.size() && json.compare(i, 4, "true") == 0) {
    out = true;
    return true;
  }
  if (i + 5 <= json.size() && json.compare(i, 5, "false") == 0) {
    out = false;
    return true;
  }
  return false;
}

bool extractJsonFloatArray(const std::string& json, const char* key, float* out,
                           size_t count) {
  if (out == nullptr || count == 0) {
    return false;
  }
  const std::string needle = std::string("\"") + key + "\":";
  const size_t start = json.find(needle);
  if (start == std::string::npos) {
    return false;
  }
  size_t i = start + needle.size();
  while (i < json.size() &&
         std::isspace(static_cast<unsigned char>(json[i]))) {
    ++i;
  }
  if (i >= json.size() || json[i] != '[') {
    return false;
  }
  ++i;
  for (size_t n = 0; n < count; ++n) {
    while (i < json.size() &&
           (std::isspace(static_cast<unsigned char>(json[i])) ||
            json[i] == ',')) {
      ++i;
    }
    char* end = nullptr;
    const float value = std::strtof(json.c_str() + i, &end);
    if (end == json.c_str() + i) {
      return false;
    }
    out[n] = value;
    i = static_cast<size_t>(end - json.c_str());
  }
  return true;
}

eastl::string liveDocumentGuid() {
  EditorSceneEditSystem* edit =
      g_runtime_global_context.m_editor_scene_edit.get();
  AssetRegistry* registry = g_runtime_global_context.m_asset_registry.get();
  if (edit == nullptr || registry == nullptr) {
    return {};
  }
  return registry->findGuidForPath(edit->activeScenePath());
}

}  // namespace

std::string buildPlayAuthorshipPatchJson(const SceneInstance& scene,
                                         EntityId entity_id) {
  const Entity* entity = scene.getEntity(entity_id);
  if (entity == nullptr || entity->getName().empty()) {
    return {};
  }
  const Vec3& t = entity->getPosition();
  const Quat& r = entity->getRotation();
  const Vec3& s = entity->getScale();
  const float t_arr[3] = {t.x, t.y, t.z};
  const float r_arr[4] = {r.x, r.y, r.z, r.w};
  const float s_arr[3] = {s.x, s.y, s.z};

  std::ostringstream oss;
  oss << "{\"address\":\"" << jsonEscape(entity->getName().c_str())
      << "\",\"local\":{\"t\":";
  appendJsonFloatArray(oss, t_arr, 3);
  oss << ",\"r\":";
  appendJsonFloatArray(oss, r_arr, 4);
  oss << ",\"s\":";
  appendJsonFloatArray(oss, s_arr, 3);
  oss << "},\"active\":" << (entity->isActive() ? "true" : "false") << "}";
  return oss.str();
}

bool applyPlayAuthorshipPatchJson(SceneInstance& scene, const std::string& json,
                                  std::string* unknown_address) {
  if (unknown_address != nullptr) {
    unknown_address->clear();
  }
  std::string address;
  if (!extractJsonStringField(json, "address", address) || address.empty()) {
    return true;
  }
  const EntityId id = scene.findEntityByName(eastl::string(address.c_str()));
  Entity* entity = scene.getEntity(id);
  if (!isValid(id) || entity == nullptr) {
    if (unknown_address != nullptr) {
      *unknown_address = address;
    }
    return false;
  }

  float t[3] = {entity->getPosition().x, entity->getPosition().y,
                entity->getPosition().z};
  float r[4] = {entity->getRotation().x, entity->getRotation().y,
                entity->getRotation().z, entity->getRotation().w};
  float s[3] = {entity->getScale().x, entity->getScale().y,
                entity->getScale().z};
  const bool has_t = extractJsonFloatArray(json, "t", t, 3);
  const bool has_r = extractJsonFloatArray(json, "r", r, 4);
  const bool has_s = extractJsonFloatArray(json, "s", s, 3);
  if (has_t || has_r || has_s) {
    entity->setPosition(Vec3(t[0], t[1], t[2]));
    entity->setRotation(Quat(r[3], r[0], r[1], r[2]));
    entity->setScale(Vec3(s[0], s[1], s[2]));
    scene.markTransformsDirty();
  }
  bool active = entity->isActive();
  if (extractJsonBoolField(json, "active", active)) {
    scene.setObjectActive(id, active);
  }
  return true;
}

bool applyPlayAuthorshipPatchOnActiveScene(SceneSystem* scenes,
                                           const std::string& json,
                                           std::string* unknown_address) {
  if (scenes == nullptr) {
    return true;
  }
  SceneInstance* instance = scenes->getActiveInstance();
  if (instance == nullptr) {
    return true;
  }
  return applyPlayAuthorshipPatchJson(*instance, json, unknown_address);
}

void maybeSendPlayAuthorshipPatch(const IEditorCommand& command) {
  if (!command.isPlayV1Patchable()) {
    return;
  }
  PlaySessionController* session =
      g_runtime_global_context.m_play_session.get();
  EditorSceneEditSystem* edit =
      g_runtime_global_context.m_editor_scene_edit.get();
  SceneSystem* scenes = g_runtime_global_context.m_scene_system.get();
  if (session == nullptr || edit == nullptr || scenes == nullptr) {
    return;
  }
  if (!session->reloadEnabled()) {
    return;
  }
  const eastl::string live_guid = liveDocumentGuid();
  if (!isPlayEntryLiveDocument(session->playEntryScene(),
                               session->playEntryGuid(),
                               edit->activeScenePath().c_str(),
                               live_guid.c_str())) {
    return;
  }
  SceneInstance* scene = scenes->getActiveInstance();
  if (scene == nullptr) {
    return;
  }
  auto send_one = [&](EntityId id) {
    const std::string json = buildPlayAuthorshipPatchJson(*scene, id);
    if (!json.empty()) {
      (void)session->sendPatch(json);
    }
  };
  if (!command.play_v1_entity_ids.empty()) {
    for (EntityId id : command.play_v1_entity_ids) {
      send_one(id);
    }
    return;
  }
  send_one(command.play_v1_entity_id);
}

}  // namespace Blunder
