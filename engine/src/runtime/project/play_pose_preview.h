#pragma once

#include "EASTL/string.h"
#include "EASTL/unordered_map.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/project/play_ipc.h"

namespace Blunder {

class SceneInstance;
class SceneSystem;

struct PlayPoseLocalTrs {
  float t[3]{0.f, 0.f, 0.f};
  float r[4]{0.f, 0.f, 0.f, 1.f};
  float s[3]{1.f, 1.f, 1.f};
};

using PlayPoseOverlayMap = eastl::unordered_map<eastl::string, PlayPoseLocalTrs>;

/// Named Play-entry entities only (omit unnamed runtime spawns).
void collectPlayPoses(const SceneInstance& scene, PlayIpcPosesRecord& out);
void collectPlayPosesFromActiveScene(const SceneSystem* scenes,
                                     PlayIpcPosesRecord& out);

/// Viewport gather: overlay local TRS by entity name. Null/empty overlay uses
/// the instance world cache (Live). Does not write SceneInstance.
Mat4 worldMatrixWithPlayPoseOverlay(const SceneInstance& scene, EntityId id,
                                    const PlayPoseOverlayMap* overlay);

}  // namespace Blunder
