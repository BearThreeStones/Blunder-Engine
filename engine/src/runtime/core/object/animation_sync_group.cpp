#include "runtime/core/object/animation_sync_group.h"

#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"

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
  if (player == nullptr || !isValidSyncGroupId(id)) {
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
  if (player == nullptr || !isValidSyncGroupId(id)) {
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
  if (player == nullptr || !isValidSyncGroupId(id)) {
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

bool AnimationSyncGroupService::fire(
    SyncGroupId id, const eastl::vector<SyncGroupFireInstruction>& instructions) {
  if (!isValidSyncGroupId(id) || instructions.empty()) {
    return false;
  }

  const Group* group = findGroup(id);
  if (group == nullptr) {
    return false;
  }

  eastl::vector<AnimationClipData> resolved_clips;
  resolved_clips.resize(instructions.size());

  for (size_t i = 0; i < instructions.size(); ++i) {
    const SyncGroupFireInstruction& instruction = instructions[i];
    if (instruction.player == nullptr || instruction.clip_name.empty()) {
      return false;
    }

    bool member = false;
    for (const AnimationPlayer* candidate : group->members) {
      if (candidate == instruction.player) {
        member = true;
        break;
      }
    }
    if (!member) {
      return false;
    }

    if (!instruction.player->resolveClipForName(instruction.clip_name,
                                                resolved_clips[i])) {
      return false;
    }
  }

  // Co-located Skeleton only: each player snaps and samples its own bound skeleton,
  // or active AnimationTree members receive OneShot (tree stays active).
  for (size_t i = 0; i < instructions.size(); ++i) {
    const SyncGroupFireInstruction& instruction = instructions[i];
    AnimationTree* tree = instruction.player->getAnimationTree();
    if (tree != nullptr && tree->isActive()) {
      if (!tree->requestOneShot(instruction.clip_name)) {
        return false;
      }
      continue;
    }

    instruction.player->snapPlayWithClip(instruction.clip_name,
                                         resolved_clips[i]);
    if (instruction.has_seek) {
      instruction.player->seekPlayback(instruction.seek_seconds);
    }
  }

  return true;
}

bool AnimationSyncGroupService::fireSameName(SyncGroupId id,
                                             const eastl::string& clip_name) {
  if (!isValidSyncGroupId(id) || clip_name.empty()) {
    return false;
  }

  const Group* group = findGroup(id);
  if (group == nullptr || group->members.empty()) {
    return false;
  }

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.reserve(group->members.size());
  for (AnimationPlayer* player : group->members) {
    instructions.push_back(SyncGroupFireInstruction{player, clip_name});
  }

  return fire(id, instructions);
}

bool AnimationSyncGroupService::fireSameName(SyncGroupId id,
                                             const eastl::string& clip_name,
                                             float seek_seconds) {
  if (!isValidSyncGroupId(id) || clip_name.empty()) {
    return false;
  }

  const Group* group = findGroup(id);
  if (group == nullptr || group->members.empty()) {
    return false;
  }

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.reserve(group->members.size());
  for (AnimationPlayer* player : group->members) {
    SyncGroupFireInstruction instruction;
    instruction.player = player;
    instruction.clip_name = clip_name;
    instruction.seek_seconds = seek_seconds;
    instruction.has_seek = true;
    instructions.push_back(instruction);
  }

  return fire(id, instructions);
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
