#pragma once

#include <cstdint>

namespace Blunder {

class AnimationPlayer;

/// Opaque runtime Sync Group handle (script-owned lifetime).
using SyncGroupId = uint64_t;

constexpr SyncGroupId k_invalid_sync_group_id = 0u;

inline bool isValid(SyncGroupId id) { return id != k_invalid_sync_group_id; }

/// Runtime registry of Sync Groups whose members are AnimationPlayers.
///
/// Member pointers are non-owning; callers must leave or destroy groups before
/// an AnimationPlayer is destroyed (no automatic lifetime tracking).
class AnimationSyncGroupService {
 public:
  AnimationSyncGroupService();
  ~AnimationSyncGroupService();

  AnimationSyncGroupService(const AnimationSyncGroupService&) = delete;
  AnimationSyncGroupService& operator=(const AnimationSyncGroupService&) = delete;

  SyncGroupId create();
  bool destroy(SyncGroupId id);

  bool join(SyncGroupId id, AnimationPlayer* player);
  bool leave(SyncGroupId id, AnimationPlayer* player);

  bool isMember(SyncGroupId id, const AnimationPlayer* player) const;
  size_t getMemberCount(SyncGroupId id) const;

  /// Test / debug helper: member pointer at stable insertion order.
  AnimationPlayer* getMemberAt(SyncGroupId id, size_t index) const;

  /// Clears every live group (unit tests). Does not reset the id counter;
  /// subsequent create() ids remain monotonically increasing.
  void clearAll();

 private:
  struct Group;
  struct Storage;

  Storage* m_storage{nullptr};

  const Group* findGroup(SyncGroupId id) const;
  Group* findGroup(SyncGroupId id);
};

AnimationSyncGroupService& animationSyncGroupService();

}  // namespace Blunder
