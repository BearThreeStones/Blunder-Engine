#include "runtime/project/play_ipc.h"

#include <chrono>
#include <cstdio>
#include <string>
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

  {
    const PlayIpcEndpoint ep = parsePlayIpcEndpoint("0.0.0.0:9");
    expect_true("reject non-loopback parse", !ep.ok);
    expect_true("reject non-loopback error",
                ep.error.find("loopback") != std::string::npos);
  }

  expect_true("loopback 127.0.0.1", isPlayIpcLoopbackHost("127.0.0.1"));
  expect_true("loopback 127.1.2.3", isPlayIpcLoopbackHost("127.1.2.3"));
  expect_true("reject 0.0.0.0", !isPlayIpcLoopbackHost("0.0.0.0"));
  expect_true("reject 8.8.8.8", !isPlayIpcLoopbackHost("8.8.8.8"));

  expect_true("cmd pause",
              parsePlayIpcCommandLine("pause") == PlayIpcCommand::Pause);
  expect_true("cmd resume",
              parsePlayIpcCommandLine("resume") == PlayIpcCommand::Resume);
  expect_true("cmd stop",
              parsePlayIpcCommandLine("stop") == PlayIpcCommand::Stop);
  expect_true("cmd trim",
              parsePlayIpcCommandLine("  pause\r") == PlayIpcCommand::Pause);

  PlayIpcServer host;
  expect_true("listen ephemeral", host.listen(0));
  expect_true("listening", host.isListening());
  expect_true("bound port nonzero", host.boundPort() != 0);

  {
    PlayIpcEndpoint bad;
    bad.ok = true;
    bad.host = "0.0.0.0";
    bad.port = 0;
    PlayIpcServer reject;
    expect_true("listen rejects non-loopback", !reject.listen(bad));
  }

  std::vector<PlayIpcCommand> received;
  PlayIpcClient agent;
  expect_true("agent connect",
              agent.connect("127.0.0.1", host.boundPort()));
  agent.setCommandHandler(
      [&](PlayIpcCommand cmd) { received.push_back(cmd); });
  expect_true("agent announce ready", agent.announceReady());

  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  bool ready = false;
  while (std::chrono::steady_clock::now() < deadline) {
    if (host.waitPeerReady(50)) {
      ready = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  expect_true("host saw ready", ready);
  expect_true("peer ready flag", host.isPeerReady());

  expect_true("send pause", host.sendCommand(PlayIpcCommand::Pause));
  expect_true("send resume", host.sendCommand(PlayIpcCommand::Resume));
  expect_true("send stop", host.sendCommand(PlayIpcCommand::Stop));

  const auto cmd_deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (received.size() < 3 &&
         std::chrono::steady_clock::now() < cmd_deadline) {
    agent.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  expect_true("received 3 commands", received.size() == 3);
  if (received.size() >= 3) {
    expect_true("got pause", received[0] == PlayIpcCommand::Pause);
    expect_true("got resume", received[1] == PlayIpcCommand::Resume);
    expect_true("got stop", received[2] == PlayIpcCommand::Stop);
  }

  agent.close();
  host.close();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("play_ipc_test: all passed\n");
  return 0;
}
