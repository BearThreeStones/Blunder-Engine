#include "runtime/core/object/animation_sampler.h"

#include "runtime/core/math/interpolation.h"

namespace Blunder {

namespace {

Vec3 vec3FromValue(const eastl::vector<float>& value) {
  if (value.size() < 3) {
    return Vec3(0.0f);
  }
  return Vec3(value[0], value[1], value[2]);
}

Quat quatFromValue(const eastl::vector<float>& value) {
  if (value.size() < 4) {
    return glm::identity<Quat>();
  }
  // Clip values are xyzw (glTF / YAML).
  return Quat(value[3], value[0], value[1], value[2]);
}

Vec3 lerpVec3(const eastl::vector<float>& a, const eastl::vector<float>& b,
              float t) {
  return lerp(vec3FromValue(a), vec3FromValue(b), t);
}

Quat lerpQuat(const eastl::vector<float>& a, const eastl::vector<float>& b,
              float t) {
  return slerp(quatFromValue(a), quatFromValue(b), t);
}

const AnimationKeyframe* sampleKeyframeValue(
    const AnimationTrack& track, float time, eastl::vector<float>& scratch) {
  const eastl::vector<AnimationKeyframe>& keys = track.keys;
  if (keys.empty()) {
    return nullptr;
  }
  if (time <= keys.front().time) {
    return &keys.front();
  }
  if (time >= keys.back().time) {
    return &keys.back();
  }

  size_t right = 1;
  while (right < keys.size() && keys[right].time <= time) {
    ++right;
  }
  const size_t left = right - 1;

  if (track.interpolation == AnimationInterpolation::Constant) {
    return &keys[left];
  }

  const float t0 = keys[left].time;
  const float t1 = keys[right].time;
  const float span = t1 - t0;
  const float alpha = span > 0.0f ? (time - t0) / span : 0.0f;

  scratch.clear();
  if (track.channel == AnimationChannel::Rotation) {
    const Quat blended = lerpQuat(keys[left].value, keys[right].value, alpha);
    scratch.push_back(blended.x);
    scratch.push_back(blended.y);
    scratch.push_back(blended.z);
    scratch.push_back(blended.w);
  } else {
    const Vec3 blended = lerpVec3(keys[left].value, keys[right].value, alpha);
    scratch.push_back(blended.x);
    scratch.push_back(blended.y);
    scratch.push_back(blended.z);
  }
  return nullptr;
}

void applyTrackValue(Skeleton& skeleton, int bone_index,
                     AnimationChannel channel,
                     const eastl::vector<float>& value) {
  BoneTransform pose = skeleton.getBonePoseLocal(static_cast<size_t>(bone_index));
  switch (channel) {
    case AnimationChannel::Translation:
      pose.translation = vec3FromValue(value);
      break;
    case AnimationChannel::Rotation:
      pose.rotation = quatFromValue(value);
      break;
    case AnimationChannel::Scale:
      pose.scale = vec3FromValue(value);
      break;
  }
  skeleton.setBonePoseLocal(static_cast<size_t>(bone_index), pose);
}

void applyTrack(Skeleton& skeleton, const AnimationTrack& track, float time,
                eastl::vector<float>& scratch) {
  const int bone_index = skeleton.findBoneIndex(track.bone);
  if (bone_index < 0) {
    return;
  }

  const AnimationKeyframe* key = sampleKeyframeValue(track, time, scratch);
  if (key != nullptr) {
    applyTrackValue(skeleton, bone_index, track.channel, key->value);
    return;
  }
  if (!scratch.empty()) {
    applyTrackValue(skeleton, bone_index, track.channel, scratch);
  }
}

}  // namespace

void sampleClipOntoSkeleton(Skeleton& skeleton, const AnimationClipData& clip,
                            float time) {
  skeleton.resetPoseToRest();
  eastl::vector<float> scratch;
  for (const AnimationTrack& track : clip.tracks) {
    applyTrack(skeleton, track, time, scratch);
  }
}

}  // namespace Blunder
