#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/skeleton_modifier.h"

namespace Blunder {

/// Spring-chain SkeletonModifier (CloudRig / logo jiggle).
/// Tails integrate in world space so a moving host lags; bone aim uses the
/// child offset as the axis, and a leaf uses local +Y times end length.
class SkeletonSpringBoneModifier : public SkeletonModifier {
 public:
  void setRootBoneName(const eastl::string& name);
  const eastl::string& getRootBoneName() const { return m_root_bone_name; }

  void setEndBoneName(const eastl::string& name);
  const eastl::string& getEndBoneName() const { return m_end_bone_name; }

  void setStiffness(float stiffness);
  float getStiffness() const { return m_stiffness; }

  void setDrag(float drag);
  float getDrag() const { return m_drag; }

  /// World-space acceleration (Z-up). Zero leaves the chain on the animated pose
  /// until inertia or a later gravity write moves it.
  void setGravity(const Vec3& gravity) { m_gravity = gravity; }
  const Vec3& getGravity() const { return m_gravity; }

  void setEndBoneLength(float length);
  float getEndBoneLength() const { return m_end_bone_length; }

  /// Host Object world matrix. Identity keeps simulation in model space.
  void setHostWorldMatrix(const Mat4& matrix) { m_host_world = matrix; }
  const Mat4& getHostWorldMatrix() const { return m_host_world; }

  /// Seconds for this apply. Zero reapplies the last tails without integrating.
  void setDeltaTime(float delta_seconds) { m_delta_time = delta_seconds; }
  float getDeltaTime() const { return m_delta_time; }

  const char* getTypeName() const override { return "SpringBone"; }

  void apply(Skeleton& skeleton) override;

 private:
  void resetSimulation();

  eastl::string m_root_bone_name{"Root"};
  eastl::string m_end_bone_name;
  float m_stiffness{0.2f};
  float m_drag{0.2f};
  Vec3 m_gravity{0.0f};
  float m_end_bone_length{0.1f};
  Mat4 m_host_world{1.0f};
  float m_delta_time{0.0f};
  float m_last_dt{0.0f};
  bool m_initialized{false};
  eastl::vector<int> m_chain;
  eastl::vector<Vec3> m_tail;
  eastl::vector<Vec3> m_prev_tail;
};

}  // namespace Blunder
