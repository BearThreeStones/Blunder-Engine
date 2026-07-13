#pragma once

#include "EASTL/string.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

/// Narrow façade so Object can materialize Transform without linking SceneInstance.
class IEntityStore {
 public:
  virtual ~IEntityStore() = default;
  virtual EntityId createEntity(eastl::string name, const Vec3& position,
                                const Quat& rotation, const Vec3& scale,
                                EntityId parent_id) = 0;
  virtual bool getTransform(EntityId id, Vec3& out_position, Quat& out_rotation,
                            Vec3& out_scale) const = 0;
  virtual bool setTransform(EntityId id, const Vec3& position,
                            const Quat& rotation, const Vec3& scale) = 0;
  virtual void markTransformsDirty() = 0;
};

}  // namespace Blunder
