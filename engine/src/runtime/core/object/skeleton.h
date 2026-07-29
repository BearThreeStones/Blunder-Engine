#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"

namespace Blunder {

struct BoneTransform {
  Vec3 translation{0.0f};
  Quat rotation{glm::identity<Quat>()};
  Vec3 scale{1.0f, 1.0f, 1.0f};
};

class Skeleton {
 public:
  size_t getBoneCount() const { return m_bones.size(); }

  /// Returns stable bone index, or -1 if `parent_index` is invalid.
  int addBone(eastl::string name, int parent_index = -1);
  int findBoneIndex(const eastl::string& name) const;
  const eastl::string& getBoneName(size_t index) const;
  int getParentIndex(size_t index) const;

  void setBoneRestLocal(size_t index, const BoneTransform& transform);
  BoneTransform getBoneRestLocal(size_t index) const;

  void setBonePoseLocal(size_t index, const BoneTransform& transform);
  BoneTransform getBonePoseLocal(size_t index) const;

  void setBoneInverseBind(size_t index, const Mat4& matrix);
  Mat4 getBoneInverseBind(size_t index) const;

  Mat4 getBoneGlobalRestMatrix(size_t index) const;
  Mat4 getBoneGlobalPoseMatrix(size_t index) const;

  void resetPoseToRest();

 private:
  struct Bone {
    eastl::string name;
    int parent_index{-1};
    BoneTransform rest_local;
    BoneTransform pose_local;
    Mat4 inverse_bind{1.0f};
  };

  Mat4 computeGlobalMatrix(size_t index, bool use_pose) const;
  static Mat4 boneTransformToMatrix(const BoneTransform& transform);

  eastl::vector<Bone> m_bones;
};

}  // namespace Blunder
