#pragma once

#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/vector.h"

namespace Blunder {

class AnimationPlayer;

/// Opaque runtime Sync Group handle (script-owned lifetime).
using SyncGroupId = uint64_t;

constexpr SyncGroupId k_invalid_sync_group_id = 0u;

inline bool isValidSyncGroupId(SyncGroupId id) {
  return id != k_invalid_sync_group_id;
}

/// Per-member Fire instruction: player, clip logical name, optional seek.
struct SyncGroupFireInstruction {
  AnimationPlayer* player{nullptr};
  eastl::string clip_name;
  float seek_seconds{0.0f};
  bool has_seek{false};
};

/// Runtime registry of Sync Groups whose members are AnimationPlayers.
///
/// Phase 1 rule: Fire coordinates players only; each AnimationPlayer samples
/// onto its co-located Skeleton (via bindSamplingSkeleton). This API has no
/// Skeleton* parameters and cannot drive a remote Skeleton on another Object.
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

  /// Fire per-member clip instructions at the same logical moment (default hard cut).
  bool fire(SyncGroupId id,
            const eastl::vector<SyncGroupFireInstruction>& instructions);

  /// Fire the same clip logical name on every group member (each resolves via own map).
  bool fireSameName(SyncGroupId id, const eastl::string& clip_name);
  /// Same with seek applied to all members.
  bool fireSameName(SyncGroupId id, const eastl::string& clip_name,
                    float seek_seconds);

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
