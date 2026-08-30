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
  expect_true("cmd frame",
              parsePlayIpcCommandLine("frame") == PlayIpcCommand::Frame);
  expect_true("cmd trim",
              parsePlayIpcCommandLine("  pause\r") == PlayIpcCommand::Pause);
  expect_true("cmd unknown stays unknown",
              parsePlayIpcCommandLine("nope") == PlayIpcCommand::Unknown);
  expect_true("cmd step without n unknown",
              parsePlayIpcCommandLine("step") == PlayIpcCommand::Unknown);
  expect_true("cmd reload",
              parsePlayIpcCommandLine("reload") == PlayIpcCommand::Reload);
  expect_true("cmd patch without json unknown",
              parsePlayIpcCommandLine("patch") == PlayIpcCommand::Unknown);
  expect_true("cmd patch empty object unknown? brace required",
              parsePlayIpcCommandLine("patch x") == PlayIpcCommand::Unknown);
  {
    const PlayIpcHostCommand step = parsePlayIpcHostCommandLine("step 30");
    expect_true("cmd step", step.command == PlayIpcCommand::Step);
    expect_true("cmd step ticks", step.step_ticks == 30);
  }
  {
    const PlayIpcHostCommand patch = parsePlayIpcHostCommandLine(
        "patch {\"address\":\"Hero\"}");
    expect_true("cmd patch", patch.command == PlayIpcCommand::Patch);
    expect_true("cmd patch json", patch.patch_json == "{\"address\":\"Hero\"}");
  }

  {
    PlayIpcLogRecord rec;
    rec.sev = "warning";
    rec.text = "hello \"play\"";
    rec.stack = "a\nb";
    rec.ms = 1700000000123;
    PlayIpcLogRecord parsed;
    expect_true("ndjson roundtrip parse",
                parsePlayIpcLogLine(formatPlayIpcLogLine(rec), parsed));
    expect_true("ndjson sev", parsed.sev == "warning");
    expect_true("ndjson text", parsed.text == rec.text);
    expect_true("ndjson stack", parsed.stack == "a\nb");
    expect_true("ndjson ms", parsed.ms == rec.ms);
    expect_true("bare pause is not a log",
                !parsePlayIpcLogLine("pause", parsed));
  }

  {
    PlayIpcFrameRecord rec;
    rec.width = 16;
    rec.height = 9;
    rec.encoding = "rgba8";
    rec.rgba.assign(16u * 9u * 4u, 7);
    PlayIpcFrameRecord parsed;
    expect_true("frame ndjson parse",
                parsePlayIpcFrameLine(formatPlayIpcFrameLine(rec), parsed));
    expect_true("frame width", parsed.width == 16);
    expect_true("frame height", parsed.height == 9);
    expect_true("frame encoding", parsed.encoding == "rgba8");
    expect_true("frame rgba", parsed.rgba == rec.rgba);
    expect_true("frame is 16:9", parsed.width != parsed.height);
    expect_true("log line is not a frame",
                !parsePlayIpcFrameLine(formatPlayIpcLogLine(PlayIpcLogRecord{}),
                                      parsed));
  }

  {
    PlayIpcIssueRecord rec;
    rec.sev = "warning";
    rec.code = "play.patch_unknown_address";
    rec.address = "Hero";
    PlayIpcIssueRecord parsed;
    expect_true("issue ndjson parse",
                parsePlayIpcIssueLine(formatPlayIpcIssueLine(rec), parsed));
    expect_true("issue sev", parsed.sev == "warning");
    expect_true("issue code", parsed.code == rec.code);
    expect_true("issue address", parsed.address == "Hero");
    PlayIpcErrorRecord as_error;
    expect_true("issue is not error kind",
                !parsePlayIpcErrorLine(formatPlayIpcIssueLine(rec), as_error));
  }

  {
    PlayIpcReloadRecord parsed;
    expect_true("reload ok parse",
                parsePlayIpcReloadLine(formatPlayIpcReloadLine(true), parsed));
    expect_true("reload ok true", parsed.ok);
    expect_true("reload fail parse",
                parsePlayIpcReloadLine(formatPlayIpcReloadLine(false), parsed));
    expect_true("reload ok false", !parsed.ok);
    expect_true("issue is not reload",
                !parsePlayIpcReloadLine(formatPlayIpcIssueLine(PlayIpcIssueRecord{}),
                                       parsed));
  }

  {
    PlayIpcPosesRecord rec;
    PlayIpcPoseEntity hero;
    hero.name = "Hero";
    hero.t[0] = 1.f;
    hero.t[1] = 2.f;
    hero.t[2] = 3.f;
    hero.r[3] = 1.f;
    hero.s[0] = 1.f;
    hero.s[1] = 1.f;
    hero.s[2] = 1.f;
    rec.entities.push_back(hero);
    PlayIpcPosesRecord parsed;
    expect_true("poses ndjson parse",
                parsePlayIpcPosesLine(formatPlayIpcPosesLine(rec), parsed));
    expect_true("poses count", parsed.entities.size() == 1);
    if (!parsed.entities.empty()) {
      expect_true("poses name", parsed.entities[0].name == "Hero");
      expect_true("poses t0", parsed.entities[0].t[0] == 1.f);
      expect_true("poses t1", parsed.entities[0].t[1] == 2.f);
      expect_true("poses t2", parsed.entities[0].t[2] == 3.f);
    }
    expect_true("empty poses parse",
                parsePlayIpcPosesLine(formatPlayIpcPosesLine(PlayIpcPosesRecord{}),
                                     parsed) &&
                    parsed.entities.empty());
  }

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

  std::vector<PlayIpcHostCommand> received;
  PlayIpcClient agent;
  expect_true("agent connect",
              agent.connect("127.0.0.1", host.boundPort()));
  agent.setCommandHandler(
      [&](PlayIpcHostCommand cmd) { received.push_back(cmd); });
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
  expect_true("send step", host.sendStep(12));
  expect_true("send frame request", host.sendFrameRequest());
  expect_true("send reload", host.sendReload());
  expect_true("send patch", host.sendPatch("{\"address\":\"Hero\"}"));
  expect_true("sendCommand rejects patch",
              !host.sendCommand(PlayIpcCommand::Patch));
  expect_true("unknown line ignored by send? host still ready",
              host.isPeerReady());

  const auto cmd_deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (received.size() < 7 &&
         std::chrono::steady_clock::now() < cmd_deadline) {
    agent.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  expect_true("received 7 commands", received.size() == 7);
  if (received.size() >= 7) {
    expect_true("got pause", received[0].command == PlayIpcCommand::Pause);
    expect_true("got resume", received[1].command == PlayIpcCommand::Resume);
    expect_true("got stop", received[2].command == PlayIpcCommand::Stop);
    expect_true("got step", received[3].command == PlayIpcCommand::Step);
    expect_true("got step n", received[3].step_ticks == 12);
    expect_true("got frame", received[4].command == PlayIpcCommand::Frame);
    expect_true("got reload", received[5].command == PlayIpcCommand::Reload);
    expect_true("got patch", received[6].command == PlayIpcCommand::Patch);
    expect_true("got patch json",
                received[6].patch_json == "{\"address\":\"Hero\"}");
  }

  expect_true("send unknown line ignored", host.sendLine("bogus-verb"));
  agent.poll();
  expect_true("unknown not dispatched", received.size() == 7);

  agent.close();
  host.close();

  // Log after ready is ingested; log before ready is discarded.
  {
    PlayIpcServer log_host;
    expect_true("log host listen", log_host.listen(0));
    PlayIpcClient log_agent;
    expect_true("log agent connect",
                log_agent.connect("127.0.0.1", log_host.boundPort()));

    PlayIpcLogRecord early;
    early.sev = "log";
    early.text = "before-ready";
    early.ms = 1;
    expect_true("send pre-ready noise",
                log_agent.sendRawLine(formatPlayIpcLogLine(early)));

    expect_true("log announce ready", log_agent.announceReady());
    const auto ready_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    bool log_ready = false;
    while (std::chrono::steady_clock::now() < ready_deadline) {
      if (log_host.waitPeerReady(50)) {
        log_ready = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    expect_true("log host ready", log_ready);
    expect_true("no pre-ready log ingested", log_host.pollLogs().empty());

    PlayIpcLogRecord sent;
    sent.sev = "warning";
    sent.text = "hello-play";
    sent.stack = "stack\nline";
    sent.ms = 12345;
    expect_true("send log after ready", log_agent.sendLog(sent));

    const auto ingest_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    std::vector<PlayIpcLogRecord> ingested;
    while (ingested.empty() &&
           std::chrono::steady_clock::now() < ingest_deadline) {
      ingested = log_host.pollLogs();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    expect_true("ingested one log", ingested.size() == 1);
    if (!ingested.empty()) {
      expect_true("log sev warning", ingested[0].sev == "warning");
      expect_true("log text", ingested[0].text == "hello-play");
      expect_true("log stack", ingested[0].stack == "stack\nline");
      expect_true("log ms", ingested[0].ms == 12345);
    }

    expect_true("send pause after log",
                log_host.sendCommand(PlayIpcCommand::Pause));
    std::vector<PlayIpcHostCommand> after_log;
    log_agent.setCommandHandler(
        [&](PlayIpcHostCommand cmd) { after_log.push_back(cmd); });
    const auto pause_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (after_log.empty() &&
           std::chrono::steady_clock::now() < pause_deadline) {
      log_agent.poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    expect_true("pause after log", !after_log.empty() &&
                                       after_log[0].command == PlayIpcCommand::Pause);

    PlayIpcFrameRecord sent_frame;
    sent_frame.width = 16;
    sent_frame.height = 9;
    sent_frame.rgba.assign(16u * 9u * 4u, 9);
    expect_true("send frame after ready", log_agent.sendFrame(sent_frame));
    const auto frame_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    std::vector<PlayIpcFrameRecord> frames;
    while (frames.empty() &&
           std::chrono::steady_clock::now() < frame_deadline) {
      frames = log_host.pollFrames();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    expect_true("ingested one frame", frames.size() == 1);
    if (!frames.empty()) {
      expect_true("frame 16:9", frames[0].width == 16 && frames[0].height == 9);
      expect_true("frame pixels", frames[0].rgba == sent_frame.rgba);
    }

    PlayIpcIssueRecord sent_issue;
    sent_issue.sev = "warning";
    sent_issue.code = "play.patch_unknown_address";
    sent_issue.address = "Missing";
    expect_true("send issue after ready", log_agent.sendIssue(sent_issue));
    const auto issue_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    std::vector<PlayIpcIssueRecord> issues;
    while (issues.empty() &&
           std::chrono::steady_clock::now() < issue_deadline) {
      issues = log_host.pollIssues();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    expect_true("ingested one issue", issues.size() == 1);
    if (!issues.empty()) {
      expect_true("issue code wire",
                  issues[0].code == "play.patch_unknown_address");
      expect_true("issue not error poll", log_host.pollErrors().empty());
    }

    expect_true("send reload ack", log_agent.sendReloadAck(true));
    const auto reload_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    std::vector<PlayIpcReloadRecord> reloads;
    while (reloads.empty() &&
           std::chrono::steady_clock::now() < reload_deadline) {
      reloads = log_host.pollReloads();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    expect_true("ingested one reload ack", reloads.size() == 1 && reloads[0].ok);

    PlayIpcPosesRecord sent_poses;
    PlayIpcPoseEntity pose;
    pose.name = "Hero";
    pose.t[0] = 4.f;
    sent_poses.entities.push_back(pose);
    expect_true("send poses after ready", log_agent.sendPoses(sent_poses));
    const auto poses_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    std::vector<PlayIpcPosesRecord> poses;
    while (poses.empty() &&
           std::chrono::steady_clock::now() < poses_deadline) {
      poses = log_host.pollPoses();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    expect_true("ingested one poses", poses.size() == 1);
    if (!poses.empty() && !poses[0].entities.empty()) {
      expect_true("poses name wire", poses[0].entities[0].name == "Hero");
      expect_true("poses t wire", poses[0].entities[0].t[0] == 4.f);
    }

    log_agent.close();
    log_host.close();
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("play_ipc_test: all passed\n");
  return 0;
}
