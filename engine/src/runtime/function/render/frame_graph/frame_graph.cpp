#include "runtime/function/render/frame_graph/frame_graph.h"

#include <queue>
#include <utility>

namespace Blunder {

GraphBuilder::GraphBuilder(FrameGraph& graph) : m_graph(&graph) {}

FrameGraphHandle GraphBuilder::createTransient(FrameGraphResourceShape shape,
                                               const char* debug_name) {
  return m_graph->addResource(FrameGraphResourceKind::Transient, shape,
                              debug_name);
}

FrameGraphHandle GraphBuilder::importExternal(FrameGraphResourceShape shape,
                                              const char* debug_name) {
  return m_graph->addResource(FrameGraphResourceKind::External, shape,
                              debug_name);
}

FrameGraphPassHandle GraphBuilder::addPass(const char* debug_name) {
  FrameGraph::Pass pass;
  if (debug_name != nullptr) {
    pass.debug_name = debug_name;
  }
  const uint32_t index = static_cast<uint32_t>(m_graph->m_passes.size());
  m_graph->m_passes.push_back(std::move(pass));
  return FrameGraphPassHandle{index};
}

void GraphBuilder::markSink(FrameGraphPassHandle pass) {
  if (!m_graph->passIndexOk(pass)) {
    m_graph->m_invalid_pass = true;
    return;
  }
  m_graph->m_passes[pass.index].sink = true;
}

void GraphBuilder::read(FrameGraphPassHandle pass, FrameGraphHandle resource,
                        FrameGraphUsage usage) {
  m_graph->addAccess(pass, resource, FrameGraphAccessKind::Read, usage);
}

void GraphBuilder::write(FrameGraphPassHandle pass, FrameGraphHandle resource,
                         FrameGraphUsage usage) {
  m_graph->addAccess(pass, resource, FrameGraphAccessKind::Write, usage);
}

FrameGraphHandle FrameGraph::addResource(FrameGraphResourceKind kind,
                                         FrameGraphResourceShape shape,
                                         const char* debug_name) {
  Resource resource;
  resource.kind = kind;
  resource.shape = shape;
  if (debug_name != nullptr) {
    resource.debug_name = debug_name;
  }
  const uint32_t index = static_cast<uint32_t>(m_resources.size());
  m_resources.push_back(std::move(resource));
  return FrameGraphHandle{index};
}

void FrameGraph::addAccess(FrameGraphPassHandle pass, FrameGraphHandle resource,
                           FrameGraphAccessKind kind, FrameGraphUsage usage) {
  if (!passIndexOk(pass)) {
    m_invalid_pass = true;
    return;
  }
  m_passes[pass.index].accesses.push_back(Access{resource, kind, usage});
}

bool FrameGraph::resourceIndexOk(FrameGraphHandle resource) const {
  return resource.valid() && resource.index < m_resources.size();
}

bool FrameGraph::passIndexOk(FrameGraphPassHandle pass) const {
  return pass.valid() && pass.index < m_passes.size();
}

void FrameGraph::clearCompileOutput() {
  m_live_passes.clear();
  for (Resource& resource : m_resources) {
    resource.live = false;
    resource.lifetime = FrameGraphLifetime{};
  }
}

FrameGraphCompileResult FrameGraph::fail(FrameGraphCompileReason reason) {
  clearCompileOutput();
  return FrameGraphCompileResult{false, reason};
}

bool FrameGraph::isResourceLive(FrameGraphHandle resource) const {
  if (!resourceIndexOk(resource)) {
    return false;
  }
  return m_resources[resource.index].live;
}

FrameGraphLifetime FrameGraph::lifetime(FrameGraphHandle resource) const {
  if (!resourceIndexOk(resource)) {
    return FrameGraphLifetime{};
  }
  return m_resources[resource.index].lifetime;
}

FrameGraphResourceKind FrameGraph::resourceKind(
    FrameGraphHandle resource) const {
  if (!resourceIndexOk(resource)) {
    return FrameGraphResourceKind::Transient;
  }
  return m_resources[resource.index].kind;
}

FrameGraphResourceShape FrameGraph::resourceShape(
    FrameGraphHandle resource) const {
  if (!resourceIndexOk(resource)) {
    return FrameGraphResourceShape::Texture;
  }
  return m_resources[resource.index].shape;
}

FrameGraphCompileResult FrameGraph::compile() {
  clearCompileOutput();

  if (m_invalid_pass) {
    return fail(FrameGraphCompileReason::InvalidPass);
  }

  const uint32_t pass_count = static_cast<uint32_t>(m_passes.size());
  bool any_sink = false;
  bool dangling = false;
  for (const Pass& pass : m_passes) {
    if (pass.sink) {
      any_sink = true;
    }
    for (const Access& access : pass.accesses) {
      if (!resourceIndexOk(access.resource)) {
        dangling = true;
      }
    }
  }
  if (dangling) {
    return fail(FrameGraphCompileReason::DanglingAccess);
  }
  if (!any_sink) {
    return fail(FrameGraphCompileReason::NoSink);
  }

  std::vector<std::vector<uint32_t>> adj(pass_count);
  std::vector<std::vector<uint32_t>> radj(pass_count);

  auto add_edge = [&](uint32_t from, uint32_t to) {
    if (from == to) {
      return;
    }
    for (uint32_t existing : adj[from]) {
      if (existing == to) {
        return;
      }
    }
    adj[from].push_back(to);
    radj[to].push_back(from);
  };

  const uint32_t resource_count = static_cast<uint32_t>(m_resources.size());
  for (uint32_t resource_index = 0; resource_index < resource_count;
       ++resource_index) {
    uint32_t last_write = ~0u;
    std::vector<uint32_t> reads_since_write;
    for (uint32_t pass_index = 0; pass_index < pass_count; ++pass_index) {
      for (const Access& access : m_passes[pass_index].accesses) {
        if (access.resource.index != resource_index) {
          continue;
        }
        if (access.kind == FrameGraphAccessKind::Read) {
          if (last_write != ~0u) {
            add_edge(last_write, pass_index);
          }
          reads_since_write.push_back(pass_index);
        } else {
          if (last_write != ~0u) {
            add_edge(last_write, pass_index);
          }
          for (uint32_t reader : reads_since_write) {
            add_edge(reader, pass_index);
          }
          reads_since_write.clear();
          last_write = pass_index;
        }
      }
    }
  }

  std::vector<uint8_t> reach_sink(pass_count, 0);
  std::queue<uint32_t> walk;
  for (uint32_t pass_index = 0; pass_index < pass_count; ++pass_index) {
    if (m_passes[pass_index].sink) {
      walk.push(pass_index);
      reach_sink[pass_index] = 1;
    }
  }
  while (!walk.empty()) {
    const uint32_t pass_index = walk.front();
    walk.pop();
    for (uint32_t pred : radj[pass_index]) {
      if (reach_sink[pred] != 0) {
        continue;
      }
      reach_sink[pred] = 1;
      walk.push(pred);
    }
  }

  uint32_t live_count = 0;
  std::vector<uint32_t> live_indegree(pass_count, 0);
  for (uint32_t pass_index = 0; pass_index < pass_count; ++pass_index) {
    if (reach_sink[pass_index] == 0) {
      continue;
    }
    ++live_count;
    for (uint32_t next : adj[pass_index]) {
      if (reach_sink[next] != 0) {
        ++live_indegree[next];
      }
    }
  }

  std::queue<uint32_t> ready;
  for (uint32_t pass_index = 0; pass_index < pass_count; ++pass_index) {
    if (reach_sink[pass_index] != 0 && live_indegree[pass_index] == 0) {
      ready.push(pass_index);
    }
  }

  std::vector<uint32_t> topo;
  topo.reserve(live_count);
  std::vector<uint32_t> remaining_indegree = live_indegree;
  while (!ready.empty()) {
    const uint32_t pass_index = ready.front();
    ready.pop();
    topo.push_back(pass_index);
    for (uint32_t next : adj[pass_index]) {
      if (reach_sink[next] == 0) {
        continue;
      }
      --remaining_indegree[next];
      if (remaining_indegree[next] == 0) {
        ready.push(next);
      }
    }
  }
  if (topo.size() != live_count) {
    return fail(FrameGraphCompileReason::Cycle);
  }

  for (uint32_t pass_index : topo) {
    m_live_passes.push_back(FrameGraphPassHandle{pass_index});
  }

  for (uint32_t live_i = 0;
       live_i < static_cast<uint32_t>(m_live_passes.size()); ++live_i) {
    const uint32_t pass_index = m_live_passes[live_i].index;
    for (const Access& access : m_passes[pass_index].accesses) {
      Resource& resource = m_resources[access.resource.index];
      if (!resource.live) {
        resource.live = true;
        resource.lifetime.first = live_i;
        resource.lifetime.last = live_i;
      } else {
        resource.lifetime.last = live_i;
      }
    }
  }

  return FrameGraphCompileResult{true, FrameGraphCompileReason::Ok};
}

}  // namespace Blunder
