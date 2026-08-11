#include "runtime/function/editor/animation_tree_canvas_document.h"

#include <cstring>
#include <filesystem>

#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"

namespace Blunder {
namespace {

bool endsWithSuffix(const eastl::string& value, const char* suffix) {
  const size_t suffix_len = std::strlen(suffix);
  return value.size() >= suffix_len &&
         value.compare(value.size() - suffix_len, suffix_len, suffix) == 0;
}

std::filesystem::path resolveVirtualPath(FileSystem& file_system,
                                         const eastl::string& virtual_path) {
  if (virtual_path.compare(0, 7, "assets/") == 0) {
    return file_system.resolveAsset(
        std::filesystem::path(virtual_path.c_str() + 7));
  }
  if (virtual_path.compare(0, 10, "resources/") == 0) {
    return file_system.resolveResource(
        std::filesystem::path(virtual_path.c_str() + 10));
  }
  return file_system.resolve(std::filesystem::path(virtual_path.c_str()));
}

AnimationTreeTopologyData::BlendSpace1DDef* findBlend1D(
    AnimationTreeTopologyData& topology, const eastl::string& node_name) {
  for (AnimationTreeTopologyData::BlendSpace1DDef& space :
       topology.blend_spaces_1d) {
    if (space.node_name == node_name) {
      return &space;
    }
  }
  return nullptr;
}

AnimationTreeTopologyData::BlendSpace2DDef* findBlend2D(
    AnimationTreeTopologyData& topology, const eastl::string& node_name) {
  for (AnimationTreeTopologyData::BlendSpace2DDef& space :
       topology.blend_spaces_2d) {
    if (space.node_name == node_name) {
      return &space;
    }
  }
  return nullptr;
}

}  // namespace

bool isAnimationTreeAssetDescriptorPath(const eastl::string& virtual_path) {
  return endsWithSuffix(virtual_path, ".animationtree.yaml");
}

bool AnimationTreeCanvasDocument::openFromYaml(
    const eastl::string& guid, const eastl::string& descriptor_path,
    const eastl::string& topology_path, const eastl::string& topology_yaml) {
  AnimationTreeTopologyData parsed;
  if (!AssetYaml::parseAnimationTreeTopologyData(topology_yaml, parsed)) {
    return false;
  }
  m_guid = guid;
  m_descriptor_path = descriptor_path;
  m_topology_path = topology_path;
  m_topology = eastl::move(parsed);
  m_open = true;
  m_dirty = false;
  return true;
}

bool AnimationTreeCanvasDocument::openFromDescriptorPath(
    FileSystem& file_system, const eastl::string& descriptor_virtual_path) {
  if (!isAnimationTreeAssetDescriptorPath(descriptor_virtual_path)) {
    return false;
  }
  eastl::string descriptor_yaml;
  if (!file_system.readText(resolveVirtualPath(file_system, descriptor_virtual_path),
                            descriptor_yaml)) {
    return false;
  }
  AnimationTreeAssetDescriptor descriptor;
  if (!AssetYaml::parseAnimationTreeAssetDescriptor(descriptor_yaml,
                                                    descriptor) ||
      descriptor.guid.empty() || descriptor.source.empty()) {
    return false;
  }
  eastl::string topology_yaml;
  if (!file_system.readText(resolveVirtualPath(file_system, descriptor.source),
                            topology_yaml)) {
    return false;
  }
  return openFromYaml(descriptor.guid, descriptor_virtual_path, descriptor.source,
                      topology_yaml);
}

bool AnimationTreeCanvasDocument::openFromAssetGuid(
    FileSystem& file_system, const eastl::string& asset_guid,
    const eastl::string& descriptor_virtual_path) {
  if (asset_guid.empty()) {
    return false;
  }
  if (!openFromDescriptorPath(file_system, descriptor_virtual_path)) {
    return false;
  }
  if (m_guid != asset_guid) {
    close();
    return false;
  }
  return true;
}

void AnimationTreeCanvasDocument::close() {
  m_open = false;
  m_dirty = false;
  m_guid.clear();
  m_descriptor_path.clear();
  m_topology_path.clear();
  m_topology = AnimationTreeTopologyData{};
}

bool AnimationTreeCanvasDocument::addBlendSpace1D(
    const eastl::string& node_name) {
  if (!m_open || node_name.empty() || findBlend1D(m_topology, node_name)) {
    return false;
  }
  AnimationTreeTopologyData::BlendSpace1DDef space;
  space.node_name = node_name;
  m_topology.blend_spaces_1d.push_back(eastl::move(space));
  setNodePosition(node_name, 0.0f, 0.0f);
  markDirty();
  return true;
}

bool AnimationTreeCanvasDocument::addBlendSpace1DPoint(
    const eastl::string& node_name, const eastl::string& clip_name,
    float scalar) {
  AnimationTreeTopologyData::BlendSpace1DDef* space =
      m_open ? findBlend1D(m_topology, node_name) : nullptr;
  if (space == nullptr || clip_name.empty()) {
    return false;
  }
  AnimationTreeTopologyData::BlendSpace1DPointDef point;
  point.clip_name = clip_name;
  point.scalar = scalar;
  space->points.push_back(eastl::move(point));
  markDirty();
  return true;
}

bool AnimationTreeCanvasDocument::addBlendSpace2D(
    const eastl::string& node_name) {
  if (!m_open || node_name.empty() || findBlend2D(m_topology, node_name)) {
    return false;
  }
  AnimationTreeTopologyData::BlendSpace2DDef space;
  space.node_name = node_name;
  m_topology.blend_spaces_2d.push_back(eastl::move(space));
  setNodePosition(node_name, 0.0f, 0.0f);
  markDirty();
  return true;
}

bool AnimationTreeCanvasDocument::addBlendSpace2DPoint(
    const eastl::string& node_name, const eastl::string& clip_name, float x,
    float y) {
  AnimationTreeTopologyData::BlendSpace2DDef* space =
      m_open ? findBlend2D(m_topology, node_name) : nullptr;
  if (space == nullptr || clip_name.empty()) {
    return false;
  }
  AnimationTreeTopologyData::BlendSpace2DPointDef point;
  point.clip_name = clip_name;
  point.x = x;
  point.y = y;
  space->points.push_back(eastl::move(point));
  markDirty();
  return true;
}

bool AnimationTreeCanvasDocument::addState(const eastl::string& state_name,
                                          const eastl::string& kind,
                                          const eastl::string& clip_or_blend_node) {
  if (!m_open || state_name.empty() || kind.empty()) {
    return false;
  }
  for (const AnimationTreeTopologyData::StateDef& existing : m_topology.states) {
    if (existing.name == state_name) {
      return false;
    }
  }
  AnimationTreeTopologyData::StateDef state;
  state.name = state_name;
  state.kind = kind;
  if (kind == "clip") {
    state.clip_name = clip_or_blend_node;
  } else {
    state.blend_space_node = clip_or_blend_node;
  }
  m_topology.states.push_back(eastl::move(state));
  setNodePosition(state_name, 0.0f, 0.0f);
  markDirty();
  return true;
}

bool AnimationTreeCanvasDocument::setOneShotClip(const eastl::string& clip_name) {
  if (!m_open) {
    return false;
  }
  m_topology.oneshot_clip = clip_name;
  setNodePosition("OneShot", 0.0f, 0.0f);
  markDirty();
  return true;
}

bool AnimationTreeCanvasDocument::setAdd2Clip(const eastl::string& clip_name) {
  if (!m_open) {
    return false;
  }
  m_topology.add2_clip = clip_name;
  setNodePosition("Add2", 0.0f, 0.0f);
  markDirty();
  return true;
}

bool AnimationTreeCanvasDocument::setBaseBlendSpace1D(
    const eastl::string& node_name) {
  if (!m_open || findBlend1D(m_topology, node_name) == nullptr) {
    return false;
  }
  m_topology.base_blend_space_node = node_name;
  markDirty();
  return true;
}

bool AnimationTreeCanvasDocument::setBaseBlendSpace2D(
    const eastl::string& node_name) {
  if (!m_open || findBlend2D(m_topology, node_name) == nullptr) {
    return false;
  }
  m_topology.base_blend_space_2d_node = node_name;
  markDirty();
  return true;
}

bool AnimationTreeCanvasDocument::declareTreeParamBool(
    const eastl::string& name, bool default_value) {
  if (!m_open || name.empty()) {
    return false;
  }
  for (AnimationTreeTopologyData::TreeParamDef& param : m_topology.tree_params) {
    if (param.name == name) {
      param.kind = "bool";
      param.bool_default = default_value;
      markDirty();
      return true;
    }
  }
  AnimationTreeTopologyData::TreeParamDef param;
  param.name = name;
  param.kind = "bool";
  param.bool_default = default_value;
  m_topology.tree_params.push_back(eastl::move(param));
  markDirty();
  return true;
}

bool AnimationTreeCanvasDocument::declareTreeParamFloat(
    const eastl::string& name, float default_value) {
  if (!m_open || name.empty()) {
    return false;
  }
  for (AnimationTreeTopologyData::TreeParamDef& param : m_topology.tree_params) {
    if (param.name == name) {
      param.kind = "float";
      param.float_default = default_value;
      markDirty();
      return true;
    }
  }
  AnimationTreeTopologyData::TreeParamDef param;
  param.name = name;
  param.kind = "float";
  param.float_default = default_value;
  m_topology.tree_params.push_back(eastl::move(param));
  markDirty();
  return true;
}

bool AnimationTreeCanvasDocument::addTransition(
    const AnimationTreeTopologyData::TransitionDef& edge) {
  if (!m_open || edge.from_state.empty() || edge.to_state.empty()) {
    return false;
  }
  m_topology.transitions.push_back(edge);
  markDirty();
  return true;
}

bool AnimationTreeCanvasDocument::setNodePosition(const eastl::string& node_id,
                                                 float x, float y) {
  if (!m_open || node_id.empty()) {
    return false;
  }
  for (AnimationTreeTopologyData::CanvasLayoutNodeDef& layout :
       m_topology.canvas_layout) {
    if (layout.node_id == node_id) {
      layout.x = x;
      layout.y = y;
      markDirty();
      return true;
    }
  }
  AnimationTreeTopologyData::CanvasLayoutNodeDef layout;
  layout.node_id = node_id;
  layout.x = x;
  layout.y = y;
  m_topology.canvas_layout.push_back(eastl::move(layout));
  markDirty();
  return true;
}

bool AnimationTreeCanvasDocument::replaceTopologyFromInspectorYaml(
    const eastl::string& topology_yaml) {
  if (!m_open) {
    return false;
  }
  AnimationTreeTopologyData parsed;
  if (!AssetYaml::parseAnimationTreeTopologyData(topology_yaml, parsed)) {
    return false;
  }
  m_topology = eastl::move(parsed);
  markDirty();
  return true;
}

eastl::string AnimationTreeCanvasDocument::exportTopologyYaml() const {
  if (!m_open) {
    return {};
  }
  return AssetYaml::serializeAnimationTreeTopologyData(m_topology);
}

bool AnimationTreeCanvasDocument::save(FileSystem& file_system) {
  if (!m_open || m_topology_path.empty()) {
    return false;
  }
  const eastl::string yaml = exportTopologyYaml();
  if (!file_system.writeText(resolveVirtualPath(file_system, m_topology_path),
                             yaml)) {
    return false;
  }
  m_dirty = false;
  return true;
}

AnimationTreeCanvasSession& AnimationTreeCanvasSession::instance() {
  static AnimationTreeCanvasSession session;
  return session;
}

bool AnimationTreeCanvasSession::openDescriptor(
    FileSystem& file_system, const eastl::string& descriptor_virtual_path) {
  return m_document.openFromDescriptorPath(file_system, descriptor_virtual_path);
}

bool AnimationTreeCanvasSession::openGuid(
    FileSystem& file_system, const eastl::string& asset_guid,
    const eastl::string& descriptor_virtual_path) {
  return m_document.openFromAssetGuid(file_system, asset_guid,
                                      descriptor_virtual_path);
}

void AnimationTreeCanvasSession::close() { m_document.close(); }

}  // namespace Blunder
