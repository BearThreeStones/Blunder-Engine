#pragma once

#include "EASTL/string.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/entity_id.h"

struct cgltf_data;
struct cgltf_node;
struct cgltf_skin;

namespace Blunder {

class AssetManager;
class SceneInstance;
class Skeleton;

void decomposeCgltfNodeLocal(const cgltf_node* node, Vec3& out_position,
                             Quat& out_rotation, Vec3& out_scale);

eastl::string gltfNodeDisplayName(const cgltf_node* node);

void populateSkeletonFromSkin(cgltf_skin* skin, Skeleton& skeleton);

/// Fills an empty Skeleton from the entity mesh Intermediate glTF. Returns
/// true when named bones were written. Does not spawn glTF child entities.
bool hydrateSkeletonFromEntityMesh(AssetManager* asset_manager,
                                   SceneInstance& scene, EntityId entity_id,
                                   Skeleton& skeleton);

}  // namespace Blunder
