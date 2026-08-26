#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Blunder {

enum class PlayIpcCommand : uint8_t {
  Pause = 0,
  Resume,
  Stop,
  Step,
  Frame,
  Unknown,
};

struct PlayIpcHostCommand {
  PlayIpcCommand command{PlayIpcCommand::Unknown};
  uint32_t step_ticks{0};
};

struct PlayIpcEndpoint {
  bool ok{false};
  std::string host{"127.0.0.1"};
  uint16_t port{0};
  std::string error;
};

/// Player → editor Console Message on the Play control channel (NDJSON).
/// Independent of ConsoleRing so `play_ipc_test` stays a small TU.
struct PlayIpcLogRecord {
  std::string sev;
  std::string text;
  std::string stack;
  int64_t ms{0};
};

/// Player → editor Play frame on the same control channel (NDJSON).
struct PlayIpcFrameRecord {
  uint32_t width{0};
  uint32_t height{0};
  std::string encoding{"rgba8"};
  std::vector<uint8_t> rgba;
};

struct PlayIpcErrorRecord {
  std::string code;
};

/// True for IPv4 loopback addresses (`127.0.0.0/8`).
bool isPlayIpcLoopbackHost(const std::string& host);

/// Parses `host:port` (e.g. `127.0.0.1:54321`). Host defaults to 127.0.0.1 if
/// only a port is given. Non-loopback hosts are rejected.
PlayIpcEndpoint parsePlayIpcEndpoint(const std::string& endpoint);
std::string formatPlayIpcEndpoint(const PlayIpcEndpoint& endpoint);
PlayIpcHostCommand parsePlayIpcHostCommandLine(const std::string& line);
PlayIpcCommand parsePlayIpcCommandLine(const std::string& line);
const char* playIpcCommandName(PlayIpcCommand command);

bool parsePlayIpcLogLine(const std::string& line, PlayIpcLogRecord& out);
std::string formatPlayIpcLogLine(const PlayIpcLogRecord& record);

bool parsePlayIpcFrameLine(const std::string& line, PlayIpcFrameRecord& out);
std::string formatPlayIpcFrameLine(const PlayIpcFrameRecord& record);

bool parsePlayIpcErrorLine(const std::string& line, PlayIpcErrorRecord& out);
std::string formatPlayIpcErrorLine(const std::string& code);

/// Editor-side control host: binds and **holds** the listen socket (eliminates
/// ephemeral-port TOCTOU), accepts the Player, waits for `ready`, then sends
/// `pause` / `resume` / `stop` / `step N` / `frame`.
class PlayIpcServer {
 public:
  PlayIpcServer();
  ~PlayIpcServer();

  PlayIpcServer(const PlayIpcServer&) = delete;
  PlayIpcServer& operator=(const PlayIpcServer&) = delete;

  /// Bind @p endpoint (port 0 = ephemeral). Host must be IPv4 loopback.
  bool listen(const PlayIpcEndpoint& endpoint);
  /// Convenience: listen on 127.0.0.1:@p port (0 = ephemeral).
  bool listen(uint16_t port = 0);

  uint16_t boundPort() const { return m_bound_port; }
  bool isListening() const;
  bool isPeerReady() const { return m_ready; }

  /// Accept the Player connection (if needed) and wait for a `ready` line.
  /// @p timeout_ms 0 = non-blocking single attempt.
  bool waitPeerReady(int timeout_ms = 2000);

  bool sendCommand(PlayIpcCommand command);
  bool sendStep(uint32_t ticks);
  bool sendFrameRequest();
  bool sendLine(const std::string& line);

  /// After `ready`: recv leftover + new bytes, parse NDJSON log records.
  std::vector<PlayIpcLogRecord> pollLogs();
  std::vector<PlayIpcFrameRecord> pollFrames();
  std::vector<PlayIpcErrorRecord> pollErrors();

  void close();

 private:
  bool tryAccept();
  bool tryReadReady();
  void drainInbound();

  std::uintptr_t m_listen_fd{0};
  std::uintptr_t m_client_fd{0};
  uint16_t m_bound_port{0};
  std::string m_recv_buffer;
  bool m_ready{false};
  std::vector<PlayIpcLogRecord> m_pending_logs;
  std::vector<PlayIpcFrameRecord> m_pending_frames;
  std::vector<PlayIpcErrorRecord> m_pending_errors;
};

/// Player-side control agent: connects to the editor host, announces `ready`,
/// then receives pause/resume/stop/step/frame commands.
class PlayIpcClient {
 public:
  PlayIpcClient();
  ~PlayIpcClient();

  PlayIpcClient(const PlayIpcClient&) = delete;
  PlayIpcClient& operator=(const PlayIpcClient&) = delete;

  bool connect(const PlayIpcEndpoint& endpoint);
  bool connect(const std::string& host, uint16_t port);

  /// Send the `ready` line once after connect (and after Player init).
  bool announceReady();

  /// Send a raw line before or after ready (adds trailing newline if missing).
  bool sendRawLine(const std::string& line);

  /// After `ready`: send a raw line (adds trailing newline if missing).
  bool sendLine(const std::string& line);

  /// After `ready`: send one NDJSON Console log record.
  bool sendLog(const PlayIpcLogRecord& record);

  /// After `ready`: send one NDJSON Play frame.
  bool sendFrame(const PlayIpcFrameRecord& record);

  /// After `ready`: send one NDJSON request-failure record.
  bool sendError(const std::string& code);

  void setCommandHandler(std::function<void(PlayIpcHostCommand)> handler);

  /// Non-blocking read / dispatch of host commands. Call from Player frame loop.
  void poll();

  bool isConnected() const;
  void close();

 private:
  void processHostBuffer();

  std::function<void(PlayIpcHostCommand)> m_handler;
  std::uintptr_t m_fd{0};
  std::string m_recv_buffer;
  bool m_ready_sent{false};
};

}  // namespace Blunder
