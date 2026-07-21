#include "runtime/project/play_ipc.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifndef _WIN32
#include <cerrno>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace Blunder {
namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;

int lastSocketError() { return WSAGetLastError(); }

bool wouldBlock(int err) {
  return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
}

void closeSocket(SocketHandle fd) {
  if (fd != kInvalidSocket) {
    closesocket(fd);
  }
}

bool setNonBlocking(SocketHandle fd) {
  u_long mode = 1;
  return ioctlsocket(fd, FIONBIO, &mode) == 0;
}
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;

int lastSocketError() { return errno; }

bool wouldBlock(int err) {
  return err == EWOULDBLOCK || err == EAGAIN || err == EINPROGRESS;
}

void closeSocket(SocketHandle fd) {
  if (fd != kInvalidSocket) {
    ::close(fd);
  }
}

bool setNonBlocking(SocketHandle fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}
#endif

SocketHandle asSocket(std::uintptr_t raw) {
  if (raw == 0) {
    return kInvalidSocket;
  }
  return static_cast<SocketHandle>(raw);
}

std::uintptr_t toRaw(SocketHandle fd) {
  if (fd == kInvalidSocket) {
    return 0;
  }
  return static_cast<std::uintptr_t>(fd);
}

bool ensureSockets() {
#ifdef _WIN32
  static bool ready = false;
  static bool ok = false;
  if (!ready) {
    WSADATA data{};
    ok = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
    ready = true;
  }
  return ok;
#else
  return true;
#endif
}

std::string trimAscii(std::string s) {
  while (!s.empty() &&
         std::isspace(static_cast<unsigned char>(s.front()))) {
    s.erase(s.begin());
  }
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
    s.pop_back();
  }
  return s;
}

bool sendAll(SocketHandle fd, const char* data, int len) {
  int sent_total = 0;
  while (sent_total < len) {
    const int n = ::send(fd, data + sent_total, len - sent_total, 0);
    if (n > 0) {
      sent_total += n;
      continue;
    }
    if (n < 0 && wouldBlock(lastSocketError())) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }
    return false;
  }
  return true;
}

bool recvAppend(SocketHandle fd, std::string& buffer) {
  char chunk[512];
  for (;;) {
    const int n = ::recv(fd, chunk, static_cast<int>(sizeof(chunk)), 0);
    if (n > 0) {
      buffer.append(chunk, static_cast<size_t>(n));
      continue;
    }
    if (n == 0) {
      return false;  // peer closed
    }
    if (wouldBlock(lastSocketError())) {
      return true;
    }
    return false;
  }
}

bool popLine(std::string& buffer, std::string& line) {
  const size_t nl = buffer.find('\n');
  if (nl == std::string::npos) {
    return false;
  }
  line = buffer.substr(0, nl);
  buffer.erase(0, nl + 1);
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  return true;
}

}  // namespace

PlayIpcEndpoint parsePlayIpcEndpoint(const std::string& endpoint) {
  PlayIpcEndpoint out;
  const std::string trimmed = trimAscii(endpoint);
  if (trimmed.empty()) {
    out.error = "empty --play-ipc endpoint";
    return out;
  }

  std::string host = "127.0.0.1";
  std::string port_text = trimmed;
  const size_t colon = trimmed.rfind(':');
  if (colon != std::string::npos) {
    host = trimmed.substr(0, colon);
    port_text = trimmed.substr(colon + 1);
    if (host.empty()) {
      host = "127.0.0.1";
    }
  }

  if (port_text.empty()) {
    out.error = "missing port in --play-ipc endpoint";
    return out;
  }

  char* end = nullptr;
  const unsigned long port =
      std::strtoul(port_text.c_str(), &end, 10);
  if (end == port_text.c_str() || (end && *end != '\0') || port > 65535ul) {
    out.error = "invalid port in --play-ipc endpoint";
    return out;
  }

  out.ok = true;
  out.host = std::move(host);
  out.port = static_cast<uint16_t>(port);
  return out;
}

std::string formatPlayIpcEndpoint(const PlayIpcEndpoint& endpoint) {
  return endpoint.host + ":" + std::to_string(endpoint.port);
}

PlayIpcCommand parsePlayIpcCommandLine(const std::string& line) {
  const std::string cmd = trimAscii(line);
  if (cmd == "pause") {
    return PlayIpcCommand::Pause;
  }
  if (cmd == "resume") {
    return PlayIpcCommand::Resume;
  }
  if (cmd == "stop") {
    return PlayIpcCommand::Stop;
  }
  return PlayIpcCommand::Unknown;
}

const char* playIpcCommandName(PlayIpcCommand command) {
  switch (command) {
    case PlayIpcCommand::Pause:
      return "pause";
    case PlayIpcCommand::Resume:
      return "resume";
    case PlayIpcCommand::Stop:
      return "stop";
    case PlayIpcCommand::Unknown:
    default:
      return "unknown";
  }
}

PlayIpcServer::PlayIpcServer() = default;

PlayIpcServer::~PlayIpcServer() { close(); }

bool PlayIpcServer::listen(uint16_t port) {
  PlayIpcEndpoint ep;
  ep.ok = true;
  ep.host = "127.0.0.1";
  ep.port = port;
  return listen(ep);
}

bool PlayIpcServer::listen(const PlayIpcEndpoint& endpoint) {
  close();
  if (!ensureSockets()) {
    return false;
  }

  const SocketHandle fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd == kInvalidSocket) {
    return false;
  }

  int yes = 1;
#ifdef _WIN32
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes),
             sizeof(yes));
#else
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(endpoint.port);
  if (inet_pton(AF_INET, endpoint.host.c_str(), &addr.sin_addr) != 1) {
    closeSocket(fd);
    return false;
  }

  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    closeSocket(fd);
    return false;
  }

  if (::listen(fd, 1) != 0) {
    closeSocket(fd);
    return false;
  }

  if (!setNonBlocking(fd)) {
    closeSocket(fd);
    return false;
  }

  sockaddr_in bound{};
  socklen_t bound_len = sizeof(bound);
  if (getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &bound_len) != 0) {
    closeSocket(fd);
    return false;
  }

  m_listen_fd = toRaw(fd);
  m_bound_port = ntohs(bound.sin_port);
  return true;
}

bool PlayIpcServer::isListening() const {
  return asSocket(m_listen_fd) != kInvalidSocket;
}

void PlayIpcServer::setCommandHandler(
    std::function<void(PlayIpcCommand)> handler) {
  m_handler = std::move(handler);
}

void PlayIpcServer::poll() {
  const SocketHandle listen_fd = asSocket(m_listen_fd);
  if (listen_fd == kInvalidSocket) {
    return;
  }

  if (asSocket(m_client_fd) == kInvalidSocket) {
    const SocketHandle client = ::accept(listen_fd, nullptr, nullptr);
    if (client == kInvalidSocket) {
      return;
    }
    if (!setNonBlocking(client)) {
      closeSocket(client);
      return;
    }
    m_client_fd = toRaw(client);
    m_recv_buffer.clear();
    m_ready_sent = false;
  }

  const SocketHandle client_fd = asSocket(m_client_fd);
  if (client_fd == kInvalidSocket) {
    return;
  }

  if (!m_ready_sent) {
    static const char kReady[] = "ready\n";
    if (!sendAll(client_fd, kReady, static_cast<int>(sizeof(kReady) - 1))) {
      closeSocket(client_fd);
      m_client_fd = 0;
      return;
    }
    m_ready_sent = true;
  }

  if (!recvAppend(client_fd, m_recv_buffer)) {
    closeSocket(client_fd);
    m_client_fd = 0;
    m_recv_buffer.clear();
    m_ready_sent = false;
    return;
  }

  processClientBuffer();
}

void PlayIpcServer::processClientBuffer() {
  std::string line;
  while (popLine(m_recv_buffer, line)) {
    const PlayIpcCommand cmd = parsePlayIpcCommandLine(line);
    if (cmd == PlayIpcCommand::Unknown) {
      continue;
    }
    if (m_handler) {
      m_handler(cmd);
    }
  }
}

void PlayIpcServer::close() {
  closeSocket(asSocket(m_client_fd));
  closeSocket(asSocket(m_listen_fd));
  m_client_fd = 0;
  m_listen_fd = 0;
  m_bound_port = 0;
  m_recv_buffer.clear();
  m_ready_sent = false;
}

PlayIpcClient::PlayIpcClient() = default;

PlayIpcClient::~PlayIpcClient() { close(); }

bool PlayIpcClient::connect(const PlayIpcEndpoint& endpoint) {
  if (!endpoint.ok) {
    return false;
  }
  return connect(endpoint.host, endpoint.port);
}

bool PlayIpcClient::connect(const std::string& host, uint16_t port) {
  close();
  if (!ensureSockets()) {
    return false;
  }

  const SocketHandle fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd == kInvalidSocket) {
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    closeSocket(fd);
    return false;
  }

  // Blocking connect is fine for the editor-side client / unit test.
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    closeSocket(fd);
    return false;
  }

  if (!setNonBlocking(fd)) {
    closeSocket(fd);
    return false;
  }

  m_fd = toRaw(fd);
  m_recv_buffer.clear();
  m_ready = false;
  return true;
}

bool PlayIpcClient::waitReady(int timeout_ms) {
  const SocketHandle fd = asSocket(m_fd);
  if (fd == kInvalidSocket) {
    return false;
  }
  if (m_ready) {
    return true;
  }

  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(std::max(0, timeout_ms));
  while (std::chrono::steady_clock::now() <= deadline) {
    if (!recvAppend(fd, m_recv_buffer)) {
      return false;
    }
    std::string line;
    while (popLine(m_recv_buffer, line)) {
      if (trimAscii(line) == "ready") {
        m_ready = true;
        return true;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return false;
}

bool PlayIpcClient::sendCommand(PlayIpcCommand command) {
  if (command == PlayIpcCommand::Unknown) {
    return false;
  }
  return sendLine(playIpcCommandName(command));
}

bool PlayIpcClient::sendLine(const std::string& line) {
  const SocketHandle fd = asSocket(m_fd);
  if (fd == kInvalidSocket) {
    return false;
  }
  std::string payload = line;
  if (payload.empty() || payload.back() != '\n') {
    payload.push_back('\n');
  }
  return sendAll(fd, payload.data(), static_cast<int>(payload.size()));
}

bool PlayIpcClient::isConnected() const {
  return asSocket(m_fd) != kInvalidSocket;
}

void PlayIpcClient::close() {
  closeSocket(asSocket(m_fd));
  m_fd = 0;
  m_recv_buffer.clear();
  m_ready = false;
}

}  // namespace Blunder
