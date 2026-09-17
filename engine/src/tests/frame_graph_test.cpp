#include "runtime/function/render/frame_graph/frame_graph.h"

#include <cstddef>
#include <cstdio>

namespace {

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

void testTwoPassTransientThenSink() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle color =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              "color");
  const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
  builder.write(writer, color, Blunder::FrameGraphUsage::ColorAttachment);
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.read(sink, color, Blunder::FrameGraphUsage::Sampled);
  builder.markSink(sink);

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
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              "color");
  const Blunder::FrameGraphHandle bloom =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
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
}

void testImportedExternalStaysExternal() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle viewport =
      builder.importExternal(Blunder::FrameGraphResourceShape::Texture,
                             "viewport");
  const Blunder::FrameGraphPassHandle sink = builder.addPass("forward");
  builder.write(sink, viewport, Blunder::FrameGraphUsage::ColorAttachment);
  builder.markSink(sink);

  const Blunder::FrameGraphCompileResult result = graph.compile();
  expect_true("external compile ok", result.ok);
  expect_true("external stays external",
              graph.resourceKind(viewport) ==
                  Blunder::FrameGraphResourceKind::External);
  expect_true("external live", graph.isResourceLive(viewport));
  expect_true("external not transient",
              graph.resourceKind(viewport) !=
                  Blunder::FrameGraphResourceKind::Transient);
}

void testBufferWriteThenRead() {
  Blunder::FrameGraph graph;
  Blunder::GraphBuilder builder(graph);
  const Blunder::FrameGraphHandle buf =
      builder.createTransient(Blunder::FrameGraphResourceShape::Buffer,
                              "buf");
  const Blunder::FrameGraphPassHandle writer = builder.addPass("write");
  builder.write(writer, buf, Blunder::FrameGraphUsage::Storage);
  const Blunder::FrameGraphPassHandle sink = builder.addPass("sink");
  builder.read(sink, buf, Blunder::FrameGraphUsage::Storage);
  builder.markSink(sink);

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
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, "a");
  const Blunder::FrameGraphHandle b =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, "b");
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
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
                              "color");
  const Blunder::FrameGraphHandle a =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, "a");
  const Blunder::FrameGraphHandle b =
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture, "b");
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
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
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
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
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
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
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
      builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
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
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
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
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
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
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
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
        builder.createTransient(Blunder::FrameGraphResourceShape::Texture,
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
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
