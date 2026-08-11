#pragma once

#include "EASTL/string.h"

#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

class FileSystem;

/// Editor document for AnimationTree Canvas — Asset body is the truth source.
class AnimationTreeCanvasDocument final {
 public:
  bool isOpen() const { return m_open; }
  bool isDirty() const { return m_dirty; }
  const eastl::string& assetGuid() const { return m_guid; }
  const eastl::string& descriptorPath() const { return m_descriptor_path; }
  const eastl::string& topologyPath() const { return m_topology_path; }
  const AnimationTreeTopologyData& topology() const { return m_topology; }

  /// Open from already-loaded descriptor + topology YAML (unit tests / dual-track).
  bool openFromYaml(const eastl::string& guid,
                    const eastl::string& descriptor_path,
                    const eastl::string& topology_path,
                    const eastl::string& topology_yaml);

  /// Open descriptor virtual path; loads descriptor then `source` topology body.
  bool openFromDescriptorPath(FileSystem& file_system,
                              const eastl::string& descriptor_virtual_path);

  /// Open by Asset GUID via registry resolve (descriptor path → source).
  bool openFromAssetGuid(FileSystem& file_system,
                         const eastl::string& asset_guid,
                         const eastl::string& descriptor_virtual_path);

  void close();

  bool addBlendSpace1D(const eastl::string& node_name);
  bool addBlendSpace1DPoint(const eastl::string& node_name,
                            const eastl::string& clip_name, float scalar);
  bool addBlendSpace2D(const eastl::string& node_name);
  bool addBlendSpace2DPoint(const eastl::string& node_name,
                            const eastl::string& clip_name, float x, float y);
  bool addState(const eastl::string& state_name, const eastl::string& kind,
                const eastl::string& clip_or_blend_node);
  bool setOneShotClip(const eastl::string& clip_name);
  bool setAdd2Clip(const eastl::string& clip_name);
  bool setBaseBlendSpace1D(const eastl::string& node_name);
  bool setBaseBlendSpace2D(const eastl::string& node_name);

  bool declareTreeParamBool(const eastl::string& name, bool default_value);
  bool declareTreeParamFloat(const eastl::string& name, float default_value);

  bool addTransition(const AnimationTreeTopologyData::TransitionDef& edge);
  bool setNodePosition(const eastl::string& node_id, float x, float y);

  /// Dual-track: Inspector YAML replaces working topology (same Asset).
  bool replaceTopologyFromInspectorYaml(const eastl::string& topology_yaml);
  eastl::string exportTopologyYaml() const;

  bool save(FileSystem& file_system);

 private:
  void markDirty() { m_dirty = true; }

  bool m_open{false};
  bool m_dirty{false};
  eastl::string m_guid;
  eastl::string m_descriptor_path;
  eastl::string m_topology_path;
  AnimationTreeTopologyData m_topology;
};

/// Process-wide open Canvas document (one Asset at a time for v1).
class AnimationTreeCanvasSession final {
 public:
  static AnimationTreeCanvasSession& instance();

  AnimationTreeCanvasDocument& document() { return m_document; }
  const AnimationTreeCanvasDocument& document() const { return m_document; }

  bool openDescriptor(FileSystem& file_system,
                      const eastl::string& descriptor_virtual_path);
  bool openGuid(FileSystem& file_system, const eastl::string& asset_guid,
                const eastl::string& descriptor_virtual_path);
  void close();

 private:
  AnimationTreeCanvasDocument m_document;
};

bool isAnimationTreeAssetDescriptorPath(const eastl::string& virtual_path);

}  // namespace Blunder
