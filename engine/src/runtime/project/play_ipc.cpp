#include "runtime/project/play_ipc.h"
#include "runtime/project/play_step.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <thread>
#include <vector>

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

std::string jsonEscape(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (unsigned char ch : value) {
    switch (ch) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (ch < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
          out += buf;
        } else {
          out.push_back(static_cast<char>(ch));
        }
        break;
    }
  }
  return out;
}

std::string truncatePlayIpcField(std::string value) {
  constexpr size_t kCap = 16 * 1024;
  if (value.size() <= kCap) {
    return value;
  }
  value.resize(kCap);
  return value;
}

bool isPlayIpcSev(const std::string& sev) {
  return sev == "log" || sev == "warning" || sev == "error";
}

bool extractJsonStringField(const std::string& json, const char* key,
                            std::string& out) {
  const std::string needle = std::string("\"") + key + "\":\"";
  const size_t start = json.find(needle);
  if (start == std::string::npos) {
    return false;
  }
  size_t i = start + needle.size();
  out.clear();
  while (i < json.size()) {
    const char ch = json[i++];
    if (ch == '\\') {
      if (i >= json.size()) {
        return false;
      }
      const char esc = json[i++];
      switch (esc) {
        case '"':
        case '\\':
        case '/':
          out.push_back(esc);
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'u':
          if (i + 4 > json.size()) {
            return false;
          }
          i += 4;  // skip unicode escape; keep empty for v1
          break;
        default:
          out.push_back(esc);
          break;
      }
      continue;
    }
    if (ch == '"') {
      return true;
    }
    out.push_back(ch);
  }
  return false;
}

bool extractJsonNumberField(const std::string& json, const char* key,
                            int64_t& out) {
  const std::string needle = std::string("\"") + key + "\":";
  const size_t start = json.find(needle);
  if (start == std::string::npos) {
    return false;
  }
  size_t i = start + needle.size();
  while (i < json.size() &&
         std::isspace(static_cast<unsigned char>(json[i]))) {
    ++i;
  }
  char* end = nullptr;
  const long long value = std::strtoll(json.c_str() + i, &end, 10);
  if (end == json.c_str() + i) {
    return false;
  }
  out = static_cast<int64_t>(value);
  return true;
}

const char k_b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const uint8_t* data, size_t len) {
  std::string out;
  out.reserve(((len + 2) / 3) * 4);
  size_t i = 0;
  while (i + 2 < len) {
    const uint32_t n = (static_cast<uint32_t>(data[i]) << 16) |
                       (static_cast<uint32_t>(data[i + 1]) << 8) |
                       static_cast<uint32_t>(data[i + 2]);
    out.push_back(k_b64[(n >> 18) & 63u]);
    out.push_back(k_b64[(n >> 12) & 63u]);
    out.push_back(k_b64[(n >> 6) & 63u]);
    out.push_back(k_b64[n & 63u]);
    i += 3;
  }
  if (i < len) {
    uint32_t n = static_cast<uint32_t>(data[i]) << 16;
    if (i + 1 < len) {
      n |= static_cast<uint32_t>(data[i + 1]) << 8;
    }
    out.push_back(k_b64[(n >> 18) & 63u]);
    out.push_back(k_b64[(n >> 12) & 63u]);
    if (i + 1 < len) {
      out.push_back(k_b64[(n >> 6) & 63u]);
      out.push_back('=');
    } else {
      out.push_back('=');
      out.push_back('=');
    }
  }
  return out;
}

int b64Value(char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return ch - 'A';
  }
  if (ch >= 'a' && ch <= 'z') {
    return ch - 'a' + 26;
  }
  if (ch >= '0' && ch <= '9') {
    return ch - '0' + 52;
  }
  if (ch == '+') {
    return 62;
  }
  if (ch == '/') {
    return 63;
  }
  return -1;
}

bool base64Decode(const std::string& in, std::vector<uint8_t>& out) {
  out.clear();
  out.reserve((in.size() / 4) * 3);
  int val = 0;
  int bits = -8;
  for (unsigned char ch : in) {
    if (ch == '=') {
      break;
    }
    if (std::isspace(ch)) {
      continue;
    }
    const int d = b64Value(static_cast<char>(ch));
    if (d < 0) {
      out.clear();
      return false;
    }
    val = (val << 6) | d;
    bits += 6;
    if (bits >= 0) {
      out.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
      bits -= 8;
    }
  }
  return true;
}

}  // namespace

std::string formatPlayIpcLogLine(const PlayIpcLogRecord& record) {
  PlayIpcLogRecord capped = record;
  if (!isPlayIpcSev(capped.sev)) {
    capped.sev = "log";
  }
  capped.text = truncatePlayIpcField(std::move(capped.text));
  capped.stack = truncatePlayIpcField(std::move(capped.stack));
  std::ostringstream oss;
  oss << "{\"v\":1,\"sev\":\"" << jsonEscape(capped.sev) << "\",\"text\":\""
      << jsonEscape(capped.text) << "\",\"stack\":\"" << jsonEscape(capped.stack)
      << "\",\"ms\":" << capped.ms << "}";
  return oss.str();
}

bool parsePlayIpcLogLine(const std::string& line, PlayIpcLogRecord& out) {
  const std::string trimmed = trimAscii(line);
  if (trimmed.empty() || trimmed.front() != '{') {
    return false;
  }
  std::string sev;
  if (!extractJsonStringField(trimmed, "sev", sev) || !isPlayIpcSev(sev)) {
    return false;
  }
  std::string text;
  if (!extractJsonStringField(trimmed, "text", text)) {
    return false;
  }
  std::string stack;
  (void)extractJsonStringField(trimmed, "stack", stack);
  int64_t ms = 0;
  (void)extractJsonNumberField(trimmed, "ms", ms);
  out.sev = std::move(sev);
  out.text = std::move(text);
  out.stack = std::move(stack);
  out.ms = ms;
  return true;
}

std::string formatPlayIpcFrameLine(const PlayIpcFrameRecord& record) {
  std::string encoding = record.encoding.empty() ? "rgba8" : record.encoding;
  std::ostringstream oss;
  oss << "{\"v\":1,\"kind\":\"frame\",\"width\":" << record.width
      << ",\"height\":" << record.height << ",\"encoding\":\""
      << jsonEscape(encoding) << "\",\"data\":\""
      << jsonEscape(base64Encode(record.rgba.data(), record.rgba.size()))
      << "\"}";
  return oss.str();
}

bool parsePlayIpcFrameLine(const std::string& line, PlayIpcFrameRecord& out) {
  const std::string trimmed = trimAscii(line);
  if (trimmed.empty() || trimmed.front() != '{') {
    return false;
  }
  std::string kind;
  if (!extractJsonStringField(trimmed, "kind", kind) || kind != "frame") {
    return false;
  }
  int64_t width = 0;
  int64_t height = 0;
  if (!extractJsonNumberField(trimmed, "width", width) ||
      !extractJsonNumberField(trimmed, "height", height) || width <= 0 ||
      height <= 0) {
    return false;
  }
  std::string encoding;
  if (!extractJsonStringField(trimmed, "encoding", encoding) ||
      encoding.empty()) {
    return false;
  }
  std::string data;
  if (!extractJsonStringField(trimmed, "data", data)) {
    return false;
  }
  std::vector<uint8_t> rgba;
  if (!base64Decode(data, rgba)) {
    return false;
  }
  out.width = static_cast<uint32_t>(width);
  out.height = static_cast<uint32_t>(height);
  out.encoding = std::move(encoding);
  out.rgba = std::move(rgba);
  return true;
}

std::string formatPlayIpcErrorLine(const std::string& code) {
  std::ostringstream oss;
  oss << "{\"v\":1,\"kind\":\"error\",\"code\":\"" << jsonEscape(code) << "\"}";
  return oss.str();
}

bool parsePlayIpcErrorLine(const std::string& line, PlayIpcErrorRecord& out) {
  const std::string trimmed = trimAscii(line);
  if (trimmed.empty() || trimmed.front() != '{') {
    return false;
  }
  std::string kind;
  if (!extractJsonStringField(trimmed, "kind", kind) || kind != "error") {
    return false;
  }
  std::string code;
  if (!extractJsonStringField(trimmed, "code", code) || code.empty()) {
    return false;
  }
  out.code = std::move(code);
  return true;
}

bool isPlayIpcLoopbackHost(const std::string& host) {
  if (host.empty()) {
    return false;
  }
  in_addr addr{};
  if (inet_pton(AF_INET, host.c_str(), &addr) != 1) {
    return false;
  }
  const uint32_t n = ntohl(addr.s_addr);
  return (n & 0xFF000000u) == 0x7F000000u;  // 127.0.0.0/8
}

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

  if (!isPlayIpcLoopbackHost(host)) {
    out.error = "play-ipc host must be IPv4 loopback (127.0.0.0/8)";
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

PlayIpcHostCommand parsePlayIpcHostCommandLine(const std::string& line) {
  PlayIpcHostCommand out;
  const std::string cmd = trimAscii(line);
  if (cmd == "pause") {
    out.command = PlayIpcCommand::Pause;
    return out;
  }
  if (cmd == "resume") {
    out.command = PlayIpcCommand::Resume;
    return out;
  }
  if (cmd == "stop") {
    out.command = PlayIpcCommand::Stop;
    return out;
  }
  if (cmd == "frame") {
    out.command = PlayIpcCommand::Frame;
    return out;
  }
  if (cmd.size() >= 6 && cmd.compare(0, 5, "step ") == 0) {
    char* end = nullptr;
    const unsigned long ticks = std::strtoul(cmd.c_str() + 5, &end, 10);
    if (end == cmd.c_str() + 5 || (end && *end != '\0') || ticks == 0) {
      return out;
    }
    out.command = PlayIpcCommand::Step;
    out.step_ticks = static_cast<uint32_t>(
        ticks > k_play_step_max_ticks ? k_play_step_max_ticks : ticks);
    return out;
  }
  return out;
}

PlayIpcCommand parsePlayIpcCommandLine(const std::string& line) {
  return parsePlayIpcHostCommandLine(line).command;
}

const char* playIpcCommandName(PlayIpcCommand command) {
  switch (command) {
    case PlayIpcCommand::Pause:
      return "pause";
    case PlayIpcCommand::Resume:
      return "resume";
    case PlayIpcCommand::Stop:
      return "stop";
    case PlayIpcCommand::Step:
      return "step";
    case PlayIpcCommand::Frame:
      return "frame";
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
  if (!isPlayIpcLoopbackHost(endpoint.host)) {
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

bool PlayIpcServer::tryAccept() {
  const SocketHandle listen_fd = asSocket(m_listen_fd);
  if (listen_fd == kInvalidSocket) {
    return false;
  }
  if (asSocket(m_client_fd) != kInvalidSocket) {
    return true;
  }

  const SocketHandle client = ::accept(listen_fd, nullptr, nullptr);
  if (client == kInvalidSocket) {
    return false;
  }
  if (!setNonBlocking(client)) {
    closeSocket(client);
    return false;
  }
  m_client_fd = toRaw(client);
  m_recv_buffer.clear();
  m_ready = false;
  return true;
}

bool PlayIpcServer::tryReadReady() {
  if (m_ready) {
    return true;
  }
  const SocketHandle client_fd = asSocket(m_client_fd);
  if (client_fd == kInvalidSocket) {
    return false;
  }
  if (!recvAppend(client_fd, m_recv_buffer)) {
    closeSocket(client_fd);
    m_client_fd = 0;
    m_recv_buffer.clear();
    return false;
  }
  std::string line;
  while (popLine(m_recv_buffer, line)) {
    if (trimAscii(line) == "ready") {
      m_ready = true;
      return true;
    }
  }
  return false;
}

bool PlayIpcServer::waitPeerReady(int timeout_ms) {
  if (m_ready) {
    return true;
  }
  if (!isListening()) {
    return false;
  }

  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(std::max(0, timeout_ms));
  for (;;) {
    (void)tryAccept();
    if (tryReadReady()) {
      return true;
    }
    if (timeout_ms <= 0 ||
        std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

bool PlayIpcServer::sendCommand(PlayIpcCommand command) {
  if (command == PlayIpcCommand::Unknown || command == PlayIpcCommand::Step) {
    return false;
  }
  return sendLine(playIpcCommandName(command));
}

bool PlayIpcServer::sendStep(uint32_t ticks) {
  if (ticks == 0) {
    return false;
  }
  if (ticks > k_play_step_max_ticks) {
    ticks = k_play_step_max_ticks;
  }
  return sendLine(std::string("step ") + std::to_string(ticks));
}

bool PlayIpcServer::sendFrameRequest() { return sendCommand(PlayIpcCommand::Frame); }

bool PlayIpcServer::sendLine(const std::string& line) {
  if (!m_ready) {
    return false;
  }
  const SocketHandle fd = asSocket(m_client_fd);
  if (fd == kInvalidSocket) {
    return false;
  }
  std::string payload = line;
  if (payload.empty() || payload.back() != '\n') {
    payload.push_back('\n');
  }
  return sendAll(fd, payload.data(), static_cast<int>(payload.size()));
}

void PlayIpcServer::drainInbound() {
  if (!m_ready) {
    return;
  }
  const SocketHandle client_fd = asSocket(m_client_fd);
  if (client_fd == kInvalidSocket) {
    return;
  }
  if (!recvAppend(client_fd, m_recv_buffer)) {
    closeSocket(client_fd);
    m_client_fd = 0;
  }
  std::string line;
  while (popLine(m_recv_buffer, line)) {
    PlayIpcFrameRecord frame;
    if (parsePlayIpcFrameLine(line, frame)) {
      m_pending_frames.push_back(std::move(frame));
      continue;
    }
    PlayIpcErrorRecord error;
    if (parsePlayIpcErrorLine(line, error)) {
      m_pending_errors.push_back(std::move(error));
      continue;
    }
    PlayIpcLogRecord record;
    if (parsePlayIpcLogLine(line, record)) {
      m_pending_logs.push_back(std::move(record));
    }
  }
}

std::vector<PlayIpcLogRecord> PlayIpcServer::pollLogs() {
  drainInbound();
  std::vector<PlayIpcLogRecord> out;
  out.swap(m_pending_logs);
  return out;
}

std::vector<PlayIpcFrameRecord> PlayIpcServer::pollFrames() {
  drainInbound();
  std::vector<PlayIpcFrameRecord> out;
  out.swap(m_pending_frames);
  return out;
}

std::vector<PlayIpcErrorRecord> PlayIpcServer::pollErrors() {
  drainInbound();
  std::vector<PlayIpcErrorRecord> out;
  out.swap(m_pending_errors);
  return out;
}

void PlayIpcServer::close() {
  closeSocket(asSocket(m_client_fd));
  closeSocket(asSocket(m_listen_fd));
  m_client_fd = 0;
  m_listen_fd = 0;
  m_bound_port = 0;
  m_recv_buffer.clear();
  m_ready = false;
  m_pending_logs.clear();
  m_pending_frames.clear();
  m_pending_errors.clear();
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
  if (!isPlayIpcLoopbackHost(host)) {
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

  // Blocking connect is fine for the Player / unit test.
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
  m_ready_sent = false;
  return true;
}

bool PlayIpcClient::announceReady() {
  const SocketHandle fd = asSocket(m_fd);
  if (fd == kInvalidSocket) {
    return false;
  }
  if (m_ready_sent) {
    return true;
  }
  static const char kReady[] = "ready\n";
  if (!sendAll(fd, kReady, static_cast<int>(sizeof(kReady) - 1))) {
    return false;
  }
  m_ready_sent = true;
  return true;
}

bool PlayIpcClient::sendRawLine(const std::string& line) {
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

void PlayIpcClient::setCommandHandler(
    std::function<void(PlayIpcHostCommand)> handler) {
  m_handler = std::move(handler);
}

void PlayIpcClient::poll() {
  const SocketHandle fd = asSocket(m_fd);
  if (fd == kInvalidSocket) {
    return;
  }
  if (!recvAppend(fd, m_recv_buffer)) {
    close();
    return;
  }
  processHostBuffer();
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

bool PlayIpcClient::sendLog(const PlayIpcLogRecord& record) {
  if (!m_ready_sent) {
    return false;
  }
  return sendLine(formatPlayIpcLogLine(record));
}

bool PlayIpcClient::sendFrame(const PlayIpcFrameRecord& record) {
  if (!m_ready_sent) {
    return false;
  }
  return sendLine(formatPlayIpcFrameLine(record));
}

bool PlayIpcClient::sendError(const std::string& code) {
  if (!m_ready_sent) {
    return false;
  }
  return sendLine(formatPlayIpcErrorLine(code));
}

void PlayIpcClient::processHostBuffer() {
  std::string line;
  while (popLine(m_recv_buffer, line)) {
    const PlayIpcHostCommand cmd = parsePlayIpcHostCommandLine(line);
    if (cmd.command == PlayIpcCommand::Unknown) {
      continue;
    }
    if (m_handler) {
      m_handler(cmd);
    }
  }
}

bool PlayIpcClient::isConnected() const {
  return asSocket(m_fd) != kInvalidSocket;
}

void PlayIpcClient::close() {
  closeSocket(asSocket(m_fd));
  m_fd = 0;
  m_recv_buffer.clear();
  m_ready_sent = false;
}

}  // namespace Blunder
