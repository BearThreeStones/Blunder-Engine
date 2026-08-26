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
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace Blunder {

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
  }
}

void scrapeArguments(const std::string& src, EditorSessionLaunch& launch) {
  const char* keys[] = {"subject", "out",     "name", "entity", "asset", "scene",
                        "steps",   "tx",      "ty",   "tz",     "qx",    "qy",
                        "qz",      "qw",      "sx",   "sy",     "sz"};
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
  HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
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

bool mcpReadMessage(std::string& json) {
  json.clear();
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
}

void mcpWriteMessage(const std::string& json) {
  std::cout << "Content-Length: " << json.size() << "\r\n\r\n" << json
            << std::flush;
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
