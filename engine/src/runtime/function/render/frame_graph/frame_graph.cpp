#include "runtime/function/render/frame_graph/frame_graph.h"

#include <algorithm>
#include <cstddef>
#include <queue>
#include <utility>

namespace Blunder {

GraphBuilder::GraphBuilder(FrameGraph& graph) : m_graph(&graph) {}

FrameGraphHandle GraphBuilder::createTransient(FrameGraphResourceShape shape,
                                               const FrameGraphResourceDesc& desc,
                                               const char* debug_name) {
  m_graph->noteSetupMutation();
  return m_graph->addResource(FrameGraphResourceKind::Transient, shape, desc,
                              debug_name);
}

FrameGraphHandle GraphBuilder::importExternal(const FrameGraphResourceDesc& desc,
                                              rhi::IGpuTexture* texture,
                                              const char* debug_name) {
  m_graph->noteSetupMutation();
  return m_graph->addResource(FrameGraphResourceKind::External,
                              FrameGraphResourceShape::Texture, desc,
                              debug_name, texture, nullptr);
}

FrameGraphHandle GraphBuilder::importExternal(const FrameGraphResourceDesc& desc,
                                              rhi::IGpuBuffer* buffer,
                                              const char* debug_name) {
  m_graph->noteSetupMutation();
  return m_graph->addResource(FrameGraphResourceKind::External,
                              FrameGraphResourceShape::Buffer, desc,
                              debug_name, nullptr, buffer);
}

FrameGraphPassHandle GraphBuilder::addPass(const char* debug_name) {
  m_graph->noteSetupMutation();
  FrameGraph::Pass pass;
  if (debug_name != nullptr) {
    pass.debug_name = debug_name;
  }
  const uint32_t index = static_cast<uint32_t>(m_graph->m_passes.size());
  m_graph->m_passes.push_back(std::move(pass));
  return FrameGraphPassHandle{index};
}

void GraphBuilder::markSink(FrameGraphPassHandle pass) {
  m_graph->noteSetupMutation();
  if (!m_graph->passIndexOk(pass)) {
    m_graph->m_invalid_pass = true;
    return;
  }
  m_graph->m_passes[pass.index].sink = true;
}

void GraphBuilder::read(FrameGraphPassHandle pass, FrameGraphHandle resource,
                        FrameGraphUsage usage) {
  m_graph->noteSetupMutation();
  m_graph->addAccess(pass, resource, FrameGraphAccessKind::Read, usage);
}

void GraphBuilder::write(FrameGraphPassHandle pass, FrameGraphHandle resource,
                         FrameGraphUsage usage) {
  m_graph->noteSetupMutation();
  m_graph->addAccess(pass, resource, FrameGraphAccessKind::Write, usage);
}

void GraphBuilder::setExecute(FrameGraphPassHandle pass,
                              std::function<void(IFrameGraphRecorder&)> fn) {
  m_graph->noteSetupMutation();
  if (!m_graph->passIndexOk(pass)) {
    m_graph->m_invalid_pass = true;
    return;
  }
  m_graph->m_passes[pass.index].execute = std::move(fn);
}

FrameGraphHandle FrameGraph::addResource(FrameGraphResourceKind kind,
                                         FrameGraphResourceShape shape,
                                         const FrameGraphResourceDesc& desc,
                                         const char* debug_name,
                                         rhi::IGpuTexture* texture,
                                         rhi::IGpuBuffer* buffer) {
  Resource resource;
  resource.kind = kind;
  resource.shape = shape;
  resource.desc = desc;
  resource.texture = texture;
  resource.buffer = buffer;
  if (debug_name != nullptr) {
    resource.debug_name = debug_name;
  }
  const uint32_t index = static_cast<uint32_t>(m_resources.size());
  m_resources.push_back(std::move(resource));
  return FrameGraphHandle{index};
}

bool FrameGraph::descIllegal(const Resource& resource) const {
  const FrameGraphResourceDesc& desc = resource.desc;
  if (resource.shape == FrameGraphResourceShape::Buffer) {
    return desc.size == 0;
  }
  return desc.format == FrameGraphFormat::Undefined || desc.width == 0 ||
         desc.height == 0 || desc.sample_count == 0 || desc.mip_count == 0;
}

bool FrameGraph::importIllegal(const Resource& resource) const {
  if (resource.kind != FrameGraphResourceKind::External) {
    return false;
  }
  if (resource.shape == FrameGraphResourceShape::Buffer) {
    return resource.buffer == nullptr;
  }
  return resource.texture == nullptr;
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

void FrameGraph::destroyOwnedTransients() {
  for (Resource& resource : m_resources) {
    resource.owned_texture.reset();
    resource.owned_buffer.reset();
  }
  m_allocated = false;
}

void FrameGraph::clearBarrierPlan() {
  m_barriers.clear();
  m_barriers_planned = false;
}

void FrameGraph::noteSetupMutation() {
  m_compiled = false;
  destroyOwnedTransients();
  clearCompileOutput();
  clearBarrierPlan();
}

FrameGraphCompileResult FrameGraph::fail(FrameGraphCompileReason reason) {
  m_compiled = false;
  clearCompileOutput();
  clearBarrierPlan();
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

FrameGraphResourceDesc FrameGraph::resourceDesc(
    FrameGraphHandle resource) const {
  if (!resourceIndexOk(resource)) {
    return FrameGraphResourceDesc{};
  }
  return m_resources[resource.index].desc;
}

rhi::IGpuTexture* FrameGraph::resolvedTexture(FrameGraphHandle resource) const {
  if (!resourceIndexOk(resource)) {
    return nullptr;
  }
  const Resource& stored = m_resources[resource.index];
  if (stored.shape != FrameGraphResourceShape::Texture) {
    return nullptr;
  }
  if (stored.kind == FrameGraphResourceKind::External) {
    return stored.texture;
  }
  if (stored.kind == FrameGraphResourceKind::Transient) {
    return stored.owned_texture.get();
  }
  return nullptr;
}

rhi::IGpuBuffer* FrameGraph::resolvedBuffer(FrameGraphHandle resource) const {
  if (!resourceIndexOk(resource)) {
    return nullptr;
  }
  const Resource& stored = m_resources[resource.index];
  if (stored.shape != FrameGraphResourceShape::Buffer) {
    return nullptr;
  }
  if (stored.kind == FrameGraphResourceKind::External) {
    return stored.buffer;
  }
  if (stored.kind == FrameGraphResourceKind::Transient) {
    return stored.owned_buffer.get();
  }
  return nullptr;
}

FrameGraphAllocateResult FrameGraph::allocate(IFrameGraphAllocator& allocator) {
  if (!m_compiled) {
    return FrameGraphAllocateResult{false, FrameGraphAllocateReason::NotCompiled};
  }
  if (m_allocated) {
    return FrameGraphAllocateResult{true, FrameGraphAllocateReason::Ok};
  }
  for (Resource& resource : m_resources) {
    if (resource.kind != FrameGraphResourceKind::Transient || !resource.live) {
      continue;
    }
    if (resource.shape == FrameGraphResourceShape::Texture) {
      resource.owned_texture = allocator.createTexture(resource.desc);
      if (!resource.owned_texture) {
        destroyOwnedTransients();
        return FrameGraphAllocateResult{false,
                                     FrameGraphAllocateReason::AllocFailed};
      }
    } else {
      resource.owned_buffer = allocator.createBuffer(resource.desc);
      if (!resource.owned_buffer) {
        destroyOwnedTransients();
        return FrameGraphAllocateResult{false,
                                     FrameGraphAllocateReason::AllocFailed};
      }
    }
  }
  m_allocated = true;
  return FrameGraphAllocateResult{true, FrameGraphAllocateReason::Ok};
}

FrameGraphCompileResult FrameGraph::compile() {
  m_compiled = false;
  clearCompileOutput();

  if (m_invalid_pass) {
    return fail(FrameGraphCompileReason::InvalidPass);
  }

  for (const Resource& resource : m_resources) {
    if (descIllegal(resource)) {
      return fail(FrameGraphCompileReason::InvalidDesc);
    }
  }

  for (const Resource& resource : m_resources) {
    if (importIllegal(resource)) {
      return fail(FrameGraphCompileReason::InvalidImport);
    }
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

  m_compiled = true;
  return FrameGraphCompileResult{true, FrameGraphCompileReason::Ok};
}

const std::vector<FrameGraphBarrier>& FrameGraph::barriers() const {
  if (!m_compiled || !m_barriers_planned) {
    return m_empty_barriers;
  }
  return m_barriers;
}

FrameGraphPlanBarriersResult FrameGraph::planBarriers() {
  if (!m_compiled) {
    return FrameGraphPlanBarriersResult{false,
                                        FrameGraphPlanBarriersReason::NotCompiled};
  }
  if (m_barriers_planned) {
    return FrameGraphPlanBarriersResult{true, FrameGraphPlanBarriersReason::Ok};
  }

  m_barriers.clear();
  std::vector<uint32_t> live_pos(m_passes.size(), ~0u);
  for (uint32_t i = 0; i < static_cast<uint32_t>(m_live_passes.size()); ++i) {
    live_pos[m_live_passes[i].index] = i;
  }

  const uint32_t resource_count = static_cast<uint32_t>(m_resources.size());
  for (uint32_t resource_index = 0; resource_index < resource_count;
       ++resource_index) {
    const Resource& resource = m_resources[resource_index];
    if (!resource.live) {
      continue;
    }

    struct ChainPass {
      FrameGraphPassHandle pass{};
      FrameGraphResourceState first{};
      FrameGraphResourceState last{};
    };
    std::vector<ChainPass> chain;
    for (const FrameGraphPassHandle& pass_handle : m_live_passes) {
      const Pass& pass = m_passes[pass_handle.index];
      ChainPass entry;
      entry.pass = pass_handle;
      bool any = false;
      for (const Access& access : pass.accesses) {
        if (access.resource.index != resource_index) {
          continue;
        }
        FrameGraphResourceState state;
        state.undefined = false;
        state.access = access.kind;
        state.usage = access.usage;
        if (!any) {
          entry.first = state;
          any = true;
        }
        entry.last = state;
      }
      if (any) {
        chain.push_back(entry);
      }
    }
    if (chain.empty()) {
      continue;
    }

    const FrameGraphHandle handle{resource_index};
    if (resource.kind == FrameGraphResourceKind::Transient) {
      FrameGraphBarrier row;
      row.resource = handle;
      row.from.undefined = true;
      row.to = chain.front().first;
      row.after = FrameGraphPassHandle{};
      row.before = chain.front().pass;
      m_barriers.push_back(row);
    }
    for (uint32_t i = 1; i < static_cast<uint32_t>(chain.size()); ++i) {
      const FrameGraphResourceState& from = chain[i - 1].last;
      const FrameGraphResourceState& to = chain[i].first;
      if (!from.undefined && !to.undefined &&
          from.access == FrameGraphAccessKind::Read &&
          to.access == FrameGraphAccessKind::Read && from.usage == to.usage) {
        continue;
      }
      FrameGraphBarrier row;
      row.resource = handle;
      row.from = from;
      row.to = to;
      row.after = chain[i - 1].pass;
      row.before = chain[i].pass;
      m_barriers.push_back(row);
    }
  }

  std::sort(m_barriers.begin(), m_barriers.end(),
            [&](const FrameGraphBarrier& a, const FrameGraphBarrier& b) {
              const uint32_t a_pos = live_pos[a.before.index];
              const uint32_t b_pos = live_pos[b.before.index];
              if (a_pos != b_pos) {
                return a_pos < b_pos;
              }
              return a.resource.index < b.resource.index;
            });

  m_barriers_planned = true;
  return FrameGraphPlanBarriersResult{true, FrameGraphPlanBarriersReason::Ok};
}

// Callbacks must not reenter GraphBuilder / compile / execute. Snapshot live
// order, barriers, and callbacks so a forbidden Setup mutation does not UAF;
// remaining Passes still record from that copy and execute still returns Ok.
FrameGraphExecuteResult FrameGraph::execute(IFrameGraphRecorder& recorder) {
  if (!m_compiled) {
    return FrameGraphExecuteResult{false, FrameGraphExecuteReason::NotCompiled};
  }
  if (!m_allocated) {
    return FrameGraphExecuteResult{false, FrameGraphExecuteReason::NotAllocated};
  }
  if (!m_barriers_planned) {
    return FrameGraphExecuteResult{false, FrameGraphExecuteReason::NotPlanned};
  }
  const std::vector<FrameGraphPassHandle> passes = m_live_passes;
  const std::vector<FrameGraphBarrier> planned = m_barriers;
  std::vector<std::function<void(IFrameGraphRecorder&)>> callbacks;
  callbacks.reserve(passes.size());
  for (const FrameGraphPassHandle& pass : passes) {
    callbacks.push_back(m_passes[pass.index].execute);
  }
  for (size_t i = 0; i < passes.size(); ++i) {
    const uint32_t pass_index = passes[i].index;
    for (const FrameGraphBarrier& barrier : planned) {
      if (barrier.before.index == pass_index) {
        recorder.pipelineBarrier(barrier);
      }
    }
    if (callbacks[i]) {
      callbacks[i](recorder);
    }
  }
  return FrameGraphExecuteResult{true, FrameGraphExecuteReason::Ok};
}

}  // namespace Blunder
