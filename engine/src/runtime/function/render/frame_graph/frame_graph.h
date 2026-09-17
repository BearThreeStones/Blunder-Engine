#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Blunder {

enum class FrameGraphResourceKind : uint8_t {
  Transient = 0,
  External = 1,
};

enum class FrameGraphResourceShape : uint8_t {
  Texture = 0,
  Buffer = 1,
};

enum class FrameGraphAccessKind : uint8_t {
  Read = 0,
  Write = 1,
};

enum class FrameGraphUsage : uint8_t {
  Sampled = 0,
  ColorAttachment = 1,
  DepthAttachment = 2,
  Storage = 3,
};

enum class FrameGraphCompileReason : uint8_t {
  Ok = 0,
  Cycle = 1,
  DanglingAccess = 2,
  NoSink = 3,
  InvalidPass = 4,
};

struct FrameGraphHandle {
  uint32_t index{~0u};

  bool valid() const { return index != ~0u; }
};

struct FrameGraphPassHandle {
  uint32_t index{~0u};

  bool valid() const { return index != ~0u; }
};

struct FrameGraphCompileResult {
  bool ok{false};
  FrameGraphCompileReason reason{FrameGraphCompileReason::NoSink};
};

struct FrameGraphLifetime {
  uint32_t first{0};
  uint32_t last{0};
};

class FrameGraph;

class GraphBuilder final {
 public:
  explicit GraphBuilder(FrameGraph& graph);

  FrameGraphHandle createTransient(FrameGraphResourceShape shape,
                                   const char* debug_name = nullptr);
  FrameGraphHandle importExternal(FrameGraphResourceShape shape,
                                  const char* debug_name = nullptr);

  FrameGraphPassHandle addPass(const char* debug_name = nullptr);
  void markSink(FrameGraphPassHandle pass);
  void read(FrameGraphPassHandle pass, FrameGraphHandle resource,
            FrameGraphUsage usage);
  void write(FrameGraphPassHandle pass, FrameGraphHandle resource,
             FrameGraphUsage usage);

 private:
  FrameGraph* m_graph{nullptr};
};

class FrameGraph final {
 public:
  FrameGraph() = default;

  FrameGraphCompileResult compile();

  const std::vector<FrameGraphPassHandle>& livePasses() const {
    return m_live_passes;
  }

  bool isResourceLive(FrameGraphHandle resource) const;
  FrameGraphLifetime lifetime(FrameGraphHandle resource) const;
  FrameGraphResourceKind resourceKind(FrameGraphHandle resource) const;
  FrameGraphResourceShape resourceShape(FrameGraphHandle resource) const;

 private:
  friend class GraphBuilder;

  struct Access {
    FrameGraphHandle resource{};
    FrameGraphAccessKind kind{FrameGraphAccessKind::Read};
    FrameGraphUsage usage{FrameGraphUsage::Sampled};
  };

  struct Resource {
    FrameGraphResourceKind kind{FrameGraphResourceKind::Transient};
    FrameGraphResourceShape shape{FrameGraphResourceShape::Texture};
    std::string debug_name;
    bool live{false};
    FrameGraphLifetime lifetime{};
  };

  struct Pass {
    std::string debug_name;
    bool sink{false};
    std::vector<Access> accesses;
  };

  FrameGraphHandle addResource(FrameGraphResourceKind kind,
                               FrameGraphResourceShape shape,
                               const char* debug_name);
  void addAccess(FrameGraphPassHandle pass, FrameGraphHandle resource,
                 FrameGraphAccessKind kind, FrameGraphUsage usage);
  bool resourceIndexOk(FrameGraphHandle resource) const;
  bool passIndexOk(FrameGraphPassHandle pass) const;
  void clearCompileOutput();
  FrameGraphCompileResult fail(FrameGraphCompileReason reason);

  std::vector<Resource> m_resources;
  std::vector<Pass> m_passes;
  std::vector<FrameGraphPassHandle> m_live_passes;
  bool m_invalid_pass{false};
};

}  // namespace Blunder
