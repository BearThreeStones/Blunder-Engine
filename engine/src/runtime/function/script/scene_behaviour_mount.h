#pragma once

namespace Blunder {

class DotNetHost;
class Scene;
class SceneInstance;

/// When `host` is running with a game assembly loaded, AttachBehaviour for each
/// null-peer Behaviour slot on bound Objects (list order). When `scene` is
/// non-null, apply each declaration's property bag after attach (bool / number /
/// string fields that exist on the managed type; unknown keys skipped).
void mountSceneBehaviours(SceneInstance& instance, DotNetHost& host,
                          const Scene* scene);

}  // namespace Blunder
