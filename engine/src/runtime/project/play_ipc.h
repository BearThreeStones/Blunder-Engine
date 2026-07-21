#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace Blunder {

enum class PlayIpcCommand : uint8_t {
  Pause = 0,
  Resume,
  Stop,
  Unknown,
};

struct PlayIpcEndpoint {
  bool ok{false};
  std::string host{"127.0.0.1"};
  uint16_t port{0};
  std::string error;
};

/// True for IPv4 loopback addresses (`127.0.0.0/8`).
bool isPlayIpcLoopbackHost(const std::string& host);

/// Parses `host:port` (e.g. `127.0.0.1:54321`). Host defaults to 127.0.0.1 if
/// only a port is given. Non-loopback hosts are rejected.
PlayIpcEndpoint parsePlayIpcEndpoint(const std::string& endpoint);
std::string formatPlayIpcEndpoint(const PlayIpcEndpoint& endpoint);
PlayIpcCommand parsePlayIpcCommandLine(const std::string& line);
const char* playIpcCommandName(PlayIpcCommand command);

/// Editor-side control host: binds and **holds** the listen socket (eliminates
/// ephemeral-port TOCTOU), accepts the Player, waits for `ready`, then sends
/// `pause` / `resume` / `stop`.
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
  bool sendLine(const std::string& line);

  void close();

 private:
  bool tryAccept();
  bool tryReadReady();

  std::uintptr_t m_listen_fd{0};
  std::uintptr_t m_client_fd{0};
  uint16_t m_bound_port{0};
  std::string m_recv_buffer;
  bool m_ready{false};
};

/// Player-side control agent: connects to the editor host, announces `ready`,
/// then receives pause/resume/stop commands.
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

  void setCommandHandler(std::function<void(PlayIpcCommand)> handler);

  /// Non-blocking read / dispatch of host commands. Call from Player frame loop.
  void poll();

  bool isConnected() const;
  void close();

 private:
  void processHostBuffer();

  std::function<void(PlayIpcCommand)> m_handler;
  std::uintptr_t m_fd{0};
  std::string m_recv_buffer;
  bool m_ready_sent{false};
};

}  // namespace Blunder
