#pragma once

#include <string>

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class IEditorCommand;
class SceneInstance;
class SceneSystem;

/// Current v1 fields for the entity (address + local TRS). Extra keys optional.
std::string buildPlayAuthorshipPatchJson(const SceneInstance& scene,
                                         EntityId entity_id);

/// Apply a v1 snapshot by Authorship Address (entity name). Unknown name:
/// returns false and writes the address. Missing address: no-op, returns true.
bool applyPlayAuthorshipPatchJson(SceneInstance& scene, const std::string& json,
                                  std::string* unknown_address);

bool applyPlayAuthorshipPatchOnActiveScene(SceneSystem* scenes,
                                           const std::string& json,
                                           std::string* unknown_address);

/// After Document History mutation: send patch if Playing/Paused on Play entry.
void maybeSendPlayAuthorshipPatch(const IEditorCommand& command);

}  // namespace Blunder
