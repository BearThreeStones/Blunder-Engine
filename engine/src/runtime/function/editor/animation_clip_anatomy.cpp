#include "runtime/function/editor/animation_clip_anatomy.h"

#include "EASTL/sort.h"

#include <cctype>

namespace Blunder {

namespace {

AnimationAnatomyChannel toAnatomyChannel(const AnimationChannel channel) {
  switch (channel) {
    case AnimationChannel::Rotation:
      return AnimationAnatomyChannel::Rotation;
    case AnimationChannel::Scale:
      return AnimationAnatomyChannel::Scale;
    case AnimationChannel::Translation:
    default:
      return AnimationAnatomyChannel::Position;
  }
}

eastl::string toLower(const eastl::string& text) {
  eastl::string lowered = text;
  for (char& character : lowered) {
    character =
        static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  return lowered;
}

AnimationAnatomyGroup& groupForBone(AnimationClipAnatomy& anatomy,
                                    const eastl::string& bone) {
  for (AnimationAnatomyGroup& candidate : anatomy.groups) {
    if (candidate.bone == bone) {
      return candidate;
    }
  }
  anatomy.groups.push_back(AnimationAnatomyGroup{});
  anatomy.groups.back().bone = bone;
  return anatomy.groups.back();
}

AnimationAnatomyChannelRow& channelRowFor(AnimationAnatomyGroup& group,
                                          const AnimationAnatomyChannel channel) {
  size_t insert_at = group.channels.size();
  for (size_t i = 0; i < group.channels.size(); ++i) {
    if (group.channels[i].channel == channel) {
      return group.channels[i];
    }
    if (group.channels[i].channel > channel) {
      insert_at = i;
      break;
    }
  }
  group.channels.insert(group.channels.begin() + insert_at,
                        AnimationAnatomyChannelRow{});
  group.channels[insert_at].channel = channel;
  return group.channels[insert_at];
}

}  // namespace

const char* animationAnatomyChannelLabel(const AnimationAnatomyChannel channel) {
  switch (channel) {
    case AnimationAnatomyChannel::Rotation:
      return "Rotation";
    case AnimationAnatomyChannel::Scale:
      return "Scale";
    case AnimationAnatomyChannel::Position:
    default:
      return "Position";
  }
}

AnimationClipAnatomy buildClipAnatomy(const AnimationClipData& clip) {
  AnimationClipAnatomy anatomy;
  anatomy.clip_name = clip.name;
  anatomy.duration = clip.duration;

  for (const AnimationTrack& track : clip.tracks) {
    if (track.bone.empty()) {
      continue;
    }
    AnimationAnatomyGroup& group = groupForBone(anatomy, track.bone);
    AnimationAnatomyChannelRow& row =
        channelRowFor(group, toAnatomyChannel(track.channel));
    for (const AnimationKeyframe& key : track.keys) {
      row.key_times.push_back(key.time);
    }
    eastl::sort(row.key_times.begin(), row.key_times.end());
  }

  return anatomy;
}

bool AnimationAnatomySession::isCollapsed(const eastl::string& bone) const {
  for (const eastl::string& collapsed : m_collapsed_bones) {
    if (collapsed == bone) {
      return true;
    }
  }
  return false;
}

void AnimationAnatomySession::setCollapsed(const eastl::string& bone,
                                           const bool collapsed) {
  for (size_t i = 0; i < m_collapsed_bones.size(); ++i) {
    if (m_collapsed_bones[i] != bone) {
      continue;
    }
    if (!collapsed) {
      m_collapsed_bones.erase(m_collapsed_bones.begin() + i);
    }
    return;
  }
  if (collapsed && !bone.empty()) {
    m_collapsed_bones.push_back(bone);
  }
}

void AnimationAnatomySession::toggleCollapsed(const eastl::string& bone) {
  setCollapsed(bone, !isCollapsed(bone));
}

bool AnimationAnatomySession::syncRulerClipName(const eastl::string& clip_name) {
  if (clip_name == m_ruler_clip_name) {
    return false;
  }
  m_ruler_clip_name = clip_name;
  expandAll();
  return true;
}

bool AnimationAnatomySession::matchesFilter(const eastl::string& bone) const {
  if (m_filter.empty()) {
    return true;
  }
  return toLower(bone).find(toLower(m_filter)) != eastl::string::npos;
}

eastl::vector<AnimationAnatomyRow> AnimationAnatomySession::buildRows(
    const AnimationClipAnatomy& anatomy) const {
  eastl::vector<AnimationAnatomyRow> rows;
  for (const AnimationAnatomyGroup& group : anatomy.groups) {
    if (!matchesFilter(group.bone)) {
      continue;
    }

    const bool collapsed = isCollapsed(group.bone);
    rows.push_back(AnimationAnatomyRow{});
    AnimationAnatomyRow& title = rows.back();
    title.group = true;
    title.bone = group.bone;
    title.label = group.bone;
    title.collapsed = collapsed;
    if (collapsed) {
      continue;
    }

    for (const AnimationAnatomyChannelRow& channel : group.channels) {
      rows.push_back(AnimationAnatomyRow{});
      AnimationAnatomyRow& row = rows.back();
      row.bone = group.bone;
      row.label = animationAnatomyChannelLabel(channel.channel);
      row.channel = channel.channel;
      row.key_times = channel.key_times;
    }
  }
  return rows;
}

}  // namespace Blunder
