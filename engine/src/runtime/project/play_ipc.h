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

/// Parses `host:port` (e.g. `127.0.0.1:54321`). Host defaults to 127.0.0.1 if
/// only a port is given.
PlayIpcEndpoint parsePlayIpcEndpoint(const std::string& endpoint);
std::string formatPlayIpcEndpoint(const PlayIpcEndpoint& endpoint);
PlayIpcCommand parsePlayIpcCommandLine(const std::string& line);
const char* playIpcCommandName(PlayIpcCommand command);

/// Line-oriented localhost TCP control channel.
/// Protocol: server emits `ready\n` after accept; client sends `pause` /
/// `resume` / `stop` (one command per line).
class PlayIpcServer {
 public:
  PlayIpcServer();
  ~PlayIpcServer();

  PlayIpcServer(const PlayIpcServer&) = delete;
  PlayIpcServer& operator=(const PlayIpcServer&) = delete;

  /// Bind @p endpoint (port 0 = ephemeral). Host should be loopback.
  bool listen(const PlayIpcEndpoint& endpoint);
  /// Convenience: listen on 127.0.0.1:@p port (0 = ephemeral).
  bool listen(uint16_t port = 0);

  uint16_t boundPort() const { return m_bound_port; }
  bool isListening() const;

  void setCommandHandler(std::function<void(PlayIpcCommand)> handler);

  /// Non-blocking accept / read / dispatch. Call from Player frame loop.
  void poll();

  void close();

 private:
  void processClientBuffer();

  std::function<void(PlayIpcCommand)> m_handler;
  std::uintptr_t m_listen_fd{0};
  std::uintptr_t m_client_fd{0};
  uint16_t m_bound_port{0};
  std::string m_recv_buffer;
  bool m_ready_sent{false};
};

class PlayIpcClient {
 public:
  PlayIpcClient();
  ~PlayIpcClient();

  PlayIpcClient(const PlayIpcClient&) = delete;
  PlayIpcClient& operator=(const PlayIpcClient&) = delete;

  bool connect(const PlayIpcEndpoint& endpoint);
  bool connect(const std::string& host, uint16_t port);

  /// Blocks until a `ready` line is received or @p timeout_ms elapses.
  bool waitReady(int timeout_ms = 2000);

  bool sendCommand(PlayIpcCommand command);
  bool sendLine(const std::string& line);

  bool isConnected() const;
  void close();

 private:
  std::uintptr_t m_fd{0};
  std::string m_recv_buffer;
  bool m_ready{false};
};

}  // namespace Blunder
