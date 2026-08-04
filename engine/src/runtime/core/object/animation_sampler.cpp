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

BoneTransform blendBoneTransforms(const BoneTransform& a, const BoneTransform& b,
                                  float weight) {
  BoneTransform result;
  result.translation = lerp(a.translation, b.translation, weight);
  result.rotation = slerp(a.rotation, b.rotation, weight);
  result.scale = lerp(a.scale, b.scale, weight);
  return result;
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

void blendClipsOntoSkeleton(Skeleton& skeleton, const AnimationClipData& clip0,
                            float time0, const AnimationClipData& clip1,
                            float time1, float blend_weight) {
  const size_t bone_count = skeleton.getBoneCount();
  eastl::vector<BoneTransform> pose0(bone_count);
  eastl::vector<BoneTransform> pose1(bone_count);

  sampleClipOntoSkeleton(skeleton, clip0, time0);
  for (size_t i = 0; i < bone_count; ++i) {
    pose0[i] = skeleton.getBonePoseLocal(i);
  }

  sampleClipOntoSkeleton(skeleton, clip1, time1);
  for (size_t i = 0; i < bone_count; ++i) {
    pose1[i] = skeleton.getBonePoseLocal(i);
  }

  skeleton.resetPoseToRest();
  for (size_t i = 0; i < bone_count; ++i) {
    skeleton.setBonePoseLocal(
        i, blendBoneTransforms(pose0[i], pose1[i], blend_weight));
  }
}

namespace {

Quat nlerpThree(const Quat& a, float wa, const Quat& b, float wb, const Quat& c,
                float wc) {
  Quat qb = b;
  Quat qc = c;
  if (glm::dot(a, qb) < 0.0f) {
    qb = -qb;
  }
  if (glm::dot(a, qc) < 0.0f) {
    qc = -qc;
  }
  Quat mixed = a * wa + qb * wb + qc * wc;
  const float len = glm::length(mixed);
  if (len <= 1.0e-8f) {
    return a;
  }
  return mixed / len;
}

BoneTransform blendBoneTransformsThree(const BoneTransform& a, float wa,
                                       const BoneTransform& b, float wb,
                                       const BoneTransform& c, float wc) {
  BoneTransform result;
  result.translation = a.translation * wa + b.translation * wb + c.translation * wc;
  result.scale = a.scale * wa + b.scale * wb + c.scale * wc;
  result.rotation = nlerpThree(a.rotation, wa, b.rotation, wb, c.rotation, wc);
  return result;
}

}  // namespace

void blendThreeClipsOntoSkeleton(Skeleton& skeleton,
                                 const AnimationClipData& clip0, float time0,
                                 float weight0, const AnimationClipData& clip1,
                                 float time1, float weight1,
                                 const AnimationClipData& clip2, float time2,
                                 float weight2) {
  const float sum = weight0 + weight1 + weight2;
  float w0 = weight0;
  float w1 = weight1;
  float w2 = weight2;
  if (sum > 1.0e-8f) {
    w0 /= sum;
    w1 /= sum;
    w2 /= sum;
  } else {
    w0 = 1.0f;
    w1 = 0.0f;
    w2 = 0.0f;
  }

  const size_t bone_count = skeleton.getBoneCount();
  eastl::vector<BoneTransform> pose0(bone_count);
  eastl::vector<BoneTransform> pose1(bone_count);
  eastl::vector<BoneTransform> pose2(bone_count);

  sampleClipOntoSkeleton(skeleton, clip0, time0);
  for (size_t i = 0; i < bone_count; ++i) {
    pose0[i] = skeleton.getBonePoseLocal(i);
  }
  sampleClipOntoSkeleton(skeleton, clip1, time1);
  for (size_t i = 0; i < bone_count; ++i) {
    pose1[i] = skeleton.getBonePoseLocal(i);
  }
  sampleClipOntoSkeleton(skeleton, clip2, time2);
  for (size_t i = 0; i < bone_count; ++i) {
    pose2[i] = skeleton.getBonePoseLocal(i);
  }

  skeleton.resetPoseToRest();
  for (size_t i = 0; i < bone_count; ++i) {
    skeleton.setBonePoseLocal(
        i, blendBoneTransformsThree(pose0[i], w0, pose1[i], w1, pose2[i], w2));
  }
}

void applyAdditiveClipOntoSkeleton(Skeleton& skeleton,
                                   const AnimationClipData& clip, float time,
                                   float weight) {
  if (weight <= 0.0f) {
    return;
  }

  eastl::vector<float> scratch;
  for (const AnimationTrack& track : clip.tracks) {
    const int bone_index = skeleton.findBoneIndex(track.bone);
    if (bone_index < 0) {
      continue;
    }

    const AnimationKeyframe* key = sampleKeyframeValue(track, time, scratch);
    const eastl::vector<float>* sampled_value =
        key != nullptr ? &key->value : nullptr;
    if (sampled_value == nullptr && scratch.empty()) {
      continue;
    }
    const eastl::vector<float>& value =
        sampled_value != nullptr ? *sampled_value : scratch;

    const BoneTransform rest =
        skeleton.getBoneRestLocal(static_cast<size_t>(bone_index));
    BoneTransform pose =
        skeleton.getBonePoseLocal(static_cast<size_t>(bone_index));

    switch (track.channel) {
      case AnimationChannel::Translation:
        pose.translation +=
            weight * (vec3FromValue(value) - rest.translation);
        break;
      case AnimationChannel::Rotation: {
        const Quat delta =
            quatFromValue(value) * glm::inverse(rest.rotation);
        pose.rotation =
            pose.rotation * slerp(glm::identity<Quat>(), delta, weight);
        break;
      }
      case AnimationChannel::Scale:
        pose.scale += weight * (vec3FromValue(value) - rest.scale);
        break;
    }

    skeleton.setBonePoseLocal(static_cast<size_t>(bone_index), pose);
  }
}

}  // namespace Blunder
