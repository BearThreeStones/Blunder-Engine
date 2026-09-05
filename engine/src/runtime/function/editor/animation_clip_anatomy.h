#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

/// Clip-track channel as the Animation Window labels it (Local Transform words).
/// AnimationChannel::Translation is the product's Position row.
enum class AnimationAnatomyChannel : uint8_t {
  Position = 0,
  Rotation,
  Scale,
};

/// One clip track: a bone's existing TRS channel plus that channel's key times.
struct AnimationAnatomyChannelRow {
  AnimationAnatomyChannel channel{AnimationAnatomyChannel::Position};
  eastl::vector<float> key_times;
};

/// One bone group; channels are Position, then Rotation, then Scale.
struct AnimationAnatomyGroup {
  eastl::string bone;
  eastl::vector<AnimationAnatomyChannelRow> channels;
};

/// Read-only Clip anatomy of one AnimationClip.
struct AnimationClipAnatomy {
  eastl::string clip_name;
  float duration{0.0f};
  eastl::vector<AnimationAnatomyGroup> groups;
};

/// Flattened list row: either a bone group title or one channel row under it.
struct AnimationAnatomyRow {
  bool group{false};
  eastl::string bone;
  eastl::string label;
  AnimationAnatomyChannel channel{AnimationAnatomyChannel::Position};
  bool collapsed{false};
  eastl::vector<float> key_times;
};

/// Group the clip's existing tracks by bone first appearance, ordering each
/// group Position -> Rotation -> Scale and omitting channels the clip lacks.
/// Method keys never produce rows; the bound Skeleton is never consulted.
AnimationClipAnatomy buildClipAnatomy(const AnimationClipData& clip);

const char* animationAnatomyChannelLabel(AnimationAnatomyChannel channel);

/// Editor Session chrome for Clip anatomy: bone-name filter and bone-group
/// fold. Never reaches Document History, the Scene, or the AnimationClip.
class AnimationAnatomySession final {
 public:
  const eastl::string& filter() const { return m_filter; }
  void setFilter(const eastl::string& query) { m_filter = query; }

  bool isCollapsed(const eastl::string& bone) const;
  void setCollapsed(const eastl::string& bone, bool collapsed);
  void toggleCollapsed(const eastl::string& bone);
  void expandAll() { m_collapsed_bones.clear(); }

  /// Re-expands every bone group when the ruler clip name changes.
  /// Returns true when the stored name changed (fold clear is a side effect).
  bool syncRulerClipName(const eastl::string& clip_name);

  /// Flatten \a anatomy into visible rows, applying the filter and the fold.
  eastl::vector<AnimationAnatomyRow> buildRows(
      const AnimationClipAnatomy& anatomy) const;

 private:
  bool matchesFilter(const eastl::string& bone) const;

  eastl::string m_filter;
  eastl::string m_ruler_clip_name;
  eastl::vector<eastl::string> m_collapsed_bones;
};

}  // namespace Blunder
