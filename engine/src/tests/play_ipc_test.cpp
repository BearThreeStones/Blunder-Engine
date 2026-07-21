#include "runtime/project/play_ipc.h"

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    const PlayIpcEndpoint ep = parsePlayIpcEndpoint("127.0.0.1:54321");
    expect_true("parse host:port ok", ep.ok);
    expect_true("parse host", ep.host == "127.0.0.1");
    expect_true("parse port", ep.port == 54321);
    expect_true("format roundtrip",
                formatPlayIpcEndpoint(ep) == "127.0.0.1:54321");
  }

  {
    const PlayIpcEndpoint ep = parsePlayIpcEndpoint("9");
    expect_true("parse port-only ok", ep.ok);
    expect_true("parse port-only host", ep.host == "127.0.0.1");
    expect_true("parse port-only port", ep.port == 9);
  }

  expect_true("cmd pause",
              parsePlayIpcCommandLine("pause") == PlayIpcCommand::Pause);
  expect_true("cmd resume",
              parsePlayIpcCommandLine("resume") == PlayIpcCommand::Resume);
  expect_true("cmd stop",
              parsePlayIpcCommandLine("stop") == PlayIpcCommand::Stop);
  expect_true("cmd trim",
              parsePlayIpcCommandLine("  pause\r") == PlayIpcCommand::Pause);

  PlayIpcServer server;
  expect_true("listen ephemeral", server.listen(0));
  expect_true("listening", server.isListening());
  expect_true("bound port nonzero", server.boundPort() != 0);

  std::vector<PlayIpcCommand> received;
  server.setCommandHandler(
      [&](PlayIpcCommand cmd) { received.push_back(cmd); });

  PlayIpcClient client;
  expect_true("client connect",
              client.connect("127.0.0.1", server.boundPort()));

  // Drive accept + ready handshake from the server side.
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  bool ready = false;
  while (std::chrono::steady_clock::now() < deadline) {
    server.poll();
    if (client.waitReady(50)) {
      ready = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  expect_true("ready handshake", ready);

  expect_true("send pause", client.sendCommand(PlayIpcCommand::Pause));
  expect_true("send resume", client.sendCommand(PlayIpcCommand::Resume));
  expect_true("send stop", client.sendCommand(PlayIpcCommand::Stop));

  const auto cmd_deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (received.size() < 3 &&
         std::chrono::steady_clock::now() < cmd_deadline) {
    server.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  expect_true("received 3 commands", received.size() == 3);
  if (received.size() >= 3) {
    expect_true("got pause", received[0] == PlayIpcCommand::Pause);
    expect_true("got resume", received[1] == PlayIpcCommand::Resume);
    expect_true("got stop", received[2] == PlayIpcCommand::Stop);
  }

  client.close();
  server.close();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("play_ipc_test: all passed\n");
  return 0;
}
