#include "runtime/project/machine_mcp.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace Blunder {

#ifdef _WIN32
namespace {
HANDLE g_mcp_stdin = INVALID_HANDLE_VALUE;
HANDLE g_mcp_stdout = INVALID_HANDLE_VALUE;

bool handleIsPipe(HANDLE handle) {
  return handle != nullptr && handle != INVALID_HANDLE_VALUE &&
         GetFileType(handle) == FILE_TYPE_PIPE;
}

void capturePipeHandle(HANDLE candidate, HANDLE& slot) {
  if (!handleIsPipe(candidate) || handleIsPipe(slot)) {
    return;
  }
  HANDLE duplicated = INVALID_HANDLE_VALUE;
  if (DuplicateHandle(GetCurrentProcess(), candidate, GetCurrentProcess(),
                      &duplicated, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    slot = duplicated;
  } else {
    slot = candidate;
  }
}

void captureStdStream(DWORD std_id, FILE* stream, HANDLE& slot) {
  if (handleIsPipe(slot)) {
    return;
  }
  if (stream != nullptr) {
    capturePipeHandle(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stream))),
                      slot);
  }
  capturePipeHandle(GetStdHandle(std_id), slot);
}
}  // namespace
#endif

void mcpCaptureStdioHandles() {
#ifdef _WIN32
  captureStdStream(STD_INPUT_HANDLE, stdin, g_mcp_stdin);
  captureStdStream(STD_OUTPUT_HANDLE, stdout, g_mcp_stdout);
  if (handleIsPipe(g_mcp_stdin)) {
    (void)_setmode(_fileno(stdin), _O_BINARY);
  }
  if (handleIsPipe(g_mcp_stdout)) {
    (void)_setmode(_fileno(stdout), _O_BINARY);
  }
#endif
}

#ifdef _WIN32
HANDLE mcpStdinHandle() {
  mcpCaptureStdioHandles();
  if (g_mcp_stdin != nullptr && g_mcp_stdin != INVALID_HANDLE_VALUE) {
    return g_mcp_stdin;
  }
  return GetStdHandle(STD_INPUT_HANDLE);
}

HANDLE mcpStdoutHandle() {
  mcpCaptureStdioHandles();
  if (g_mcp_stdout != nullptr && g_mcp_stdout != INVALID_HANDLE_VALUE) {
    return g_mcp_stdout;
  }
  return GetStdHandle(STD_OUTPUT_HANDLE);
}

namespace {
struct McpStdioCaptureAtLoad {
  McpStdioCaptureAtLoad() { mcpCaptureStdioHandles(); }
} g_mcp_stdio_capture_at_load;
}  // namespace
#endif

namespace {

bool jsonExtractObject(const std::string& src, const char* key, std::string& out) {
  const std::string needle = std::string("\"") + key + "\"";
  const size_t pos = src.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  const size_t colon = src.find(':', pos + needle.size());
  if (colon == std::string::npos) {
    return false;
  }
  size_t i = colon + 1;
  while (i < src.size() && (src[i] == ' ' || src[i] == '\t' || src[i] == '\n' ||
                            src[i] == '\r')) {
    ++i;
  }
  if (i >= src.size() || src[i] != '{') {
    return false;
  }
  int depth = 0;
  bool in_string = false;
  bool escape = false;
  const size_t begin = i;
  for (; i < src.size(); ++i) {
    const char c = src[i];
    if (in_string) {
      if (escape) {
        escape = false;
        continue;
      }
      if (c == '\\') {
        escape = true;
        continue;
      }
      if (c == '"') {
        in_string = false;
      }
      continue;
    }
    if (c == '"') {
      in_string = true;
      continue;
    }
    if (c == '{') {
      ++depth;
    } else if (c == '}') {
      --depth;
      if (depth == 0) {
        out = src.substr(begin, i - begin + 1);
        return true;
      }
    }
  }
  return false;
}

bool jsonExtractString(const std::string& src, const char* key, std::string& out) {
  const std::string needle = std::string("\"") + key + "\"";
  const size_t pos = src.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  const size_t colon = src.find(':', pos + needle.size());
  if (colon == std::string::npos) {
    return false;
  }
  size_t i = colon + 1;
  while (i < src.size() && (src[i] == ' ' || src[i] == '\t' || src[i] == '\n' ||
                            src[i] == '\r')) {
    ++i;
  }
  if (i >= src.size() || src[i] != '"') {
    return false;
  }
  ++i;
  out.clear();
  while (i < src.size() && src[i] != '"') {
    if (src[i] == '\\' && i + 1 < src.size()) {
      ++i;
    }
    out.push_back(src[i]);
    ++i;
  }
  return true;
}

bool jsonExtractRawId(const std::string& src, std::string& out) {
  const size_t pos = src.find("\"id\"");
  if (pos == std::string::npos) {
    out = "null";
    return false;
  }
  const size_t colon = src.find(':', pos);
  if (colon == std::string::npos) {
    out = "null";
    return false;
  }
  size_t i = colon + 1;
  while (i < src.size() && (src[i] == ' ' || src[i] == '\t')) {
    ++i;
  }
  if (i >= src.size()) {
    out = "null";
    return false;
  }
  if (src[i] == '"') {
    std::string value;
    if (!jsonExtractString(src, "id", value)) {
      out = "null";
      return false;
    }
    out = "\"" + value + "\"";
    return true;
  }
  const size_t end = src.find_first_of(",}", i);
  out = src.substr(i, end == std::string::npos ? std::string::npos : end - i);
  while (!out.empty() && (out.back() == ' ' || out.back() == '\r' ||
                          out.back() == '\n')) {
    out.pop_back();
  }
  return true;
}

std::string jsonRpcError(const std::string& id, int code, const char* message) {
  std::ostringstream os;
  os << "{\"jsonrpc\":\"2.0\",\"id\":" << id << ",\"error\":{\"code\":" << code
     << ",\"message\":\"" << message << "\"}}";
  return os.str();
}

std::string jsonRpcResult(const std::string& id, const std::string& result) {
  return std::string("{\"jsonrpc\":\"2.0\",\"id\":") + id + ",\"result\":" +
         result + "}";
}

void applyArgString(EditorSessionLaunch& launch, const std::string& key,
                    const std::string& value) {
  if (key == "subject") {
    launch.cli.subject = value.c_str();
  } else if (key == "out") {
    launch.cli.out_path = value.c_str();
  } else if (key == "name" || key == "entity") {
    launch.cli.entity = value.c_str();
  } else if (key == "asset") {
    launch.cli.asset = value.c_str();
  } else if (key == "scene") {
    launch.scene = value.c_str();
  }
}

void applyArgNumber(EditorSessionLaunch& launch, const std::string& key,
                    const std::string& value) {
  char* end = nullptr;
  if (key == "steps") {
    launch.cli.steps = static_cast<uint32_t>(std::strtoul(value.c_str(), &end, 10));
  } else if (key == "tx") {
    launch.cli.tx = std::strtof(value.c_str(), &end);
  } else if (key == "ty") {
    launch.cli.ty = std::strtof(value.c_str(), &end);
  } else if (key == "tz") {
    launch.cli.tz = std::strtof(value.c_str(), &end);
  } else if (key == "qx") {
    launch.cli.qx = std::strtof(value.c_str(), &end);
  } else if (key == "qy") {
    launch.cli.qy = std::strtof(value.c_str(), &end);
  } else if (key == "qz") {
    launch.cli.qz = std::strtof(value.c_str(), &end);
  } else if (key == "qw") {
    launch.cli.qw = std::strtof(value.c_str(), &end);
  } else if (key == "sx") {
    launch.cli.sx = std::strtof(value.c_str(), &end);
  } else if (key == "sy") {
    launch.cli.sy = std::strtof(value.c_str(), &end);
  } else if (key == "sz") {
    launch.cli.sz = std::strtof(value.c_str(), &end);
  } else if (key == "dx") {
    launch.cli.dx = std::strtof(value.c_str(), &end);
  } else if (key == "dy") {
    launch.cli.dy = std::strtof(value.c_str(), &end);
  } else if (key == "wheel") {
    launch.cli.wheel = std::strtof(value.c_str(), &end);
  } else if (key == "eye_x") {
    launch.cli.eye_x = std::strtof(value.c_str(), &end);
    launch.cli.has_eye = true;
  } else if (key == "eye_y") {
    launch.cli.eye_y = std::strtof(value.c_str(), &end);
    launch.cli.has_eye = true;
  } else if (key == "eye_z") {
    launch.cli.eye_z = std::strtof(value.c_str(), &end);
    launch.cli.has_eye = true;
  } else if (key == "target_x") {
    launch.cli.target_x = std::strtof(value.c_str(), &end);
    launch.cli.has_target = true;
  } else if (key == "target_y") {
    launch.cli.target_y = std::strtof(value.c_str(), &end);
    launch.cli.has_target = true;
  } else if (key == "target_z") {
    launch.cli.target_z = std::strtof(value.c_str(), &end);
    launch.cli.has_target = true;
  }
}

void scrapeArguments(const std::string& src, EditorSessionLaunch& launch) {
  const char* keys[] = {"subject", "out",     "name", "entity", "asset", "scene",
                        "steps",   "tx",      "ty",   "tz",     "qx",    "qy",
                        "qz",      "qw",      "sx",   "sy",     "sz",    "dx",
                        "dy",      "wheel",   "eye_x", "eye_y", "eye_z",
                        "target_x", "target_y", "target_z"};
  for (const char* key : keys) {
    std::string value;
    if (jsonExtractString(src, key, value)) {
      applyArgString(launch, key, value);
      continue;
    }
    const std::string needle = std::string("\"") + key + "\"";
    const size_t pos = src.find(needle);
    if (pos == std::string::npos) {
      continue;
    }
    const size_t colon = src.find(':', pos + needle.size());
    if (colon == std::string::npos) {
      continue;
    }
    size_t i = colon + 1;
    while (i < src.size() && (src[i] == ' ' || src[i] == '\t')) {
      ++i;
    }
    if (i < src.size() && src.compare(i, 4, "true") == 0 &&
        std::strcmp(key, "save") == 0) {
      launch.cli.save = true;
      continue;
    }
    const size_t end = src.find_first_of(",} \n\r", i);
    const std::string raw =
        src.substr(i, end == std::string::npos ? std::string::npos : end - i);
    applyArgNumber(launch, key, raw);
  }
  if (src.find("\"save\":true") != std::string::npos ||
      src.find("\"save\": true") != std::string::npos) {
    launch.cli.save = true;
  }
}

const char* k_tools_list =
    "{\"tools\":["
    "{\"name\":\"query\",\"description\":\"Authorship Query\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"subject\":{\"type\":"
    "\"string\"},\"name\":{\"type\":\"string\"},\"asset\":{\"type\":\"string\"}}"
    "}},"
    "{\"name\":\"op\",\"description\":\"Authorship transform Op\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":"
    "\"string\"},\"tx\":{\"type\":\"number\"},\"ty\":{\"type\":\"number\"},"
    "\"tz\":{\"type\":\"number\"}}}},"
    "{\"name\":\"diagnose\",\"description\":\"Authorship Diagnose\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"subject\":{\"type\":"
    "\"string\"},\"asset\":{\"type\":\"string\"}}}},"
    "{\"name\":\"capture\",\"description\":\"Host observation Capture\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"subject\":{\"type\":"
    "\"string\"},\"asset\":{\"type\":\"string\"}}}},"
    "{\"name\":\"get-camera\",\"description\":\"Read Viewport editor camera "
    "(eye, target, yaw, pitch, distance). Headless --mcp session camera.\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"set-camera\",\"description\":\"Set Viewport editor camera "
    "look-at (eye + target). Headless --mcp session camera.\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"eye_x\":{\"type\":"
    "\"number\"},\"eye_y\":{\"type\":\"number\"},\"eye_z\":{\"type\":\"number\"},"
    "\"target_x\":{\"type\":\"number\"},\"target_y\":{\"type\":\"number\"},"
    "\"target_z\":{\"type\":\"number\"}}}},"
    "{\"name\":\"orbit\",\"description\":\"MMB world-origin tumble from current "
    "eye. Pivot (0,0,0). No look-at snap. dx/dy are pixel deltas.\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"dx\":{\"type\":"
    "\"number\"},\"dy\":{\"type\":\"number\"}}}},"
    "{\"name\":\"orbit-camera\",\"description\":\"RMB camera-pivot orbit. Keeps "
    "the eye; rotates look target. dx/dy are pixel deltas.\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"dx\":{\"type\":"
    "\"number\"},\"dy\":{\"type\":\"number\"}}}},"
    "{\"name\":\"pan\",\"description\":\"Shift+MMB pan. dx/dy are pixel "
    "deltas.\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"dx\":{\"type\":"
    "\"number\"},\"dy\":{\"type\":\"number\"}}}},"
    "{\"name\":\"zoom\",\"description\":\"Wheel zoom along view (unclamped "
    "except the existing orbit distance cap). wheel matches scroll Y.\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"wheel\":{\"type\":"
    "\"number\"}}}},"
    "{\"name\":\"play\",\"description\":\"Start Play Session\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"pause\",\"description\":\"Play Pause\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"resume\",\"description\":\"Play Resume\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"stop\",\"description\":\"Play Stop\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"step\",\"description\":\"Play step\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"steps\":{\"type\":"
    "\"integer\"}}}},"
    "{\"name\":\"play-frame\",\"description\":\"Play frame\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"save\",\"description\":\"Persist Live document\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}"
    "]}";

}  // namespace

bool mcpStdinHasBytes() {
#ifdef _WIN32
  HANDLE handle = mcpStdinHandle();
  if (handle == INVALID_HANDLE_VALUE || handle == nullptr) {
    return false;
  }
  DWORD avail = 0;
  if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &avail, nullptr)) {
    return false;
  }
  return avail > 0;
#else
  pollfd fd{};
  fd.fd = STDIN_FILENO;
  fd.events = POLLIN;
  return poll(&fd, 1, 0) > 0;
#endif
}

bool mcpStdinClosed() {
#ifdef _WIN32
  HANDLE handle = mcpStdinHandle();
  if (handle == INVALID_HANDLE_VALUE || handle == nullptr) {
    return true;
  }
  DWORD avail = 0;
  if (PeekNamedPipe(handle, nullptr, 0, nullptr, &avail, nullptr)) {
    return false;
  }
  const DWORD err = GetLastError();
  return err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED;
#else
  pollfd fd{};
  fd.fd = STDIN_FILENO;
  fd.events = POLLIN;
  const int r = poll(&fd, 1, 0);
  if (r < 0) {
    return true;
  }
  return (fd.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0;
#endif
}

bool mcpMessageNeedsEngine(const std::string& request) {
  std::string method;
  if (!jsonExtractString(request, "method", method)) {
    return false;
  }
  return method == "tools/call";
}

namespace {

#ifdef _WIN32
bool mcpReadExact(HANDLE handle, void* buf, DWORD n) {
  auto* bytes = static_cast<char*>(buf);
  DWORD got_total = 0;
  while (got_total < n) {
    DWORD got = 0;
    if (!ReadFile(handle, bytes + got_total, n - got_total, &got, nullptr) ||
        got == 0) {
      return false;
    }
    got_total += got;
  }
  return true;
}

bool mcpReadLine(HANDLE handle, std::string& line) {
  line.clear();
  char c = 0;
  DWORD got = 0;
  while (ReadFile(handle, &c, 1, &got, nullptr) && got == 1) {
    if (c == '\n') {
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      return true;
    }
    line.push_back(c);
  }
  return !line.empty();
}

bool mcpWriteAll(HANDLE handle, const char* data, size_t n) {
  size_t off = 0;
  while (off < n) {
    DWORD chunk = static_cast<DWORD>(n - off);
    DWORD written = 0;
    if (!WriteFile(handle, data + off, chunk, &written, nullptr) ||
        written == 0) {
      return false;
    }
    off += written;
  }
  return true;
}

bool lineStartsWith(const std::string& line, const char* prefix) {
  const size_t n = std::strlen(prefix);
  return line.size() >= n && std::strncmp(line.c_str(), prefix, n) == 0;
}
#endif

}  // namespace

bool mcpReadMessage(std::string& json) {
  json.clear();
#ifdef _WIN32
  HANDLE hin = mcpStdinHandle();
  if (hin == nullptr || hin == INVALID_HANDLE_VALUE) {
    return false;
  }
  std::string line;
  if (!mcpReadLine(hin, line)) {
    return false;
  }
  const char* k_content_length = "Content-Length:";
  if (!lineStartsWith(line, k_content_length)) {
    json = std::move(line);
    return !json.empty();
  }
  int content_length = std::atoi(line.c_str() + std::strlen(k_content_length));
  while (true) {
    std::string header;
    if (!mcpReadLine(hin, header)) {
      return false;
    }
    if (header.empty()) {
      break;
    }
    if (lineStartsWith(header, k_content_length)) {
      content_length =
          std::atoi(header.c_str() + std::strlen(k_content_length));
    }
  }
  if (content_length < 0) {
    return false;
  }
  json.resize(static_cast<size_t>(content_length));
  return mcpReadExact(hin, json.data(), static_cast<DWORD>(content_length));
#else
  std::string header;
  char line[1024];
  int content_length = -1;
  while (std::fgets(line, sizeof(line), stdin) != nullptr) {
    if (std::strcmp(line, "\r\n") == 0 || std::strcmp(line, "\n") == 0) {
      break;
    }
    header += line;
    const char* key = "Content-Length:";
    const char* found = std::strstr(line, key);
    if (found != nullptr) {
      content_length = std::atoi(found + std::strlen(key));
    }
  }
  if (content_length < 0) {
    return false;
  }
  json.resize(static_cast<size_t>(content_length));
  const size_t got =
      std::fread(json.data(), 1, static_cast<size_t>(content_length), stdin);
  json.resize(got);
  return got == static_cast<size_t>(content_length);
#endif
}

void mcpWriteMessage(const std::string& json) {
#ifdef _WIN32
  HANDLE hout = mcpStdoutHandle();
  if (hout == nullptr || hout == INVALID_HANDLE_VALUE) {
    return;
  }
  char header[64];
  const int header_n = std::snprintf(
      header, sizeof(header), "Content-Length: %zu\r\n\r\n", json.size());
  if (header_n > 0) {
    (void)mcpWriteAll(hout, header, static_cast<size_t>(header_n));
  }
  (void)mcpWriteAll(hout, json.data(), json.size());
  (void)FlushFileBuffers(hout);
#else
  std::cout << "Content-Length: " << json.size() << "\r\n\r\n" << json
            << std::flush;
#endif
}

std::string mcpHandleMessage(const std::string& request,
                             const EditorSessionLaunch& session,
                             MachineAdapterHost& host) {
  std::string id;
  jsonExtractRawId(request, id);
  if (id.empty()) {
    id = "null";
  }
  std::string method;
  jsonExtractString(request, "method", method);
  if (method == "initialize") {
    return jsonRpcResult(
        id,
        "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},"
        "\"serverInfo\":{\"name\":\"blunder-editor\",\"version\":\"0.1\"},"
        "\"instructions\":\"CLI/MCP adapt Authorship, Host observation, and "
        "Play Session on this Headless Editor Session.\"}");
  }
  if (method == "notifications/initialized" || method == "initialized") {
    return {};
  }
  if (method == "tools/list") {
    return jsonRpcResult(id, k_tools_list);
  }
  if (method == "tools/call") {
    EditorSessionLaunch launch = session;
    launch.adapter = MachineAdapterKind::mcp;
    std::string name;
    std::string params;
    std::string arguments;
    if (jsonExtractObject(request, "params", params)) {
      jsonExtractString(params, "name", name);
      jsonExtractObject(params, "arguments", arguments);
    } else {
      jsonExtractString(request, "name", name);
    }
    launch.cli.verb = name.c_str();
    if (!arguments.empty()) {
      scrapeArguments(arguments, launch);
    } else if (params.empty()) {
      scrapeArguments(request, launch);
    }
    MachineResult result;
    dispatchMachineAdapter(launch, host, result);
    std::string text = machineResultJson(result);
    std::string content = "[{\"type\":\"text\",\"text\":";
    // escape text as JSON string via wrapping
    std::string escaped;
    escaped.push_back('"');
    for (char c : text) {
      if (c == '\\' || c == '"') {
        escaped.push_back('\\');
      }
      escaped.push_back(c);
    }
    escaped.push_back('"');
    content += escaped;
    content += "}";
    if (!result.png.empty()) {
      content += ",{\"type\":\"image\",\"mimeType\":\"image/png\",\"data\":\"";
      content += base64Encode(result.png.data(), result.png.size());
      content += "\"}";
    }
    content += "]";
    std::string is_error = result.ok ? "false" : "true";
    return jsonRpcResult(id, std::string("{\"content\":") + content +
                                 ",\"isError\":" + is_error + "}");
  }
  if (method == "ping") {
    return jsonRpcResult(id, "{}");
  }
  return jsonRpcError(id, -32601, "Method not found");
}

}  // namespace Blunder
