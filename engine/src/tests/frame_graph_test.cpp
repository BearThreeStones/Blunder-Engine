#include "runtime/function/render/frame_graph/frame_graph.h"
#include "runtime/function/render/rhi/i_gpu_buffer.h"
#include "runtime/function/render/rhi/i_gpu_texture.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

class DummyGpuTexture final : public Blunder::rhi::IGpuTexture {};

class DummyGpuBuffer final : public Blunder::rhi::IGpuBuffer {
 public:
  void upload(const void*, uint64_t) override {}
  uint64_t size() const override { return 0; }
};

class DummyRecorder final : public Blunder::IFrameGraphRecorder {
 public:
  enum Kind : uint8_t { Barrier = 0, Callback = 1 };

  struct Event {
    Kind kind{Barrier};
    Blunder::FrameGraphBarrier barrier{};
    int callback_id{0};
  };

  std::vector<Event> events;
  std::vector<Blunder::FrameGraphBarrier> barriers;

  void pipelineBarrier(const Blunder::FrameGraphBarrier& barrier) override {
    barriers.push_back(barrier);
    Event event;
    event.kind = Barrier;
    event.barrier = barrier;
    events.push_back(event);
  }

  void noteCallback(int id) {
    Event event;
    event.kind = Callback;
    event.callback_id = id;
    events.push_back(event);
  }
};

Blunder::FrameGraphExecuteResult runExecute(Blunder::FrameGraph& graph) {
  DummyRecorder recorder;
  return graph.execute(recorder);
}

bool planOk(Blunder::FrameGraph& graph) {
  const Blunder::FrameGraphPlanBarriersResult planned = graph.planBarriers();
  return planned.ok &&
         planned.reason == Blunder::FrameGraphPlanBarriersReason::Ok;
}

class DummyAllocator final : public Blunder::IFrameGraphAllocator {
 public:
  int texture_creates{0};
  int buffer_creates{0};
  int fail_texture_at{0};
  int fail_buffer_at{0};
  Blunder::FrameGraphResourceDesc last_texture_desc{};
  Blunder::FrameGraphResourceDesc last_buffer_desc{};

  std::unique_ptr<Blunder::rhi::IGpuTexture> createTexture(
      const Blunder::FrameGraphResourceDesc& desc) override {
    last_texture_desc = desc;
    ++texture_creates;
    if (fail_texture_at != 0 && texture_creates == fail_texture_at) {
      return nullptr;
    }
    return std::make_unique<DummyGpuTexture>();
  }

  std::unique_ptr<Blunder::rhi::IGpuBuffer> createBuffer(
      const Blunder::FrameGraphResourceDesc& desc) override {
    last_buffer_desc = desc;
    ++buffer_creates;
    if (fail_buffer_at != 0 && buffer_creates == fail_buffer_at) {
      return nullptr;
    }
    return std::make_unique<DummyGpuBuffer>();
  }
};

bool allocateOk(Blunder::FrameGraph& graph) {
  DummyAllocator allocator;
  const Blunder::FrameGraphAllocateResult result = graph.allocate(allocator);
  return result.ok &&
         result.reason == Blunder::FrameGraphAllocateReason::Ok;
}

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

int liveIndex(const Blunder::FrameGraph& graph,
              Blunder::FrameGraphPassHandle pass) {
  const std::vector<Blunder::FrameGraphPassHandle>& live = graph.livePasses();
  for (int i = 0; i < static_cast<int>(live.size()); ++i) {
    if (live[static_cast<std::size_t>(i)].index == pass.index) {
      return i;
    }
  }
  return -1;
}

Blunder::FrameGraphResourceDesc legalColorDesc() {
  Blunder::FrameGraphResourceDesc desc;
  desc.format = Blunder::FrameGraphFormat::R8G8B8A8_UNORM;
  desc.width = 8;
  desc.height = 8;
  desc.sample_count = 1;
  desc.mip_count = 1;
  return desc;
}

Blunder::FrameGraphResourceDesc legalDepthDesc() {
  Blunder::FrameGraphResourceDesc desc = legalColorDesc();
  desc.format = Blunder::FrameGraphFormat::D32_SFLOAT;
  return desc;
}

Blunder::FrameGraphResourceDesc legalBufferDesc(uint64_t bytes) {
  Blunder::FrameGraphResourceDesc desc;
  desc.size = bytes;
  return desc;
}

bool descEqual(const Blunder::FrameGraphResourceDesc& a,
               const Blunder::FrameGraphResourceDesc& b) {
  return a.format == b.format && a.width == b.width && a.height == b.height &&
         a.sample_count == b.sample_count && a.mip_count == b.mip_count &&
         a.size == b.size;
}

void testTwoPassTransientThenSink() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                              "color");
  const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
  builder.write(writer, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.read(sink, color, Blunder::FrameGraphUsage::Sampled);
  builder.markSink(sink);

  expect_true("two-pass desc before compile",
              descEqual(graph.resourceDesc(color), legalColorDesc()));
  expect_true("two-pass kind transient before compile",
              graph.resourceKind(color) ==
                  Blunder::FrameGraphResourceKind::Transient);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("two-pass compile ok", result.ok);
  expect_true("two-pass reason ok",
              result.reason == Blunder::FrameGraphCompileReason::Ok);
  expect_true("two-pass live order",
              graph.livePasses().size() == 2 &&
                  graph.livePasses()[0].index == writer.index &&
                  graph.livePasses()[1].index == sink.index);
  const Blunder::FrameGraphLifetime life = graph.lifetime(color);
  expect_true("two-pass resource live", graph.isResourceLive(color));
  expect_true("two-pass lifetime covers both",
              life.first == 0 && life.last == 1);
  expect_true("two-pass kind transient",
              graph.resourceKind(color) ==
                  Blunder::FrameGraphResourceKind::Transient);
}

void testUnreachablePostProcessDropped() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                              "color");
  const Blunder::FrameGraphHandle bloom =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                              "bloom");
  const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
  builder.write(writer, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.read(sink, color, Blunder::FrameGraphUsage::Sampled);
  builder.markSink(sink);
  const Blunder::FrameGraphPassHandle post = builder.addPass("bloom");
  builder.write(post, bloom, Blunder::FrameGraphUsage::ColorAttachment);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("dce compile ok", result.ok);
  expect_true("dce live order omits post",
              graph.livePasses().size() == 2 &&
                  graph.livePasses()[0].index == writer.index &&
                  graph.livePasses()[1].index == sink.index);
  expect_true("dce color live", graph.isResourceLive(color));
  expect_true("dce bloom not live", !graph.isResourceLive(bloom));
  expect_true("dce bloom desc kept",
              descEqual(graph.resourceDesc(bloom), legalColorDesc()));
}

void testImportedExternalStaysExternal() {
  DummyGpuTexture dummy;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle viewport =
      builder.importExternal(legalColorDesc(), &dummy, "viewport");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("forward");
  builder.write(sink, viewport, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);

  expect_true("external desc before compile",
              descEqual(graph.resourceDesc(viewport), legalColorDesc()));
  expect_true("external texture before compile",
              graph.resolvedTexture(viewport) == &dummy);
  expect_true("external kind before compile",
              graph.resourceKind(viewport) ==
                  Blunder::FrameGraphResourceKind::External);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("external compile ok", result.ok);
  expect_true("external stays external",
              graph.resourceKind(viewport) ==
                  Blunder::FrameGraphResourceKind::External);
  expect_true("external live", graph.isResourceLive(viewport));
  expect_true("external not transient",
              graph.resourceKind(viewport) !=
                  Blunder::FrameGraphResourceKind::Transient);
  expect_true("external texture after compile",
              graph.resolvedTexture(viewport) == &dummy);
}

void testBufferWriteThenRead() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle buf =
      builder.createTransient(Blunder::FrameGraphResourceShape::Buffer, legalBufferDesc(256),
                              "buf");
  const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
  builder.write(writer, buf, Blunder::FrameGraphUsage::Storage);
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.read(sink, buf, Blunder::FrameGraphUsage::Storage);
  builder.markSink(sink);

  expect_true("buffer size before compile",
              graph.resourceDesc(buf).size == 256);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("buffer compile ok", result.ok);
  expect_true("buffer live order",
              graph.livePasses().size() == 2 &&
                  graph.livePasses()[0].index == writer.index &&
                  graph.livePasses()[1].index == sink.index);
  const Blunder::FrameGraphLifetime life = graph.lifetime(buf);
  expect_true("buffer lifetime covers both",
              graph.isResourceLive(buf) && life.first == 0 && life.last == 1);
  expect_true("buffer shape",
              graph.resourceShape(buf) ==
                  Blunder::FrameGraphResourceShape::Buffer);
}

void testPingPongIsNotCycle() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle a =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(), "a");
  const Blunder::FrameGraphHandle b =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(), "b");
  const Blunder::FrameGraphPassHandle pass_a = builder.addPass("a");
  builder.write(pass_a, a, Blunder::FrameGraphUsage::ColorAttachment);
  builder.read(pass_a, b, Blunder::FrameGraphUsage::Sampled);
  const Blunder::FrameGraphPassHandle pass_b = builder.addPass("b");
  builder.write(pass_b, b, Blunder::FrameGraphUsage::ColorAttachment);
  builder.read(pass_b, a, Blunder::FrameGraphUsage::Sampled);
  builder.markSink(pass_b);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("ping-pong compile ok", result.ok);
  expect_true("ping-pong reason ok",
              result.reason == Blunder::FrameGraphCompileReason::Ok);
  expect_true("ping-pong live order",
              graph.livePasses().size() == 2 &&
                  graph.livePasses()[0].index == pass_a.index &&
                  graph.livePasses()[1].index == pass_b.index);
}

void testDanglingAccessFails() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.read(sink, Blunder::FrameGraphHandle{99},
               Blunder::FrameGraphUsage::Sampled);
  builder.markSink(sink);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("dangling not ok", !result.ok);
  expect_true("dangling reason",
              result.reason == Blunder::FrameGraphCompileReason::DanglingAccess);
  expect_true("dangling empty live", graph.livePasses().empty());
}

void testUnreachableCycleIsNotFailure() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                              "color");
  const Blunder::FrameGraphHandle a =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(), "a");
  const Blunder::FrameGraphHandle b =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(), "b");
  const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
  builder.write(writer, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.read(sink, color, Blunder::FrameGraphUsage::Sampled);
  builder.markSink(sink);
  const Blunder::FrameGraphPassHandle dead_a = builder.addPass("dead_a");
  builder.write(dead_a, a, Blunder::FrameGraphUsage::ColorAttachment);
  builder.read(dead_a, b, Blunder::FrameGraphUsage::Sampled);
  const Blunder::FrameGraphPassHandle dead_b = builder.addPass("dead_b");
  builder.write(dead_b, b, Blunder::FrameGraphUsage::ColorAttachment);
  builder.read(dead_b, a, Blunder::FrameGraphUsage::Sampled);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("dead-cycle compile ok", result.ok);
  expect_true("dead-cycle live omits cycle",
              graph.livePasses().size() == 2 &&
                  graph.livePasses()[0].index == writer.index &&
                  graph.livePasses()[1].index == sink.index);
}

void testEmptyGraphFails() {
  Blunder::FrameGraph graph;
  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("empty not ok", !result.ok);
  expect_true("empty reason",
              result.reason == Blunder::FrameGraphCompileReason::NoSink);
  expect_true("empty live", graph.livePasses().empty());
}

void testDanglingWriteFails() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, Blunder::FrameGraphHandle{99},
                Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("dangling-write not ok", !result.ok);
  expect_true("dangling-write reason",
              result.reason == Blunder::FrameGraphCompileReason::DanglingAccess);
  expect_true("dangling-write empty live", graph.livePasses().empty());
}

void testZeroSinksFails() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                              "color");
  const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
  builder.write(writer, color, Blunder::FrameGraphUsage::ColorAttachment);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("no-sink not ok", !result.ok);
  expect_true("no-sink reason",
              result.reason == Blunder::FrameGraphCompileReason::NoSink);
  expect_true("no-sink empty live", graph.livePasses().empty());
}

void testColorAttachmentWriteChain() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                              "color");
  const Blunder::FrameGraphPassHandle clear = builder.addPass("clear");
  builder.write(clear, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle forward = builder.addPass("forward");
  builder.write(forward, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(forward);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("waw compile ok", result.ok);
  expect_true("waw live order",
              graph.livePasses().size() == 2 &&
                  graph.livePasses()[0].index == clear.index &&
                  graph.livePasses()[1].index == forward.index);
  const Blunder::FrameGraphLifetime life = graph.lifetime(color);
  expect_true("waw resource live", graph.isResourceLive(color));
  expect_true("waw lifetime covers both",
              life.first == 0 && life.last == 1);
}

void testWarReaderBeforeLaterWrite() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                              "color");
  const Blunder::FrameGraphPassHandle producer = builder.addPass("producer");
  builder.write(producer, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle ssao = builder.addPass("ssao");
  builder.read(ssao, color, Blunder::FrameGraphUsage::Sampled);
  const Blunder::FrameGraphPassHandle taa = builder.addPass("taa");
  builder.write(taa, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(taa);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("war compile ok", result.ok);
  expect_true("war live order",
              graph.livePasses().size() == 3 &&
                  graph.livePasses()[0].index == producer.index &&
                  graph.livePasses()[1].index == ssao.index &&
                  graph.livePasses()[2].index == taa.index);
}

void testTwoReadersBeforeWrite() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                              "color");
  const Blunder::FrameGraphPassHandle reader_a = builder.addPass("a");
  builder.read(reader_a, color, Blunder::FrameGraphUsage::Sampled);
  const Blunder::FrameGraphPassHandle reader_b = builder.addPass("b");
  builder.read(reader_b, color, Blunder::FrameGraphUsage::Sampled);
  const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
  builder.write(writer, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(writer);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  const int index_a = liveIndex(graph, reader_a);
  const int index_b = liveIndex(graph, reader_b);
  const int index_w = liveIndex(graph, writer);
  expect_true("two-readers compile ok", result.ok);
  expect_true("two-readers all live",
              graph.livePasses().size() == 3 && index_a >= 0 &&
                  index_b >= 0 && index_w >= 0);
  expect_true("two-readers before write",
              index_a < index_w && index_b < index_w);
}

void testSamePassAccessIsNotCycle() {
  {
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle color =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                                "color");
    const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
    builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
    builder.read(sink, color, Blunder::FrameGraphUsage::Sampled);
    builder.markSink(sink);

    const Blunder::FrameGraphCompileResult result = graph.compile();
    expect_true("same-pass rw compile ok", result.ok);
    expect_true("same-pass rw not cycle",
                result.reason == Blunder::FrameGraphCompileReason::Ok);
  }
  {
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle color =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                                "color");
    const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
    builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
    builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
    builder.markSink(sink);

    const Blunder::FrameGraphCompileResult result = graph.compile();
    expect_true("same-pass ww compile ok", result.ok);
    expect_true("same-pass ww not cycle",
                result.reason == Blunder::FrameGraphCompileReason::Ok);
  }
}

void testInvalidPassFails() {
  const Blunder::FrameGraphPassHandle bad{99};
  {
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle color =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                                "color");
    builder.read(bad, color, Blunder::FrameGraphUsage::Sampled);

    const Blunder::FrameGraphCompileResult result = graph.compile();
    expect_true("invalid-read not ok", !result.ok);
    expect_true("invalid-read reason",
                result.reason == Blunder::FrameGraphCompileReason::InvalidPass);
    expect_true("invalid-read empty live", graph.livePasses().empty());
  }
  {
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle color =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                                "color");
    builder.write(bad, color, Blunder::FrameGraphUsage::ColorAttachment);

    const Blunder::FrameGraphCompileResult result = graph.compile();
    expect_true("invalid-write not ok", !result.ok);
    expect_true("invalid-write reason",
                result.reason == Blunder::FrameGraphCompileReason::InvalidPass);
    expect_true("invalid-write empty live", graph.livePasses().empty());
  }
  {
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    builder.markSink(bad);

    const Blunder::FrameGraphCompileResult result = graph.compile();
    expect_true("invalid-sink not ok", !result.ok);
    expect_true("invalid-sink reason",
                result.reason == Blunder::FrameGraphCompileReason::InvalidPass);
    expect_true("invalid-sink empty live", graph.livePasses().empty());
  }
}

void testFailureReasonPriority() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphPassHandle bad{99};
  const Blunder::FrameGraphPassHandle pass = builder.addPass("pass");
  builder.write(bad, Blunder::FrameGraphHandle{99},
                Blunder::FrameGraphUsage::ColorAttachment);
  builder.write(pass, Blunder::FrameGraphHandle{99},
                Blunder::FrameGraphUsage::ColorAttachment);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("priority not ok", !result.ok);
  expect_true("priority invalid-pass first",
              result.reason == Blunder::FrameGraphCompileReason::InvalidPass);
  expect_true("priority empty live", graph.livePasses().empty());
}

void testDanglingBeatsNoSink() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphPassHandle pass = builder.addPass("pass");
  builder.write(pass, Blunder::FrameGraphHandle{99},
                Blunder::FrameGraphUsage::ColorAttachment);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("dangling-vs-nosink not ok", !result.ok);
  expect_true("dangling-vs-nosink reason",
              result.reason == Blunder::FrameGraphCompileReason::DanglingAccess);
  expect_true("dangling-vs-nosink empty live", graph.livePasses().empty());
}

void testLiveCallbacksRunInOrderAndDceSkipped() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                              "color");
  const Blunder::FrameGraphHandle bloom =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                              "bloom");
  const Blunder::FrameGraphPassHandle clear = builder.addPass("clear");
  builder.write(clear, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle forward = builder.addPass("forward");
  builder.write(forward, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(forward);
  const Blunder::FrameGraphPassHandle post = builder.addPass("bloom");
  builder.write(post, bloom, Blunder::FrameGraphUsage::ColorAttachment);

  int order[4] = {0, 0, 0, 0};
  int next = 0;
  int bloom_runs = 0;
  builder.setExecute(clear, [&](Blunder::IFrameGraphRecorder&) { order[next++] = 1; });
  builder.setExecute(forward, [&](Blunder::IFrameGraphRecorder&) { order[next++] = 2; });
  builder.setExecute(post, [&](Blunder::IFrameGraphRecorder&) { ++bloom_runs; });

  const Blunder::FrameGraphCompileResult compiled = graph.compile();
  expect_true("exec-dce compile ok", compiled.ok);
  expect_true("exec-dce allocate ok", allocateOk(graph));
  expect_true("exec-dce plan ok", planOk(graph));
  const Blunder::FrameGraphExecuteResult executed = runExecute(graph);
  expect_true("exec-dce execute ok", executed.ok);
  expect_true("exec-dce reason ok",
              executed.reason == Blunder::FrameGraphExecuteReason::Ok);
  expect_true("exec-dce live order ran",
              next == 2 && order[0] == 1 && order[1] == 2);
  expect_true("exec-dce culled never ran", bloom_runs == 0);
}

void testLivePassNoCallbackIsEmptyRun() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                              "color");
  const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
  builder.write(writer, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.read(sink, color, Blunder::FrameGraphUsage::Sampled);
  builder.markSink(sink);

  int writer_runs = 0;
  builder.setExecute(writer, [&](Blunder::IFrameGraphRecorder&) { ++writer_runs; });

  DummyRecorder dummy;
  expect_true("empty-run compile ok", graph.compile().ok);
  expect_true("empty-run allocate ok", allocateOk(graph));
  expect_true("empty-run plan ok", planOk(graph));
  const Blunder::FrameGraphExecuteResult executed = graph.execute(dummy);
  expect_true("empty-run execute ok", executed.ok);
  expect_true("empty-run writer ran", writer_runs == 1);
  expect_true("empty-run live still two", graph.livePasses().size() == 2);
  bool sink_barrier = false;
  for (const Blunder::FrameGraphBarrier& row : dummy.barriers) {
    if (row.before.index == sink.index) {
      sink_barrier = true;
    }
  }
  expect_true("empty-run sink barriers recorded", sink_barrier);
}

void testExecuteNotCompiled() {
  {
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle color =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                                "color");
    const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
    builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
    builder.markSink(sink);
    int runs = 0;
    builder.setExecute(sink, [&](Blunder::IFrameGraphRecorder&) { ++runs; });

    const Blunder::FrameGraphExecuteResult executed = runExecute(graph);
    expect_true("before-compile not ok", !executed.ok);
    expect_true("before-compile notcompiled",
                executed.reason ==
                    Blunder::FrameGraphExecuteReason::NotCompiled);
    expect_true("before-compile no callback", runs == 0);
    expect_true("before-compile did not compile", graph.livePasses().empty());
  }
  {
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle color =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                                "color");
    const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
    builder.write(writer, color, Blunder::FrameGraphUsage::ColorAttachment);
    int runs = 0;
    builder.setExecute(writer, [&](Blunder::IFrameGraphRecorder&) { ++runs; });

    const Blunder::FrameGraphCompileResult compiled = graph.compile();
    expect_true("failed-compile nosink",
                !compiled.ok &&
                    compiled.reason == Blunder::FrameGraphCompileReason::NoSink);
    const Blunder::FrameGraphExecuteResult executed = runExecute(graph);
    expect_true("after-fail not ok", !executed.ok);
    expect_true("after-fail notcompiled",
                executed.reason ==
                    Blunder::FrameGraphExecuteReason::NotCompiled);
    expect_true("after-fail no callback", runs == 0);
    expect_true("after-fail still empty live", graph.livePasses().empty());
  }
}

void testDirtySetupThenRecompileAndTwice() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(),
                              "color");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);
  int runs = 0;
  builder.setExecute(sink, [&](Blunder::IFrameGraphRecorder&) { ++runs; });

  expect_true("dirty compile ok", graph.compile().ok);
  expect_true("dirty allocate ok", allocateOk(graph));
  expect_true("dirty plan ok", planOk(graph));
  expect_true("dirty first execute ok", runExecute(graph).ok);
  expect_true("dirty first run", runs == 1);

  builder.setExecute(sink, [&](Blunder::IFrameGraphRecorder&) { ++runs; });
  const Blunder::FrameGraphExecuteResult dirty = runExecute(graph);
  expect_true("dirty execute not ok", !dirty.ok);
  expect_true("dirty reason notcompiled",
              dirty.reason == Blunder::FrameGraphExecuteReason::NotCompiled);
  expect_true("dirty no extra run", runs == 1);
  expect_true("dirty cleared live", graph.livePasses().empty());

  expect_true("dirty recompile ok", graph.compile().ok);
  expect_true("dirty realloc ok", allocateOk(graph));
  expect_true("dirty replan ok", planOk(graph));
  expect_true("dirty re-execute ok", runExecute(graph).ok);
  expect_true("dirty after recompile ran", runs == 2);
  expect_true("dirty second execute ok", runExecute(graph).ok);
  expect_true("dirty twice ran", runs == 3);

  builder.addPass("extra");
  {
    const Blunder::FrameGraphExecuteResult dirty_add = runExecute(graph);
    expect_true("addpass-dirty not ok", !dirty_add.ok);
    expect_true("addpass-dirty notcompiled",
                dirty_add.reason ==
                    Blunder::FrameGraphExecuteReason::NotCompiled);
    expect_true("addpass-dirty no extra run", runs == 3);
  }
  expect_true("addpass-dirty recompile ok", graph.compile().ok);
  builder.createTransient(Blunder::FrameGraphResourceShape::Texture, legalColorDesc(), "later");
  {
    const Blunder::FrameGraphExecuteResult dirty_create = runExecute(graph);
    expect_true("create-dirty not ok", !dirty_create.ok);
    expect_true("create-dirty notcompiled",
                dirty_create.reason ==
                    Blunder::FrameGraphExecuteReason::NotCompiled);
    expect_true("create-dirty no extra run", runs == 3);
  }
  expect_true("create-dirty recompile ok", graph.compile().ok);
  builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
  {
    const Blunder::FrameGraphExecuteResult dirty_write = runExecute(graph);
    expect_true("write-dirty not ok", !dirty_write.ok);
    expect_true("write-dirty notcompiled",
                dirty_write.reason ==
                    Blunder::FrameGraphExecuteReason::NotCompiled);
    expect_true("write-dirty no extra run", runs == 3);
  }
}

void testInvalidSetExecuteFailsCompile() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphPassHandle bad{99};
  builder.setExecute(bad, [](Blunder::IFrameGraphRecorder&) {});

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("invalid-setexecute not ok", !result.ok);
  expect_true("invalid-setexecute reason",
              result.reason == Blunder::FrameGraphCompileReason::InvalidPass);
  expect_true("invalid-setexecute empty live", graph.livePasses().empty());
}

void expectInvalidDesc(const char* label, const Blunder::FrameGraphResourceDesc& desc,
                       Blunder::FrameGraphResourceShape shape) {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  builder.createTransient(shape, desc, "bad");
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true(label, !result.ok &&
                         result.reason ==
                             Blunder::FrameGraphCompileReason::InvalidDesc &&
                         graph.livePasses().empty());
}

void testUnusedIllegalDescFails() {
  {
    Blunder::FrameGraphResourceDesc desc = legalColorDesc();
    desc.format = Blunder::FrameGraphFormat::Undefined;
    expectInvalidDesc("illegal undefined format", desc,
                      Blunder::FrameGraphResourceShape::Texture);
  }
  {
    Blunder::FrameGraphResourceDesc desc = legalColorDesc();
    desc.width = 0;
    expectInvalidDesc("illegal zero width", desc,
                      Blunder::FrameGraphResourceShape::Texture);
  }
  {
    Blunder::FrameGraphResourceDesc desc = legalColorDesc();
    desc.height = 0;
    expectInvalidDesc("illegal zero height", desc,
                      Blunder::FrameGraphResourceShape::Texture);
  }
  {
    Blunder::FrameGraphResourceDesc desc = legalColorDesc();
    desc.sample_count = 0;
    expectInvalidDesc("illegal zero samples", desc,
                      Blunder::FrameGraphResourceShape::Texture);
  }
  {
    Blunder::FrameGraphResourceDesc desc = legalColorDesc();
    desc.mip_count = 0;
    expectInvalidDesc("illegal zero mips", desc,
                      Blunder::FrameGraphResourceShape::Texture);
  }
  expectInvalidDesc("illegal zero buffer size", legalBufferDesc(0),
                    Blunder::FrameGraphResourceShape::Buffer);
  expect_true("never-created desc is default",
              descEqual(Blunder::FrameGraph().resourceDesc(
                            Blunder::FrameGraphHandle{99}),
                        Blunder::FrameGraphResourceDesc{}));
}

void testInvalidPassBeatsInvalidDesc() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphPassHandle bad{99};
  builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                             Blunder::FrameGraphResourceDesc{}, "illegal");
  builder.write(bad, Blunder::FrameGraphHandle{99},
                Blunder::FrameGraphUsage::ColorAttachment);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("pass-beats-desc not ok", !result.ok);
  expect_true("pass-beats-desc reason",
              result.reason == Blunder::FrameGraphCompileReason::InvalidPass);
  expect_true("pass-beats-desc empty live", graph.livePasses().empty());
}

void testInvalidDescBeatsDanglingAccess() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                             Blunder::FrameGraphResourceDesc{}, "illegal");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, Blunder::FrameGraphHandle{99},
                Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("desc-beats-dangling not ok", !result.ok);
  expect_true("desc-beats-dangling reason",
              result.reason == Blunder::FrameGraphCompileReason::InvalidDesc);
  expect_true("desc-beats-dangling empty live", graph.livePasses().empty());
}

void testExecuteAfterInvalidDesc() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                             Blunder::FrameGraphResourceDesc{}, "illegal");
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);
  int runs = 0;
  builder.setExecute(sink, [&](Blunder::IFrameGraphRecorder&) { ++runs; });

  const Blunder::FrameGraphCompileResult compiled = graph.compile();
  expect_true("invaliddesc compile reason",
              !compiled.ok &&
                  compiled.reason ==
                      Blunder::FrameGraphCompileReason::InvalidDesc);
  const Blunder::FrameGraphExecuteResult executed = runExecute(graph);
  expect_true("invaliddesc execute not ok", !executed.ok);
  expect_true("invaliddesc execute notcompiled",
              executed.reason == Blunder::FrameGraphExecuteReason::NotCompiled);
  expect_true("invaliddesc no callback", runs == 0);
}

void testImportedBufferResolve() {
  DummyGpuBuffer dummy;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle buf =
      builder.importExternal(legalBufferDesc(256), &dummy, "buf");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, buf, Blunder::FrameGraphUsage::Storage);
  builder.markSink(sink);

  expect_true("import-buffer pointer", graph.resolvedBuffer(buf) == &dummy);
  expect_true("import-buffer size", graph.resourceDesc(buf).size == 256);
  expect_true("import-buffer compile ok", graph.compile().ok);
  const Blunder::FrameGraphExecuteResult before_alloc = runExecute(graph);
  expect_true("import-buffer notallocated",
              !before_alloc.ok &&
                  before_alloc.reason ==
                      Blunder::FrameGraphExecuteReason::NotAllocated);
  expect_true("import-buffer allocate", allocateOk(graph));
  expect_true("import-buffer plan", planOk(graph));
  expect_true("import-buffer execute", runExecute(graph).ok);
  expect_true("import-buffer stays external",
              graph.resourceKind(buf) ==
                  Blunder::FrameGraphResourceKind::External);
}

void testUnusedNullImportFails() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  Blunder::rhi::IGpuTexture* none = nullptr;
  const Blunder::FrameGraphHandle unused =
      builder.importExternal(legalColorDesc(), none, "unused");
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);
  int runs = 0;
  builder.setExecute(sink, [&](Blunder::IFrameGraphRecorder&) { ++runs; });

  expect_true("null-import resolved null",
              graph.resolvedTexture(unused) == nullptr);
  const Blunder::FrameGraphCompileResult compiled = graph.compile();
  expect_true("null-import not ok", !compiled.ok);
  expect_true("null-import reason",
              compiled.reason == Blunder::FrameGraphCompileReason::InvalidImport);
  expect_true("null-import empty live", graph.livePasses().empty());
  const Blunder::FrameGraphExecuteResult executed = runExecute(graph);
  expect_true("null-import execute not ok", !executed.ok);
  expect_true("null-import execute notcompiled",
              executed.reason == Blunder::FrameGraphExecuteReason::NotCompiled);
  expect_true("null-import no callback", runs == 0);
}

void testUnusedNullBufferImportFails() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  Blunder::rhi::IGpuBuffer* none = nullptr;
  const Blunder::FrameGraphHandle unused =
      builder.importExternal(legalBufferDesc(256), none, "unused");
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);

  expect_true("null-buffer-import resolved null",
              graph.resolvedBuffer(unused) == nullptr);
  const Blunder::FrameGraphCompileResult compiled = graph.compile();
  expect_true("null-buffer-import not ok", !compiled.ok);
  expect_true("null-buffer-import reason",
              compiled.reason == Blunder::FrameGraphCompileReason::InvalidImport);
  expect_true("null-buffer-import empty live", graph.livePasses().empty());
}

void testImportedIllegalDescFails() {
  DummyGpuTexture dummy;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  builder.importExternal(Blunder::FrameGraphResourceDesc{}, &dummy, "bad");
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("imported-illegal-desc",
              !result.ok &&
                  result.reason == Blunder::FrameGraphCompileReason::InvalidDesc &&
                  graph.livePasses().empty());
}

void testLateNullImportDirtiesExecute() {
  DummyGpuTexture dummy;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle viewport =
      builder.importExternal(legalColorDesc(), &dummy, "viewport");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, viewport, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);
  int runs = 0;
  builder.setExecute(sink, [&](Blunder::IFrameGraphRecorder&) { ++runs; });

  expect_true("late-import compile", graph.compile().ok);
  expect_true("late-import allocate", allocateOk(graph));
  expect_true("late-import plan", planOk(graph));
  expect_true("late-import execute", runExecute(graph).ok);
  Blunder::rhi::IGpuTexture* none = nullptr;
  builder.importExternal(legalColorDesc(), none, "late-null");
  const Blunder::FrameGraphExecuteResult dirty = runExecute(graph);
  expect_true("late-null-import dirty",
              !dirty.ok &&
                  dirty.reason == Blunder::FrameGraphExecuteReason::NotCompiled &&
                  runs == 1);
}

void testLiveExternalClearThenForward() {
  DummyGpuTexture dummy;
  DummyGpuTexture unused_dummy;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle viewport =
      builder.importExternal(legalColorDesc(), &dummy, "viewport");
  const Blunder::FrameGraphHandle unused =
      builder.importExternal(legalColorDesc(), &unused_dummy, "unused");
  const Blunder::FrameGraphPassHandle clear = builder.addPass("clear");
  builder.write(clear, viewport, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle forward = builder.addPass("forward");
  builder.write(forward, viewport, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(forward);
  int next = 0;
  int order[2] = {0, 0};
  builder.setExecute(clear, [&](Blunder::IFrameGraphRecorder&) {
    expect_true("clear resolves", graph.resolvedTexture(viewport) == &dummy);
    order[next++] = 1;
  });
  builder.setExecute(forward, [&](Blunder::IFrameGraphRecorder&) {
    expect_true("forward resolves", graph.resolvedTexture(viewport) == &dummy);
    order[next++] = 2;
  });

  expect_true("live-external compile", graph.compile().ok);
  expect_true("live-external order",
              liveIndex(graph, clear) == 0 && liveIndex(graph, forward) == 1);
  expect_true("live-external unused dce", !graph.isResourceLive(unused));
  expect_true("live-external allocate", allocateOk(graph));
  expect_true("live-external plan", planOk(graph));
  expect_true("live-external execute", runExecute(graph).ok);
  expect_true("live-external ran", next == 2 && order[0] == 1 && order[1] == 2);
  expect_true("live-external kept", graph.resolvedTexture(viewport) == &dummy);
}

void testInvalidPassBeatsInvalidImport() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphPassHandle bad{99};
  builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                             Blunder::FrameGraphResourceDesc{}, "illegal");
  Blunder::rhi::IGpuTexture* none = nullptr;
  builder.importExternal(legalColorDesc(), none, "null");
  builder.write(bad, Blunder::FrameGraphHandle{99},
                Blunder::FrameGraphUsage::ColorAttachment);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("pass-beats-import not ok", !result.ok);
  expect_true("pass-beats-import reason",
              result.reason == Blunder::FrameGraphCompileReason::InvalidPass);
  expect_true("pass-beats-import empty live", graph.livePasses().empty());
}

void testInvalidDescBeatsInvalidImport() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                             Blunder::FrameGraphResourceDesc{}, "illegal");
  Blunder::rhi::IGpuTexture* none = nullptr;
  builder.importExternal(legalColorDesc(), none, "null");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, Blunder::FrameGraphHandle{99},
                Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("desc-beats-import not ok", !result.ok);
  expect_true("desc-beats-import reason",
              result.reason == Blunder::FrameGraphCompileReason::InvalidDesc);
  expect_true("desc-beats-import empty live", graph.livePasses().empty());
}

void testInvalidImportBeatsDanglingAccess() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  Blunder::rhi::IGpuTexture* none = nullptr;
  builder.importExternal(legalColorDesc(), none, "null");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, Blunder::FrameGraphHandle{99},
                Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("import-beats-dangling not ok", !result.ok);
  expect_true("import-beats-dangling reason",
              result.reason == Blunder::FrameGraphCompileReason::InvalidImport);
  expect_true("import-beats-dangling empty live", graph.livePasses().empty());
}

void testResolveMissAndDceKeepsPointer() {
  DummyGpuTexture dummy;
  DummyGpuBuffer dummy_buf;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphHandle unused =
      builder.importExternal(legalColorDesc(), &dummy, "unused");
  const Blunder::FrameGraphHandle unused_buf =
      builder.importExternal(legalBufferDesc(256), &dummy_buf, "unused_buf");
  const Blunder::FrameGraphPassHandle clear = builder.addPass("clear");
  builder.write(clear, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle forward = builder.addPass("forward");
  builder.write(forward, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(forward);
  const Blunder::FrameGraphHandle bloom =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "bloom");
  const Blunder::FrameGraphPassHandle post = builder.addPass("bloom");
  builder.write(post, bloom, Blunder::FrameGraphUsage::ColorAttachment);

  int order[4] = {0, 0, 0, 0};
  int next = 0;
  int bloom_runs = 0;
  builder.setExecute(clear, [&](Blunder::IFrameGraphRecorder&) { order[next++] = 1; });
  builder.setExecute(forward, [&](Blunder::IFrameGraphRecorder&) { order[next++] = 2; });
  builder.setExecute(post, [&](Blunder::IFrameGraphRecorder&) { ++bloom_runs; });

  expect_true("miss never-created texture",
              graph.resolvedTexture(Blunder::FrameGraphHandle{99}) == nullptr);
  expect_true("miss never-created buffer",
              graph.resolvedBuffer(Blunder::FrameGraphHandle{99}) == nullptr);
  expect_true("miss transient texture", graph.resolvedTexture(color) == nullptr);
  expect_true("miss texture as buffer", graph.resolvedBuffer(unused) == nullptr);
  expect_true("miss buffer as texture",
              graph.resolvedTexture(unused_buf) == nullptr);
  expect_true("miss empty handle",
              graph.resolvedTexture(Blunder::FrameGraphHandle{}) == nullptr);

  const Blunder::FrameGraphCompileResult compiled = graph.compile();
  expect_true("resolve-dce compile ok", compiled.ok);
  expect_true("dce unused external not live", !graph.isResourceLive(unused));
  expect_true("dce unused pointer kept", graph.resolvedTexture(unused) == &dummy);
  expect_true("dce unused buffer pointer kept",
              graph.resolvedBuffer(unused_buf) == &dummy_buf);
  expect_true("resolve-dce allocate ok", allocateOk(graph));
  expect_true("resolve-dce live texture",
              graph.resolvedTexture(color) != nullptr);
  expect_true("resolve-dce plan ok", planOk(graph));
  const Blunder::FrameGraphExecuteResult executed = runExecute(graph);
  expect_true("resolve-dce execute ok", executed.ok);
  expect_true("resolve-dce live order ran",
              next == 2 && order[0] == 1 && order[1] == 2);
  expect_true("resolve-dce culled never ran", bloom_runs == 0);
  expect_true("miss dummy buffer never imported as texture",
              graph.resolvedBuffer(color) == nullptr);
}

void testLiveTransientTextureAllocate() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);

  DummyAllocator allocator;
  expect_true("live-tex compile", graph.compile().ok);
  expect_true("live-tex unresolved before allocate",
              graph.resolvedTexture(color) == nullptr);
  const Blunder::FrameGraphAllocateResult allocated = graph.allocate(allocator);
  expect_true("live-tex allocate ok",
              allocated.ok &&
                  allocated.reason == Blunder::FrameGraphAllocateReason::Ok);
  expect_true("live-tex one create", allocator.texture_creates == 1);
  expect_true("live-tex desc forwarded",
              descEqual(allocator.last_texture_desc, legalColorDesc()));
  expect_true("live-tex resolve", graph.resolvedTexture(color) != nullptr);
  expect_true("live-tex stays transient",
              graph.resourceKind(color) ==
                  Blunder::FrameGraphResourceKind::Transient);
}

void testLiveTransientBufferAllocate() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle buf =
      builder.createTransient(Blunder::FrameGraphResourceShape::Buffer,
                              legalBufferDesc(256), "buf");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, buf, Blunder::FrameGraphUsage::Storage);
  builder.markSink(sink);

  DummyAllocator allocator;
  expect_true("live-buf compile", graph.compile().ok);
  expect_true("live-buf unresolved before allocate",
              graph.resolvedBuffer(buf) == nullptr);
  expect_true("live-buf allocate", graph.allocate(allocator).ok);
  expect_true("live-buf one create", allocator.buffer_creates == 1);
  expect_true("live-buf desc forwarded",
              allocator.last_buffer_desc.size == 256);
  expect_true("live-buf resolve", graph.resolvedBuffer(buf) != nullptr);
  expect_true("live-buf size 256", graph.resourceDesc(buf).size == 256);
}

void testAllocateNotCompiledAndNotAllocatedAndAllocFailed() {
  {
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle color =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                                legalColorDesc(), "color");
    const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
    builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
    builder.markSink(sink);
    DummyAllocator allocator;
    const Blunder::FrameGraphAllocateResult allocated = graph.allocate(allocator);
    expect_true("alloc-before-compile not ok", !allocated.ok);
    expect_true("alloc-before-compile notcompiled",
                allocated.reason ==
                    Blunder::FrameGraphAllocateReason::NotCompiled);
    expect_true("alloc-before-compile no create",
                allocator.texture_creates == 0 && allocator.buffer_creates == 0);
  }
  {
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle color =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                                legalColorDesc(), "color");
    const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
    builder.write(writer, color, Blunder::FrameGraphUsage::ColorAttachment);
    DummyAllocator allocator;
    expect_true("alloc-after-fail nosink", !graph.compile().ok);
    const Blunder::FrameGraphAllocateResult allocated = graph.allocate(allocator);
    expect_true("alloc-after-fail notcompiled",
                !allocated.ok &&
                    allocated.reason ==
                        Blunder::FrameGraphAllocateReason::NotCompiled);
    expect_true("alloc-after-fail no create", allocator.texture_creates == 0);
  }
  {
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle color =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                                legalColorDesc(), "color");
    const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
    builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
    builder.markSink(sink);
    int runs = 0;
    builder.setExecute(sink, [&](Blunder::IFrameGraphRecorder&) { ++runs; });
    expect_true("notalloc compile", graph.compile().ok);
    const Blunder::FrameGraphExecuteResult executed = runExecute(graph);
    expect_true("notalloc execute",
                !executed.ok &&
                    executed.reason ==
                        Blunder::FrameGraphExecuteReason::NotAllocated);
    expect_true("notalloc no callback", runs == 0);
  }
  {
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle a =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                                legalColorDesc(), "a");
    const Blunder::FrameGraphHandle b =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                                legalColorDesc(), "b");
    const Blunder::FrameGraphPassHandle sink_a = builder.addPass("sink-a");
    builder.write(sink_a, a, Blunder::FrameGraphUsage::ColorAttachment);
    builder.markSink(sink_a);
    const Blunder::FrameGraphPassHandle sink_b = builder.addPass("sink-b");
    builder.write(sink_b, b, Blunder::FrameGraphUsage::ColorAttachment);
    builder.markSink(sink_b);
    int runs = 0;
    builder.setExecute(sink_a, [&](Blunder::IFrameGraphRecorder&) { ++runs; });
    builder.setExecute(sink_b, [&](Blunder::IFrameGraphRecorder&) { ++runs; });
    DummyAllocator allocator;
    allocator.fail_texture_at = 2;
    expect_true("allocfail compile", graph.compile().ok);
    const Blunder::FrameGraphAllocateResult allocated = graph.allocate(allocator);
    expect_true("allocfail not ok", !allocated.ok);
    expect_true("allocfail reason",
                allocated.reason == Blunder::FrameGraphAllocateReason::AllocFailed);
    expect_true("allocfail two creates", allocator.texture_creates == 2);
    expect_true("allocfail both null",
                graph.resolvedTexture(a) == nullptr &&
                    graph.resolvedTexture(b) == nullptr);
    const Blunder::FrameGraphExecuteResult executed = runExecute(graph);
    expect_true("allocfail execute notallocated",
                !executed.ok &&
                    executed.reason ==
                        Blunder::FrameGraphExecuteReason::NotAllocated &&
                    runs == 0);
  }
}

void testDceTransientNotCreatedExternalUntouched() {
  DummyGpuTexture dummy;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle bloom =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "bloom");
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphHandle viewport =
      builder.importExternal(legalColorDesc(), &dummy, "viewport");
  const Blunder::FrameGraphPassHandle post = builder.addPass("bloom");
  builder.write(post, bloom, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.write(sink, viewport, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);

  DummyAllocator allocator;
  expect_true("dce-alloc compile", graph.compile().ok);
  expect_true("dce-alloc bloom dead", !graph.isResourceLive(bloom));
  expect_true("dce-alloc color live", graph.isResourceLive(color));
  expect_true("dce-alloc allocate", graph.allocate(allocator).ok);
  expect_true("dce-alloc one texture create", allocator.texture_creates == 1);
  expect_true("dce-alloc no buffer create", allocator.buffer_creates == 0);
  expect_true("dce-alloc bloom null", graph.resolvedTexture(bloom) == nullptr);
  expect_true("dce-alloc color owned", graph.resolvedTexture(color) != nullptr);
  expect_true("dce-alloc external kept",
              graph.resolvedTexture(viewport) == &dummy);
}

void testStickyAllocateAndSetupDirtyClearForward() {
  DummyGpuTexture dummy;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphHandle viewport =
      builder.importExternal(legalColorDesc(), &dummy, "viewport");
  const Blunder::FrameGraphPassHandle clear = builder.addPass("clear");
  builder.write(clear, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle forward = builder.addPass("forward");
  builder.write(forward, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.write(forward, viewport, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(forward);
  int order[4] = {0, 0, 0, 0};
  int next = 0;
  builder.setExecute(clear, [&](Blunder::IFrameGraphRecorder&) { order[next++] = 1; });
  builder.setExecute(forward, [&](Blunder::IFrameGraphRecorder&) { order[next++] = 2; });

  DummyAllocator allocator;
  expect_true("sticky compile", graph.compile().ok);
  expect_true("sticky first allocate", graph.allocate(allocator).ok);
  expect_true("sticky first create", allocator.texture_creates == 1);
  Blunder::rhi::IGpuTexture* first = graph.resolvedTexture(color);
  expect_true("sticky first pointer", first != nullptr);
  expect_true("sticky second allocate", graph.allocate(allocator).ok);
  expect_true("sticky create unchanged", allocator.texture_creates == 1);
  expect_true("sticky same pointer", graph.resolvedTexture(color) == first);
  expect_true("sticky external kept",
              graph.resolvedTexture(viewport) == &dummy);

  builder.setExecute(clear, [&](Blunder::IFrameGraphRecorder&) { order[next++] = 1; });
  expect_true("sticky-dirty transient null",
              graph.resolvedTexture(color) == nullptr);
  expect_true("sticky-dirty external kept",
              graph.resolvedTexture(viewport) == &dummy);
  DummyAllocator after_dirty;
  const Blunder::FrameGraphAllocateResult dirty_alloc =
      graph.allocate(after_dirty);
  expect_true("sticky-dirty allocate notcompiled",
              !dirty_alloc.ok &&
                  dirty_alloc.reason ==
                      Blunder::FrameGraphAllocateReason::NotCompiled &&
                  after_dirty.texture_creates == 0 &&
                  after_dirty.buffer_creates == 0);
  const Blunder::FrameGraphExecuteResult dirty = runExecute(graph);
  expect_true("sticky-dirty execute notcompiled",
              !dirty.ok &&
                  dirty.reason == Blunder::FrameGraphExecuteReason::NotCompiled &&
                  next == 0);

  expect_true("sticky recompile", graph.compile().ok);
  DummyAllocator realloc;
  expect_true("sticky realloc", graph.allocate(realloc).ok);
  expect_true("sticky realloc create", realloc.texture_creates == 1);
  expect_true("sticky realloc resolve",
              graph.resolvedTexture(color) != nullptr);
  expect_true("sticky plan", planOk(graph));
  expect_true("sticky execute", runExecute(graph).ok);
  expect_true("sticky clear then forward",
              next == 2 && order[0] == 1 && order[1] == 2);
}

void testTwoLiveTransientsAllocate() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle a =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "a");
  const Blunder::FrameGraphHandle b =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "b");
  const Blunder::FrameGraphPassHandle sink_a = builder.addPass("sink-a");
  builder.write(sink_a, a, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink_a);
  const Blunder::FrameGraphPassHandle sink_b = builder.addPass("sink-b");
  builder.write(sink_b, b, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink_b);
  DummyAllocator allocator;
  expect_true("two-live compile", graph.compile().ok);
  expect_true("two-live allocate", graph.allocate(allocator).ok);
  expect_true("two-live creates", allocator.texture_creates == 2);
  expect_true("two-live distinct",
              graph.resolvedTexture(a) != nullptr &&
                  graph.resolvedTexture(b) != nullptr &&
                  graph.resolvedTexture(a) != graph.resolvedTexture(b));
}

void testAllocFailedBufferRollback() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle a =
      builder.createTransient(Blunder::FrameGraphResourceShape::Buffer,
                              legalBufferDesc(256), "a");
  const Blunder::FrameGraphHandle b =
      builder.createTransient(Blunder::FrameGraphResourceShape::Buffer,
                              legalBufferDesc(256), "b");
  const Blunder::FrameGraphPassHandle sink_a = builder.addPass("sa");
  builder.write(sink_a, a, Blunder::FrameGraphUsage::Storage);
  builder.markSink(sink_a);
  const Blunder::FrameGraphPassHandle sink_b = builder.addPass("sb");
  builder.write(sink_b, b, Blunder::FrameGraphUsage::Storage);
  builder.markSink(sink_b);
  DummyAllocator allocator;
  allocator.fail_buffer_at = 2;
  expect_true("buf-allocfail compile", graph.compile().ok);
  const Blunder::FrameGraphAllocateResult allocated = graph.allocate(allocator);
  expect_true("buf-allocfail reason",
              !allocated.ok &&
                  allocated.reason ==
                      Blunder::FrameGraphAllocateReason::AllocFailed);
  expect_true("buf-allocfail both null",
              graph.resolvedBuffer(a) == nullptr &&
                  graph.resolvedBuffer(b) == nullptr);
  expect_true("buf-allocfail execute",
              runExecute(graph).reason ==
                  Blunder::FrameGraphExecuteReason::NotAllocated);
}

bool stateWriteColor(const Blunder::FrameGraphResourceState& state) {
  return !state.undefined &&
         state.access == Blunder::FrameGraphAccessKind::Write &&
         state.usage == Blunder::FrameGraphUsage::ColorAttachment;
}

bool stateReadSampled(const Blunder::FrameGraphResourceState& state) {
  return !state.undefined &&
         state.access == Blunder::FrameGraphAccessKind::Read &&
         state.usage == Blunder::FrameGraphUsage::Sampled;
}

bool stateWriteStorage(const Blunder::FrameGraphResourceState& state) {
  return !state.undefined &&
         state.access == Blunder::FrameGraphAccessKind::Write &&
         state.usage == Blunder::FrameGraphUsage::Storage;
}

bool stateReadStorage(const Blunder::FrameGraphResourceState& state) {
  return !state.undefined &&
         state.access == Blunder::FrameGraphAccessKind::Read &&
         state.usage == Blunder::FrameGraphUsage::Storage;
}

bool barrierEqual(const Blunder::FrameGraphBarrier& a,
                  const Blunder::FrameGraphBarrier& b) {
  return a.resource.index == b.resource.index &&
         a.from.undefined == b.from.undefined && a.from.access == b.from.access &&
         a.from.usage == b.from.usage && a.to.undefined == b.to.undefined &&
         a.to.access == b.to.access && a.to.usage == b.to.usage &&
         a.after.index == b.after.index && a.before.index == b.before.index;
}

void testClearForwardColorWawPlan() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle clear = builder.addPass("clear");
  builder.write(clear, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle forward = builder.addPass("forward");
  builder.write(forward, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(forward);

  expect_true("waw compile", graph.compile().ok);
  expect_true("waw barriers empty before plan", graph.barriers().empty());
  const Blunder::FrameGraphPlanBarriersResult planned = graph.planBarriers();
  expect_true("waw plan ok",
              planned.ok &&
                  planned.reason == Blunder::FrameGraphPlanBarriersReason::Ok);

  bool found_waw = false;
  int first_use_at = -1;
  int waw_at = -1;
  const std::vector<Blunder::FrameGraphBarrier>& rows = graph.barriers();
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    const Blunder::FrameGraphBarrier& row = rows[static_cast<std::size_t>(i)];
    if (row.resource.index != color.index) {
      continue;
    }
    if (row.from.undefined && stateWriteColor(row.to) && !row.after.valid() &&
        row.before.index == clear.index) {
      first_use_at = i;
    }
    if (stateWriteColor(row.from) && stateWriteColor(row.to) &&
        row.after.index == clear.index && row.before.index == forward.index) {
      found_waw = true;
      waw_at = i;
    }
  }
  expect_true("waw row present", found_waw);
  expect_true("waw sort first-use then WAW",
              first_use_at >= 0 && waw_at >= 0 && first_use_at < waw_at);
}

void testTransientFirstUseExternalNoUndefinedDceSilent() {
  DummyGpuTexture dummy;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphHandle dead =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "dead");
  const Blunder::FrameGraphHandle viewport =
      builder.importExternal(legalColorDesc(), &dummy, "viewport");
  const Blunder::FrameGraphPassHandle clear = builder.addPass("clear");
  builder.write(clear, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle forward = builder.addPass("forward");
  builder.write(forward, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.write(forward, viewport, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(forward);
  const Blunder::FrameGraphPassHandle bloom = builder.addPass("bloom");
  builder.write(bloom, dead, Blunder::FrameGraphUsage::ColorAttachment);

  expect_true("first-use compile", graph.compile().ok);
  expect_true("first-use plan", graph.planBarriers().ok);
  expect_true("first-use dead not live", !graph.isResourceLive(dead));

  bool found_first = false;
  bool external_undefined = false;
  bool dead_row = false;
  for (const Blunder::FrameGraphBarrier& row : graph.barriers()) {
    if (row.resource.index == color.index && row.from.undefined &&
        stateWriteColor(row.to) && !row.after.valid() &&
        row.before.index == clear.index) {
      found_first = true;
    }
    if (row.resource.index == viewport.index && row.from.undefined) {
      external_undefined = true;
    }
    if (row.resource.index == dead.index) {
      dead_row = true;
    }
  }
  expect_true("first-use undefined row", found_first);
  expect_true("external no undefined row", !external_undefined);
  expect_true("dce transient no rows", !dead_row);
}

void testColorThenSampledRawSkipReadRead() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
  builder.write(writer, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle reader_a = builder.addPass("read-a");
  builder.read(reader_a, color, Blunder::FrameGraphUsage::Sampled);
  builder.markSink(reader_a);
  const Blunder::FrameGraphPassHandle reader_b = builder.addPass("read-b");
  builder.read(reader_b, color, Blunder::FrameGraphUsage::Sampled);
  builder.markSink(reader_b);

  expect_true("raw compile", graph.compile().ok);
  expect_true("raw live writer then readers",
              liveIndex(graph, writer) == 0 && liveIndex(graph, reader_a) >= 0 &&
                  liveIndex(graph, reader_b) >= 0 &&
                  liveIndex(graph, writer) < liveIndex(graph, reader_a) &&
                  liveIndex(graph, writer) < liveIndex(graph, reader_b));
  expect_true("raw plan", graph.planBarriers().ok);

  const int a_live = liveIndex(graph, reader_a);
  const int b_live = liveIndex(graph, reader_b);
  const Blunder::FrameGraphPassHandle first_reader =
      a_live < b_live ? reader_a : reader_b;
  const Blunder::FrameGraphPassHandle second_reader =
      a_live < b_live ? reader_b : reader_a;

  bool found_raw = false;
  bool found_read_read = false;
  for (const Blunder::FrameGraphBarrier& row : graph.barriers()) {
    if (row.resource.index != color.index) {
      continue;
    }
    if (stateWriteColor(row.from) && stateReadSampled(row.to) &&
        row.after.index == writer.index &&
        row.before.index == first_reader.index) {
      found_raw = true;
    }
    if (stateReadSampled(row.from) && stateReadSampled(row.to) &&
        row.after.index == first_reader.index &&
        row.before.index == second_reader.index) {
      found_read_read = true;
    }
  }
  expect_true("raw color to sampled", found_raw);
  expect_true("no sampled read-read", !found_read_read);
}

void testTransientBufferStorageRaw() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle buf =
      builder.createTransient(Blunder::FrameGraphResourceShape::Buffer,
                              legalBufferDesc(256), "buf");
  const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
  builder.write(writer, buf, Blunder::FrameGraphUsage::Storage);
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.read(sink, buf, Blunder::FrameGraphUsage::Storage);
  builder.markSink(sink);

  expect_true("buf-raw compile", graph.compile().ok);
  expect_true("buf-raw plan", graph.planBarriers().ok);
  expect_true("buf-raw desc size", graph.resourceDesc(buf).size == 256);

  bool found_raw = false;
  for (const Blunder::FrameGraphBarrier& row : graph.barriers()) {
    if (row.resource.index == buf.index && stateWriteStorage(row.from) &&
        stateReadStorage(row.to) && row.after.index == writer.index &&
        row.before.index == sink.index) {
      found_raw = true;
    }
  }
  expect_true("buf-raw storage row", found_raw);
}

void testSamePassLeavingIsLastAccess() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle pingpong = builder.addPass("pingpong");
  builder.write(pingpong, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.read(pingpong, color, Blunder::FrameGraphUsage::Sampled);
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);

  expect_true("same-pass compile", graph.compile().ok);
  expect_true("same-pass plan", graph.planBarriers().ok);

  bool found_intra = false;
  bool found_leave_read = false;
  bool found_leave_write = false;
  for (const Blunder::FrameGraphBarrier& row : graph.barriers()) {
    if (row.resource.index != color.index) {
      continue;
    }
    if (row.after.index == pingpong.index && row.before.index == pingpong.index) {
      found_intra = true;
    }
    if (stateReadSampled(row.from) && stateWriteColor(row.to) &&
        row.after.index == pingpong.index && row.before.index == sink.index) {
      found_leave_read = true;
    }
    if (stateWriteColor(row.from) && stateWriteColor(row.to) &&
        row.after.index == pingpong.index && row.before.index == sink.index) {
      found_leave_write = true;
    }
  }
  expect_true("same-pass no intra row", !found_intra);
  expect_true("same-pass leave is last Read", found_leave_read);
  expect_true("same-pass leave is not first Write", !found_leave_write);
}

void testEmptyPlanExternalSinkOk() {
  DummyGpuTexture dummy;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle viewport =
      builder.importExternal(legalColorDesc(), &dummy, "viewport");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("forward");
  builder.write(sink, viewport, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);

  expect_true("empty-plan compile", graph.compile().ok);
  const Blunder::FrameGraphPlanBarriersResult planned = graph.planBarriers();
  expect_true("empty-plan ok",
              planned.ok &&
                  planned.reason == Blunder::FrameGraphPlanBarriersReason::Ok);
  expect_true("empty-plan empty list", graph.barriers().empty());
}

void testPlanBarriersNotCompiledStickyExecuteWithoutPlan() {
  {
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle color =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                                legalColorDesc(), "color");
    const Blunder::FrameGraphPassHandle clear = builder.addPass("clear");
    builder.write(clear, color, Blunder::FrameGraphUsage::ColorAttachment);
    const Blunder::FrameGraphPassHandle forward = builder.addPass("forward");
    builder.write(forward, color, Blunder::FrameGraphUsage::ColorAttachment);
    builder.markSink(forward);
    const Blunder::FrameGraphPlanBarriersResult planned = graph.planBarriers();
    expect_true("plan valid graph not compiled",
                !planned.ok &&
                    planned.reason ==
                        Blunder::FrameGraphPlanBarriersReason::NotCompiled);
    expect_true("plan valid graph empty", graph.barriers().empty());
    expect_true("plan valid graph live empty", graph.livePasses().empty());
  }

  {
    Blunder::FrameGraph graph;
    const Blunder::FrameGraphPlanBarriersResult planned = graph.planBarriers();
    expect_true("plan never compiled",
                !planned.ok &&
                    planned.reason ==
                        Blunder::FrameGraphPlanBarriersReason::NotCompiled);
    expect_true("plan never compiled empty", graph.barriers().empty());
  }

  {
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    Blunder::FrameGraphResourceDesc bad;
    builder.createTransient(Blunder::FrameGraphResourceShape::Texture, bad,
                            "bad");
    const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
    builder.markSink(sink);
    expect_true("plan after invaliddesc compile",
                graph.compile().reason ==
                    Blunder::FrameGraphCompileReason::InvalidDesc);
    const Blunder::FrameGraphPlanBarriersResult planned = graph.planBarriers();
    expect_true("plan after fail notcompiled",
                !planned.ok &&
                    planned.reason ==
                        Blunder::FrameGraphPlanBarriersReason::NotCompiled);
    expect_true("plan after fail empty", graph.barriers().empty());
  }

  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle clear = builder.addPass("clear");
  builder.write(clear, color, Blunder::FrameGraphUsage::ColorAttachment);
  int ran = 0;
  builder.setExecute(clear, [&ran](Blunder::IFrameGraphRecorder&) { ++ran; });
  const Blunder::FrameGraphPassHandle forward = builder.addPass("forward");
  builder.write(forward, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(forward);
  builder.setExecute(forward, [&ran](Blunder::IFrameGraphRecorder&) { ++ran; });

  expect_true("sticky compile", graph.compile().ok);
  expect_true("sticky allocate", allocateOk(graph));
  DummyRecorder no_plan;
  const Blunder::FrameGraphExecuteResult without_plan = graph.execute(no_plan);
  expect_true("execute without plan not ok", !without_plan.ok);
  expect_true("execute without plan NotPlanned",
              without_plan.reason ==
                  Blunder::FrameGraphExecuteReason::NotPlanned);
  expect_true("execute without plan empty dummy", no_plan.barriers.empty());
  expect_true("execute without plan no callback", ran == 0);

  const Blunder::FrameGraphPlanBarriersResult first = graph.planBarriers();
  expect_true("sticky first plan", first.ok);
  const std::vector<Blunder::FrameGraphBarrier> snapshot = graph.barriers();
  expect_true("sticky snapshot nonempty", !snapshot.empty());
  const Blunder::FrameGraphPlanBarriersResult second = graph.planBarriers();
  expect_true("sticky second plan", second.ok);
  expect_true("sticky same size", graph.barriers().size() == snapshot.size());
  bool same = graph.barriers().size() == snapshot.size();
  for (std::size_t i = 0; i < snapshot.size(); ++i) {
    if (!barrierEqual(graph.barriers()[i], snapshot[i])) {
      same = false;
    }
  }
  expect_true("sticky list unchanged", same);

  expect_true("sticky recompile", graph.compile().ok);
  expect_true("sticky recompile size", graph.barriers().size() == snapshot.size());
  bool after_recompile = graph.barriers().size() == snapshot.size();
  for (std::size_t i = 0; i < snapshot.size(); ++i) {
    if (!barrierEqual(graph.barriers()[i], snapshot[i])) {
      after_recompile = false;
    }
  }
  expect_true("sticky recompile list unchanged", after_recompile);
  expect_true("sticky recompile plan", graph.planBarriers().ok);

  builder.setExecute(forward, [&ran](Blunder::IFrameGraphRecorder&) { ran += 10; });
  expect_true("setup clears barriers", graph.barriers().empty());
  expect_true("setup plan notcompiled",
              graph.planBarriers().reason ==
                  Blunder::FrameGraphPlanBarriersReason::NotCompiled);
  expect_true("setup recompile", graph.compile().ok);
  expect_true("setup reallocate", allocateOk(graph));
  expect_true("setup replan", graph.planBarriers().ok);
  expect_true("setup restored size",
              graph.barriers().size() == snapshot.size());
  bool restored = graph.barriers().size() == snapshot.size();
  for (std::size_t i = 0; i < snapshot.size(); ++i) {
    if (i >= graph.barriers().size() ||
        !barrierEqual(graph.barriers()[i], snapshot[i])) {
      restored = false;
    }
  }
  expect_true("setup restored table", restored);
}

bool isFirstUseWriteColor(const Blunder::FrameGraphBarrier& row,
                          Blunder::FrameGraphHandle color,
                          Blunder::FrameGraphPassHandle clear) {
  return row.resource.index == color.index && row.from.undefined &&
         stateWriteColor(row.to) && !row.after.valid() &&
         row.before.index == clear.index;
}

bool isWawWriteColor(const Blunder::FrameGraphBarrier& row,
                     Blunder::FrameGraphHandle color,
                     Blunder::FrameGraphPassHandle clear,
                     Blunder::FrameGraphPassHandle forward) {
  return row.resource.index == color.index && stateWriteColor(row.from) &&
         stateWriteColor(row.to) && row.after.index == clear.index &&
         row.before.index == forward.index;
}

void testClearForwardExecuteRecordsFirstUseThenWaw() {
  DummyRecorder dummy;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle clear = builder.addPass("clear");
  builder.write(clear, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle forward = builder.addPass("forward");
  builder.write(forward, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(forward);
  builder.setExecute(clear, [&](Blunder::IFrameGraphRecorder& rec) {
    expect_true("exec-waw clear same recorder", &rec == &dummy);
    dummy.noteCallback(1);
  });
  builder.setExecute(forward, [&](Blunder::IFrameGraphRecorder& rec) {
    expect_true("exec-waw forward same recorder", &rec == &dummy);
    dummy.noteCallback(2);
  });

  expect_true("exec-waw compile", graph.compile().ok);
  expect_true("exec-waw allocate", allocateOk(graph));
  expect_true("exec-waw plan", planOk(graph));
  expect_true("exec-waw execute", graph.execute(dummy).ok);
  expect_true("exec-waw four events", dummy.events.size() == 4);
  expect_true("exec-waw first-use then clear",
              dummy.events.size() >= 4 &&
                  dummy.events[0].kind == DummyRecorder::Barrier &&
                  isFirstUseWriteColor(dummy.events[0].barrier, color, clear) &&
                  dummy.events[1].kind == DummyRecorder::Callback &&
                  dummy.events[1].callback_id == 1);
  expect_true("exec-waw waw then forward",
              dummy.events.size() >= 4 &&
                  dummy.events[2].kind == DummyRecorder::Barrier &&
                  isWawWriteColor(dummy.events[2].barrier, color, clear,
                                  forward) &&
                  dummy.events[3].kind == DummyRecorder::Callback &&
                  dummy.events[3].callback_id == 2);
}

void testEmptyPlanExternalExecute() {
  DummyGpuTexture tex;
  DummyRecorder dummy;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle viewport =
      builder.importExternal(legalColorDesc(), &tex, "viewport");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, viewport, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);
  int runs = 0;
  builder.setExecute(sink, [&](Blunder::IFrameGraphRecorder&) { ++runs; });

  expect_true("empty-exec compile", graph.compile().ok);
  expect_true("empty-exec allocate", allocateOk(graph));
  expect_true("empty-exec plan", planOk(graph));
  expect_true("empty-exec plan empty", graph.barriers().empty());
  expect_true("empty-exec execute", graph.execute(dummy).ok);
  expect_true("empty-exec zero barrier", dummy.barriers.empty());
  expect_true("empty-exec sink ran", runs == 1);
}

void testExecuteRawBeforeReaderSkipReadReadAndBuffer() {
  {
    DummyRecorder dummy;
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle color =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                                legalColorDesc(), "color");
    const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
    builder.write(writer, color, Blunder::FrameGraphUsage::ColorAttachment);
    const Blunder::FrameGraphPassHandle reader_a = builder.addPass("read-a");
    builder.read(reader_a, color, Blunder::FrameGraphUsage::Sampled);
    builder.markSink(reader_a);
    const Blunder::FrameGraphPassHandle reader_b = builder.addPass("read-b");
    builder.read(reader_b, color, Blunder::FrameGraphUsage::Sampled);
    builder.markSink(reader_b);
    builder.setExecute(writer, [&](Blunder::IFrameGraphRecorder&) {
      dummy.noteCallback(1);
    });
    builder.setExecute(reader_a, [&](Blunder::IFrameGraphRecorder&) {
      dummy.noteCallback(2);
    });
    builder.setExecute(reader_b, [&](Blunder::IFrameGraphRecorder&) {
      dummy.noteCallback(3);
    });

    expect_true("exec-raw compile", graph.compile().ok);
    expect_true("exec-raw allocate", allocateOk(graph));
    expect_true("exec-raw plan", planOk(graph));
    expect_true("exec-raw execute", graph.execute(dummy).ok);

    bool raw_before_first_reader = false;
    bool barrier_between_readers = false;
    int last_callback = 0;
    for (const DummyRecorder::Event& event : dummy.events) {
      if (event.kind == DummyRecorder::Callback) {
        last_callback = event.callback_id;
        continue;
      }
      if (event.barrier.resource.index != color.index) {
        continue;
      }
      if (stateWriteColor(event.barrier.from) &&
          stateReadSampled(event.barrier.to) && last_callback == 1) {
        raw_before_first_reader = true;
      }
      if (stateReadSampled(event.barrier.from) &&
          stateReadSampled(event.barrier.to) &&
          (last_callback == 2 || last_callback == 3)) {
        barrier_between_readers = true;
      }
    }
    expect_true("exec-raw before first reader", raw_before_first_reader);
    expect_true("exec-raw no read-read", !barrier_between_readers);
  }
  {
    DummyRecorder dummy;
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle buf =
        builder.createTransient(Blunder::FrameGraphResourceShape::Buffer,
                                legalBufferDesc(256), "buf");
    const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
    builder.write(writer, buf, Blunder::FrameGraphUsage::Storage);
    const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
    builder.read(sink, buf, Blunder::FrameGraphUsage::Storage);
    builder.markSink(sink);
    builder.setExecute(writer, [&](Blunder::IFrameGraphRecorder&) {
      dummy.noteCallback(1);
    });
    builder.setExecute(sink, [&](Blunder::IFrameGraphRecorder&) {
      dummy.noteCallback(2);
    });
    expect_true("exec-buf compile", graph.compile().ok);
    expect_true("exec-buf allocate", allocateOk(graph));
    expect_true("exec-buf plan", planOk(graph));
    expect_true("exec-buf execute", graph.execute(dummy).ok);
    bool raw_before_reader = false;
    int last_callback = 0;
    for (const DummyRecorder::Event& event : dummy.events) {
      if (event.kind == DummyRecorder::Callback) {
        last_callback = event.callback_id;
        continue;
      }
      if (event.barrier.resource.index == buf.index &&
          stateWriteStorage(event.barrier.from) &&
          stateReadStorage(event.barrier.to) && last_callback == 1) {
        raw_before_reader = true;
      }
    }
    expect_true("exec-buf raw before reader", raw_before_reader);
  }
}

void testGpuExecuteFailurePriorityStickyDirty() {
  {
    DummyRecorder dummy;
    Blunder::FrameGraph graph;
    const Blunder::FrameGraphExecuteResult executed = graph.execute(dummy);
    expect_true("exec-never notcompiled",
                !executed.ok &&
                    executed.reason ==
                        Blunder::FrameGraphExecuteReason::NotCompiled);
    expect_true("exec-never empty dummy", dummy.barriers.empty());
  }
  {
    DummyRecorder dummy;
    Blunder::FrameGraph graph;
    Blunder::GraphBuilder builder(graph);
    const Blunder::FrameGraphHandle color =
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                                legalColorDesc(), "color");
    const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
    builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
    builder.markSink(sink);
    int runs = 0;
    builder.setExecute(sink, [&](Blunder::IFrameGraphRecorder&) { ++runs; });
    expect_true("exec-noalloc compile", graph.compile().ok);
    const Blunder::FrameGraphExecuteResult executed = graph.execute(dummy);
    expect_true("exec-noalloc NotAllocated",
                !executed.ok &&
                    executed.reason ==
                        Blunder::FrameGraphExecuteReason::NotAllocated);
    expect_true("exec-noalloc empty dummy", dummy.barriers.empty());
    expect_true("exec-noalloc no callback", runs == 0);
  }
  DummyRecorder first;
  DummyRecorder second;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle clear = builder.addPass("clear");
  builder.write(clear, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle forward = builder.addPass("forward");
  builder.write(forward, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(forward);
  builder.setExecute(clear, [&](Blunder::IFrameGraphRecorder&) {});
  builder.setExecute(forward, [&](Blunder::IFrameGraphRecorder&) {});
  expect_true("exec-sticky compile", graph.compile().ok);
  expect_true("exec-sticky allocate", allocateOk(graph));
  expect_true("exec-sticky plan", planOk(graph));
  expect_true("exec-sticky first", graph.execute(first).ok);
  expect_true("exec-sticky second", graph.execute(second).ok);
  expect_true("exec-sticky same barrier count",
              first.barriers.size() == second.barriers.size());
  bool same = first.barriers.size() == second.barriers.size();
  for (std::size_t i = 0; i < first.barriers.size(); ++i) {
    if (i >= second.barriers.size() ||
        !barrierEqual(first.barriers[i], second.barriers[i])) {
      same = false;
    }
  }
  expect_true("exec-sticky same sequence", same);

  builder.setExecute(forward, [&](Blunder::IFrameGraphRecorder&) {});
  DummyRecorder dirty;
  const Blunder::FrameGraphExecuteResult after_setup = graph.execute(dirty);
  expect_true("exec-dirty notcompiled",
              !after_setup.ok &&
                  after_setup.reason ==
                      Blunder::FrameGraphExecuteReason::NotCompiled);
  expect_true("exec-dirty empty dummy", dirty.barriers.empty());
}

void testExecuteNotAllocatedBeatsNotPlanned() {
  DummyRecorder dummy;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.write(sink, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);
  int runs = 0;
  builder.setExecute(sink, [&](Blunder::IFrameGraphRecorder&) { ++runs; });
  expect_true("alloc-beats-plan compile", graph.compile().ok);
  expect_true("alloc-beats-plan plan", planOk(graph));
  const Blunder::FrameGraphExecuteResult executed = graph.execute(dummy);
  expect_true("alloc-beats-plan NotAllocated",
              !executed.ok &&
                  executed.reason ==
                      Blunder::FrameGraphExecuteReason::NotAllocated);
  expect_true("alloc-beats-plan empty dummy", dummy.events.empty());
  expect_true("alloc-beats-plan no callback", runs == 0);
}

void testExecuteSnapshotSurvivesInCallbackMutation() {
  DummyRecorder dummy;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              legalColorDesc(), "color");
  const Blunder::FrameGraphPassHandle clear = builder.addPass("clear");
  builder.write(clear, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle forward = builder.addPass("forward");
  builder.write(forward, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(forward);
  int forward_runs = 0;
  builder.setExecute(forward, [&](Blunder::IFrameGraphRecorder&) {
    ++forward_runs;
    dummy.noteCallback(2);
  });
  builder.setExecute(clear, [&](Blunder::IFrameGraphRecorder&) {
    dummy.noteCallback(1);
    builder.setExecute(forward, [&](Blunder::IFrameGraphRecorder&) {
      forward_runs += 100;
    });
  });
  expect_true("snap compile", graph.compile().ok);
  expect_true("snap allocate", allocateOk(graph));
  expect_true("snap plan", planOk(graph));
  expect_true("snap execute ok", graph.execute(dummy).ok);
  expect_true("snap forward ran once", forward_runs == 1);
  expect_true("snap callbacks still ordered",
              dummy.events.size() >= 2 &&
                  dummy.events[1].kind == DummyRecorder::Callback &&
                  dummy.events[1].callback_id == 1);
  bool saw_forward = false;
  for (const DummyRecorder::Event& event : dummy.events) {
    if (event.kind == DummyRecorder::Callback && event.callback_id == 2) {
      saw_forward = true;
    }
  }
  expect_true("snap forward callback from copy", saw_forward);
  DummyRecorder after;
  const Blunder::FrameGraphExecuteResult dirty = graph.execute(after);
  expect_true("snap next NotCompiled",
              !dirty.ok &&
                  dirty.reason == Blunder::FrameGraphExecuteReason::NotCompiled);
  expect_true("snap next empty dummy", after.events.empty());
}

void testOverlayHandshakeNoColorAttachmentBeforeOverlay() {
  DummyGpuTexture dummy;
  DummyRecorder rec;
  DummyAllocator allocator;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.importExternal(legalColorDesc(), &dummy, "viewport.color");
  const Blunder::FrameGraphPassHandle scene = builder.addPass("scene");
  builder.write(scene, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.read(scene, color, Blunder::FrameGraphUsage::Sampled);
  const Blunder::FrameGraphPassHandle overlay = builder.addPass("overlay");
  builder.read(overlay, color, Blunder::FrameGraphUsage::Sampled);
  builder.write(overlay, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.read(overlay, color, Blunder::FrameGraphUsage::Sampled);
  const Blunder::FrameGraphPassHandle copy = builder.addPass("copy");
  builder.read(copy, color, Blunder::FrameGraphUsage::Sampled);
  builder.markSink(copy);
  builder.setExecute(scene, [&](Blunder::IFrameGraphRecorder&) {
    rec.noteCallback(1);
  });
  builder.setExecute(overlay, [&](Blunder::IFrameGraphRecorder&) {
    rec.noteCallback(2);
  });
  builder.setExecute(copy, [&](Blunder::IFrameGraphRecorder&) {
    rec.noteCallback(3);
  });
  expect_true("handshake compile", graph.compile().ok);
  expect_true("handshake allocate none",
              graph.allocate(allocator).ok && allocator.texture_creates == 0);
  expect_true("handshake plan", planOk(graph));
  expect_true("handshake execute", graph.execute(rec).ok);
  expect_true("handshake live scene overlay copy",
              graph.livePasses().size() == 3 &&
                  graph.livePasses()[0].index == scene.index &&
                  graph.livePasses()[1].index == overlay.index &&
                  graph.livePasses()[2].index == copy.index);
  bool saw_overlay = false;
  bool color_attachment_before_overlay = false;
  for (const DummyRecorder::Event& event : rec.events) {
    if (event.kind == DummyRecorder::Barrier &&
        event.barrier.before.index == overlay.index &&
        event.barrier.to.usage == Blunder::FrameGraphUsage::ColorAttachment) {
      color_attachment_before_overlay = true;
    }
    if (event.kind == DummyRecorder::Callback && event.callback_id == 2) {
      saw_overlay = true;
    }
  }
  expect_true("handshake overlay ran", saw_overlay);
  expect_true("handshake no ColorAttachment before overlay",
              !color_attachment_before_overlay);
}

void testOmittedOverlayNotLiveCopyIsSink() {
  DummyGpuTexture dummy;
  DummyAllocator allocator;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.importExternal(legalColorDesc(), &dummy, "viewport.color");
  const Blunder::FrameGraphPassHandle scene = builder.addPass("scene");
  builder.write(scene, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.read(scene, color, Blunder::FrameGraphUsage::Sampled);
  const Blunder::FrameGraphPassHandle copy = builder.addPass("copy");
  builder.read(copy, color, Blunder::FrameGraphUsage::Sampled);
  builder.markSink(copy);
  expect_true("omit compile", graph.compile().ok);
  expect_true("omit allocate none",
              graph.allocate(allocator).ok && allocator.texture_creates == 0);
  expect_true("omit live scene then copy",
              graph.livePasses().size() == 2 &&
                  graph.livePasses()[0].index == scene.index &&
                  graph.livePasses()[1].index == copy.index);
}

void testUnreachableOverlayCallbackDoesNotRun() {
  DummyGpuTexture dummy;
  DummyRecorder rec;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.importExternal(legalColorDesc(), &dummy, "viewport.color");
  const Blunder::FrameGraphPassHandle scene = builder.addPass("scene");
  builder.write(scene, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.read(scene, color, Blunder::FrameGraphUsage::Sampled);
  const Blunder::FrameGraphPassHandle overlay = builder.addPass("overlay");
  const Blunder::FrameGraphPassHandle copy = builder.addPass("copy");
  builder.read(copy, color, Blunder::FrameGraphUsage::Sampled);
  builder.markSink(copy);
  int overlay_runs = 0;
  builder.setExecute(overlay, [&](Blunder::IFrameGraphRecorder&) {
    ++overlay_runs;
    rec.noteCallback(99);
  });
  builder.setExecute(copy, [&](Blunder::IFrameGraphRecorder&) {
    rec.noteCallback(3);
  });
  expect_true("dce overlay compile", graph.compile().ok);
  expect_true("dce overlay allocate", allocateOk(graph));
  expect_true("dce overlay plan", planOk(graph));
  expect_true("dce overlay execute", graph.execute(rec).ok);
  expect_true("dce overlay not live", liveIndex(graph, overlay) == -1);
  expect_true("dce overlay callback skipped", overlay_runs == 0);
}

void testDeferredDepthHandshakeNoDepthAttachmentToSampledBeforeLighting() {
  DummyGpuTexture color_tex;
  DummyGpuTexture depth_tex;
  DummyGpuTexture shadow_tex;
  DummyRecorder rec;
  DummyAllocator allocator;
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.importExternal(legalColorDesc(), &color_tex, "viewport.color");
  const Blunder::FrameGraphHandle depth =
      builder.importExternal(legalDepthDesc(), &depth_tex, "viewport.depth");
  const Blunder::FrameGraphHandle shadow =
      builder.importExternal(legalDepthDesc(), &shadow_tex, "shadow.map");
  const Blunder::FrameGraphPassHandle gbuffer = builder.addPass("viewport.gbuffer");
  builder.write(gbuffer, depth, Blunder::FrameGraphUsage::DepthAttachment);
  builder.read(gbuffer, depth, Blunder::FrameGraphUsage::Sampled);
  const Blunder::FrameGraphPassHandle lighting =
      builder.addPass("viewport.lighting");
  builder.read(lighting, depth, Blunder::FrameGraphUsage::Sampled);
  builder.read(lighting, shadow, Blunder::FrameGraphUsage::Sampled);
  builder.write(lighting, color, Blunder::FrameGraphUsage::ColorAttachment);
  builder.read(lighting, color, Blunder::FrameGraphUsage::Sampled);
  const Blunder::FrameGraphPassHandle copy = builder.addPass("viewport.copy");
  builder.read(copy, color, Blunder::FrameGraphUsage::Sampled);
  builder.markSink(copy);
  builder.setExecute(gbuffer, [&](Blunder::IFrameGraphRecorder&) {
    rec.noteCallback(1);
  });
  builder.setExecute(lighting, [&](Blunder::IFrameGraphRecorder&) {
    rec.noteCallback(2);
  });
  builder.setExecute(copy, [&](Blunder::IFrameGraphRecorder&) {
    rec.noteCallback(3);
  });
  expect_true("deferred handshake compile", graph.compile().ok);
  expect_true("deferred handshake allocate none",
              graph.allocate(allocator).ok && allocator.texture_creates == 0);
  expect_true("deferred handshake plan", planOk(graph));
  expect_true("deferred handshake execute", graph.execute(rec).ok);
  expect_true("deferred live gbuffer lighting copy",
              graph.livePasses().size() == 3 &&
                  graph.livePasses()[0].index == gbuffer.index &&
                  graph.livePasses()[1].index == lighting.index &&
                  graph.livePasses()[2].index == copy.index);
  bool saw_lighting = false;
  bool depth_attachment_to_sampled_before_lighting = false;
  for (const DummyRecorder::Event& event : rec.events) {
    if (event.kind == DummyRecorder::Barrier &&
        event.barrier.before.index == lighting.index &&
        event.barrier.from.usage == Blunder::FrameGraphUsage::DepthAttachment &&
        event.barrier.to.usage == Blunder::FrameGraphUsage::Sampled) {
      depth_attachment_to_sampled_before_lighting = true;
    }
    if (event.kind == DummyRecorder::Callback && event.callback_id == 2) {
      saw_lighting = true;
    }
  }
  expect_true("deferred lighting ran", saw_lighting);
  expect_true("deferred no DepthAttachment to Sampled before lighting",
              !depth_attachment_to_sampled_before_lighting);
}

}  // namespace

int main() {
  testTwoPassTransientThenSink();
  testUnreachablePostProcessDropped();
  testImportedExternalStaysExternal();
  testBufferWriteThenRead();
  testPingPongIsNotCycle();
  testUnreachableCycleIsNotFailure();
  testDanglingAccessFails();
  testDanglingWriteFails();
  testEmptyGraphFails();
  testZeroSinksFails();
  testColorAttachmentWriteChain();
  testWarReaderBeforeLaterWrite();
  testTwoReadersBeforeWrite();
  testSamePassAccessIsNotCycle();
  testInvalidPassFails();
  testFailureReasonPriority();
  testDanglingBeatsNoSink();
  testLiveCallbacksRunInOrderAndDceSkipped();
  testLivePassNoCallbackIsEmptyRun();
  testExecuteNotCompiled();
  testDirtySetupThenRecompileAndTwice();
  testInvalidSetExecuteFailsCompile();
  testUnusedIllegalDescFails();
  testInvalidPassBeatsInvalidDesc();
  testInvalidDescBeatsDanglingAccess();
  testExecuteAfterInvalidDesc();
  testImportedBufferResolve();
  testUnusedNullImportFails();
  testUnusedNullBufferImportFails();
  testImportedIllegalDescFails();
  testLateNullImportDirtiesExecute();
  testLiveExternalClearThenForward();
  testInvalidPassBeatsInvalidImport();
  testInvalidDescBeatsInvalidImport();
  testInvalidImportBeatsDanglingAccess();
  testResolveMissAndDceKeepsPointer();
  testLiveTransientTextureAllocate();
  testLiveTransientBufferAllocate();
  testAllocateNotCompiledAndNotAllocatedAndAllocFailed();
  testDceTransientNotCreatedExternalUntouched();
  testStickyAllocateAndSetupDirtyClearForward();
  testTwoLiveTransientsAllocate();
  testAllocFailedBufferRollback();
  testClearForwardColorWawPlan();
  testTransientFirstUseExternalNoUndefinedDceSilent();
  testColorThenSampledRawSkipReadRead();
  testTransientBufferStorageRaw();
  testSamePassLeavingIsLastAccess();
  testEmptyPlanExternalSinkOk();
  testPlanBarriersNotCompiledStickyExecuteWithoutPlan();
  testClearForwardExecuteRecordsFirstUseThenWaw();
  testEmptyPlanExternalExecute();
  testExecuteRawBeforeReaderSkipReadReadAndBuffer();
  testGpuExecuteFailurePriorityStickyDirty();
  testExecuteNotAllocatedBeatsNotPlanned();
  testExecuteSnapshotSurvivesInCallbackMutation();
  testOverlayHandshakeNoColorAttachmentBeforeOverlay();
  testOmittedOverlayNotLiveCopyIsSink();
  testUnreachableOverlayCallbackDoesNotRun();
  testDeferredDepthHandshakeNoDepthAttachmentToSampledBeforeLighting();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
