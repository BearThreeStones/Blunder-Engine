#include "runtime/core/object/animation_sync_group.h"

#include "runtime/core/object/animation_player.h"

#include "EASTL/hash_map.h"
#include "EASTL/vector.h"

namespace Blunder {

struct AnimationSyncGroupService::Group {
  eastl::vector<AnimationPlayer*> members;
};

struct AnimationSyncGroupService::Storage {
  SyncGroupId next_id{1};
  eastl::hash_map<SyncGroupId, Group> groups;
};

AnimationSyncGroupService::AnimationSyncGroupService() : m_storage(new Storage) {}

AnimationSyncGroupService::~AnimationSyncGroupService() { delete m_storage; }

SyncGroupId AnimationSyncGroupService::create() {
  const SyncGroupId id = m_storage->next_id++;
  m_storage->groups[id] = Group{};
  return id;
}

bool AnimationSyncGroupService::destroy(SyncGroupId id) {
  return m_storage->groups.erase(id) > 0;
}

bool AnimationSyncGroupService::join(SyncGroupId id, AnimationPlayer* player) {
  if (player == nullptr || !isValid(id)) {
    return false;
  }

  const auto it = m_storage->groups.find(id);
  if (it == m_storage->groups.end()) {
    return false;
  }

  Group& group = it->second;
  for (const AnimationPlayer* member : group.members) {
    if (member == player) {
      return false;
    }
  }

  group.members.push_back(player);
  return true;
}

bool AnimationSyncGroupService::leave(SyncGroupId id, AnimationPlayer* player) {
  if (player == nullptr || !isValid(id)) {
    return false;
  }

  const auto it = m_storage->groups.find(id);
  if (it == m_storage->groups.end()) {
    return false;
  }

  Group& group = it->second;
  for (size_t i = 0; i < group.members.size(); ++i) {
    if (group.members[i] == player) {
      group.members.erase(group.members.begin() +
                          static_cast<ptrdiff_t>(i));
      return true;
    }
  }

  return false;
}

bool AnimationSyncGroupService::isMember(SyncGroupId id,
                                         const AnimationPlayer* player) const {
  if (player == nullptr || !isValid(id)) {
    return false;
  }

  const Group* group = findGroup(id);
  if (group == nullptr) {
    return false;
  }

  for (const AnimationPlayer* member : group->members) {
    if (member == player) {
      return true;
    }
  }

  return false;
}

size_t AnimationSyncGroupService::getMemberCount(SyncGroupId id) const {
  const Group* group = findGroup(id);
  if (group == nullptr) {
    return 0;
  }

  return group->members.size();
}

AnimationPlayer* AnimationSyncGroupService::getMemberAt(SyncGroupId id,
                                                        size_t index) const {
  const Group* group = findGroup(id);
  if (group == nullptr || index >= group->members.size()) {
    return nullptr;
  }

  return group->members[index];
}

void AnimationSyncGroupService::clearAll() { m_storage->groups.clear(); }

const AnimationSyncGroupService::Group*
AnimationSyncGroupService::findGroup(SyncGroupId id) const {
  const auto it = m_storage->groups.find(id);
  if (it == m_storage->groups.end()) {
    return nullptr;
  }

  return &it->second;
}

AnimationSyncGroupService::Group* AnimationSyncGroupService::findGroup(
    SyncGroupId id) {
  const auto it = m_storage->groups.find(id);
  if (it == m_storage->groups.end()) {
    return nullptr;
  }

  return &it->second;
}

AnimationSyncGroupService& animationSyncGroupService() {
  static AnimationSyncGroupService s_service;
  return s_service;
}

}  // namespace Blunder
