#include "runtime/function/scene/skeleton_from_gltf.h"

#include <cgltf.h>

#include <cstring>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/core/math/coordinate_system.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/resource/asset/guid.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_manager/asset_manager_gltf.h"
#include "runtime/resource/asset_registry/asset_registry.h"

namespace Blunder {

void decomposeCgltfNodeLocal(const cgltf_node* node, Vec3& out_position,
                             Quat& out_rotation, Vec3& out_scale) {
  cgltf_float local_matrix_cgltf[16];
  cgltf_node_transform_local(const_cast<cgltf_node*>(node), local_matrix_cgltf);
  glm::mat4 local_matrix(1.0f);
  std::memcpy(glm::value_ptr(local_matrix), local_matrix_cgltf,
              sizeof(cgltf_float) * 16);
  local_matrix = similarityGltfToEngine(local_matrix);

  out_position = Vec3(local_matrix[3]);
  const Vec3 basis_x = Vec3(local_matrix[0]);
  const Vec3 basis_y = Vec3(local_matrix[1]);
  const Vec3 basis_z = Vec3(local_matrix[2]);
  out_scale = Vec3(glm::length(basis_x), glm::length(basis_y), glm::length(basis_z));

  constexpr float k_epsilon = 1e-6f;
  const Vec3 inv_scale(
      out_scale.x > k_epsilon ? 1.0f / out_scale.x : 1.0f,
      out_scale.y > k_epsilon ? 1.0f / out_scale.y : 1.0f,
      out_scale.z > k_epsilon ? 1.0f / out_scale.z : 1.0f);
  const Mat3 rotation_matrix(glm::normalize(basis_x * inv_scale.x),
                             glm::normalize(basis_y * inv_scale.y),
                             glm::normalize(basis_z * inv_scale.z));
  out_rotation = glm::normalize(glm::quat_cast(rotation_matrix));
}

eastl::string gltfNodeDisplayName(const cgltf_node* node) {
  if (node != nullptr && node->name != nullptr && node->name[0] != '\0') {
    return eastl::string(node->name);
  }
  return eastl::string("node");
}

void populateSkeletonFromSkin(cgltf_skin* skin, Skeleton& skeleton) {
  if (skin == nullptr || skin->joints_count == 0) {
    return;
  }

  eastl::vector<int> joint_to_bone_index(static_cast<size_t>(skin->joints_count), -1);
  for (cgltf_size joint_index = 0; joint_index < skin->joints_count; ++joint_index) {
    cgltf_node* joint_node = skin->joints[joint_index];
    if (joint_node == nullptr) {
      continue;
    }

    // Always add as a root first. glTF skin.joints is not parent-before-child,
    // so resolving parent_index in this pass would drop most of the hierarchy.
    const int bone_index =
        skeleton.addBone(gltfNodeDisplayName(joint_node), -1);
    if (bone_index < 0) {
      continue;
    }
    joint_to_bone_index[static_cast<size_t>(joint_index)] = bone_index;

    Vec3 local_position{};
    Quat local_rotation = glm::identity<Quat>();
    Vec3 local_scale(1.0f);
    decomposeCgltfNodeLocal(joint_node, local_position, local_rotation, local_scale);
    BoneTransform rest_local;
    rest_local.translation = local_position;
    rest_local.rotation = local_rotation;
    rest_local.scale = local_scale;
    skeleton.setBoneRestLocal(static_cast<size_t>(bone_index), rest_local);
  }

  for (cgltf_size joint_index = 0; joint_index < skin->joints_count; ++joint_index) {
    cgltf_node* joint_node = skin->joints[joint_index];
    const int bone_index = joint_to_bone_index[static_cast<size_t>(joint_index)];
    if (joint_node == nullptr || joint_node->parent == nullptr || bone_index < 0) {
      continue;
    }
    for (cgltf_size parent_joint_index = 0;
         parent_joint_index < skin->joints_count; ++parent_joint_index) {
      if (skin->joints[parent_joint_index] != joint_node->parent) {
        continue;
      }
      const int parent_bone =
          joint_to_bone_index[static_cast<size_t>(parent_joint_index)];
      if (parent_bone >= 0) {
        skeleton.setParentIndex(static_cast<size_t>(bone_index), parent_bone);
      }
      break;
    }
  }

  skeleton.rebuildInverseBindsFromRest();
  skeleton.resetPoseToRest();
  skeleton.rebuildPoseBuffers();
}

namespace {

cgltf_skin* findHydrationSkin(cgltf_data* data) {
  if (data == nullptr) {
    return nullptr;
  }
  for (cgltf_size node_index = 0; node_index < data->nodes_count; ++node_index) {
    cgltf_node* node = &data->nodes[node_index];
    if (node->mesh != nullptr && node->skin != nullptr) {
      return node->skin;
    }
  }
  if (data->skins_count > 0) {
    return &data->skins[0];
  }
  return nullptr;
}

eastl::string meshReferenceForEntity(AssetManager* asset_manager,
                                     SceneInstance& scene, EntityId entity_id) {
  eastl::string mesh_ref;
  if (Entity* entity = scene.getEntity(entity_id)) {
    mesh_ref = entity->getMeshVirtualPath();
  }
  if (isValidGuidFormat(mesh_ref) && asset_manager != nullptr) {
    if (AssetRegistry* registry =
            g_runtime_global_context.m_asset_registry.get()) {
      const eastl::string path =
          asset_manager->resolveGuidPath(mesh_ref, *registry);
      if (!path.empty()) {
        mesh_ref = path;
      }
    }
  }
  if (mesh_ref.empty() || isValidGuidFormat(mesh_ref)) {
    if (const MeshRendererComponent* renderer = scene.getMeshRenderer(entity_id);
        renderer != nullptr && renderer->mesh) {
      const eastl::string renderer_path = renderer->mesh->getVirtualPath();
      if (!renderer_path.empty()) {
        return renderer_path;
      }
    }
  }
  return mesh_ref;
}

}  // namespace

bool hydrateSkeletonFromEntityMesh(AssetManager* asset_manager,
                                   SceneInstance& scene, EntityId entity_id,
                                   Skeleton& skeleton) {
  if (skeleton.getBoneCount() > 0) {
    return true;
  }
  if (asset_manager == nullptr) {
    LOG_WARN("[hydrateSkeletonFromEntityMesh] no AssetManager; leaving empty Skeleton");
    return false;
  }

  const eastl::string mesh_ref =
      meshReferenceForEntity(asset_manager, scene, entity_id);
  if (mesh_ref.empty()) {
    LOG_WARN("[hydrateSkeletonFromEntityMesh] no mesh on entity; leaving empty Skeleton");
    return false;
  }

  eastl::string gltf_path;
  if (!asset_manager->resolveGltfSourcePath(mesh_ref, gltf_path)) {
    LOG_WARN("[hydrateSkeletonFromEntityMesh] cannot resolve glTF for '{}'; "
             "leaving empty Skeleton",
             mesh_ref.c_str());
    return false;
  }

  GltfImportDocument document{};
  if (!asset_manager->openGltfImportDocument(gltf_path, document)) {
    LOG_WARN("[hydrateSkeletonFromEntityMesh] failed to open '{}'; leaving empty Skeleton",
             gltf_path.c_str());
    return false;
  }

  cgltf_skin* skin = findHydrationSkin(document.data);
  if (skin == nullptr) {
    asset_manager->closeGltfImportDocument(document);
    LOG_WARN("[hydrateSkeletonFromEntityMesh] no skin in '{}'; leaving empty Skeleton",
             gltf_path.c_str());
    return false;
  }

  populateSkeletonFromSkin(skin, skeleton);
  asset_manager->closeGltfImportDocument(document);
  if (skeleton.getBoneCount() == 0) {
    LOG_WARN("[hydrateSkeletonFromEntityMesh] skin in '{}' produced no bones",
             gltf_path.c_str());
    return false;
  }
  return true;
}

bool hydrateEmptySkeletonsFromEntityMeshes(AssetManager* asset_manager,
                                           SceneInstance& scene) {
  bool ok = true;
  scene.forEachEntity([&](EntityId entity_id, const Entity&) {
    Object* object = scene.findBoundObject(entity_id);
    if (object == nullptr || !object->hasSkeleton()) {
      return;
    }
    if (!object->hasAnimationPlayer() && !object->hasAnimationTree()) {
      return;
    }
    Skeleton* skeleton = object->getSkeleton();
    if (skeleton == nullptr) {
      return;
    }
    if (!hydrateSkeletonFromEntityMesh(asset_manager, scene, entity_id,
                                       *skeleton)) {
      ok = false;
      return;
    }
    if (AnimationTree* tree = object->getAnimationTree();
        tree != nullptr && tree->isActive()) {
      tree->sampleBoundSkeleton();
    }
  });
  return ok;
}

}  // namespace Blunder
