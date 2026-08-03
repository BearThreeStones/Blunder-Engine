#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/object/animation_sync_group.h"

namespace Blunder {

class Object;

/// Edit Mode Sync Group + CINE preview — no DotNetHost / Behaviour Tick.
/// Does not auto-snap Object TRS or run gameplay state machines (Design Decision 7).
class AnimationSyncCinePreviewController final {
 public:
  bool hasMembers() const { return !m_objects.empty(); }
  size_t memberCount() const { return m_objects.size(); }

  bool isPlaying() const { return m_playing; }
  bool isInCine() const;
  bool isGameplayInputSuppressed() const;

  void bindObjects(eastl::vector<Object*> objects);
  void clearMembers();

  bool fire(const eastl::vector<SyncGroupFireInstruction>& instructions);
  bool fireSameName(const eastl::string& clip_name);

  bool enterCine(bool suppress_gameplay_input = false);
  bool endCine();

  void stop();

  /// Advance bound AnimationPlayers via tickObjectAnimationPreviewFrame when playing.
  void tick(float delta_time);

 private:
  void rebuildSyncGroup();

  eastl::vector<Object*> m_objects;
  SyncGroupId m_sync_group_id{k_invalid_sync_group_id};
  bool m_playing{false};
};

}  // namespace Blunder
