#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "runtime/core/base/macro.h"
#include "runtime/engine.h"
#include "runtime/function/global/engine_host_mode.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/project/editor_launch.h"
#include "runtime/project/machine_adapter.h"
#include "runtime/project/machine_mcp.h"

namespace {

Blunder::BlunderEngine* g_engine = nullptr;
Blunder::EditorSessionLaunch g_launch{};
bool g_mcp = false;
bool g_mcp_saw_initialize = false;
bool g_mcp_saw_tools_list = false;
bool g_mcp_engine_attempted = false;

std::filesystem::path compiledProjectRoot() {
#ifdef BLUNDER_PROJECT_ROOT
  return std::filesystem::path(BLUNDER_PROJECT_ROOT);
#else
  return {};
#endif
}

bool isDebugLaunchBuild() {
#ifdef BLUNDER_PROJECT_ROOT
  return true;
#else
  return false;
#endif
}

void mcpBreadcrumb(const char* msg) {
  const char* path = std::getenv("BLUNDER_LOG_FILE");
  if (path == nullptr || path[0] == '\0' || msg == nullptr) {
    return;
  }
  FILE* file = nullptr;
#if defined(_MSC_VER)
  if (fopen_s(&file, path, "a") != 0 || file == nullptr) {
    return;
  }
#else
  file = std::fopen(path, "a");
  if (file == nullptr) {
    return;
  }
#endif
  std::fputs(msg, file);
  std::fputc('\n', file);
  std::fflush(file);
  std::fclose(file);
}

#ifdef _WIN32
LONG CALLBACK mcpAccessViolationVeh(PEXCEPTION_POINTERS info) {
  if (info != nullptr && info->ExceptionRecord != nullptr &&
      info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
    char line[192];
    std::snprintf(line, sizeof(line), "AV at %p code=%lx",
                  info->ExceptionRecord->ExceptionAddress,
                  static_cast<unsigned long>(info->ExceptionRecord->ExceptionCode));
    mcpBreadcrumb(line);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}
#endif

Blunder::MachineAdapterHost makeHost() {
  Blunder::MachineAdapterHost host;
  auto& ctx = Blunder::g_runtime_global_context;
  host.project_root = g_launch.project_root;
  host.authorship = ctx.m_authorship.get();
  host.file_system = ctx.m_file_system.get();
  host.thumbs = ctx.m_scene_thumbnail_service.get();
  host.scene_edit = ctx.m_editor_scene_edit.get();
  host.play = ctx.m_play_session.get();
  if (ctx.m_scene_system) {
    host.live_scene = ctx.m_scene_system->getActiveInstance();
  }
  host.editor_camera = ctx.editorCamera();
  host.pump = []() {
    if (g_engine) {
      g_engine->tickOneFrame(1.0f / 60.0f);
    }
  };
  return host;
}

bool ensureMcpEngine() {
  if (g_engine) {
    return true;
  }
  if (g_mcp_engine_attempted) {
    return false;
  }
  g_mcp_engine_attempted = true;
#ifdef _WIN32
  (void)AddVectoredExceptionHandler(1, mcpAccessViolationVeh);
#endif
  mcpBreadcrumb("ensureMcpEngine begin");
  try {
    g_engine = new Blunder::BlunderEngine();
    mcpBreadcrumb("startEngine begin");
    g_engine->startEngine(g_launch.project_root, Blunder::EngineHostMode::Editor,
                          {}, g_launch.headless);
    mcpBreadcrumb("startEngine done");
    LOG_INFO("[MCP] startEngine done");
    if (Blunder::g_runtime_global_context.isQuitRequested()) {
      mcpBreadcrumb("startEngine quit requested");
      return false;
    }
    mcpBreadcrumb("initialize begin");
    LOG_INFO("[MCP] initialize begin scene='{}'", g_launch.scene.c_str());
    g_engine->initialize(g_launch.scene, false);
    mcpBreadcrumb("initialize done");
    LOG_INFO("[MCP] initialize done");
    return true;
  } catch (const std::exception& e) {
    std::cerr << "MCP engine boot failed: " << e.what() << std::endl;
    delete g_engine;
    g_engine = nullptr;
    return false;
  } catch (...) {
    std::cerr << "MCP engine boot failed: unknown exception" << std::endl;
    delete g_engine;
    g_engine = nullptr;
    return false;
  }
}

void dispatchMcp(const std::string& message) {
  if (message.find("\"method\":\"initialize\"") != std::string::npos ||
      message.find("\"method\": \"initialize\"") != std::string::npos) {
    g_mcp_saw_initialize = true;
  }
  if (message.find("\"tools/list\"") != std::string::npos) {
    g_mcp_saw_tools_list = true;
  }
  if (Blunder::mcpMessageNeedsEngine(message)) {
    (void)ensureMcpEngine();
  }
  Blunder::MachineAdapterHost host;
  if (g_engine) {
    host = makeHost();
  } else {
    host.project_root = g_launch.project_root;
  }
  const std::string reply =
      Blunder::mcpHandleMessage(message, g_launch, host);
  if (!reply.empty()) {
    Blunder::mcpWriteMessage(reply);
  }
}

bool pumpMcp(bool block_for_one) {
  if (!block_for_one) {
    if (Blunder::mcpStdinClosed()) {
      return false;
    }
    if (!Blunder::mcpStdinHasBytes()) {
      return true;
    }
  }
  std::string message;
  if (!Blunder::mcpReadMessage(message)) {
    return false;
  }
  dispatchMcp(message);
  return true;
}

}  // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
  try {
    Blunder::mcpCaptureStdioHandles();
    g_launch = Blunder::resolveEditorSessionLaunch(
        argc, argv, isDebugLaunchBuild(), compiledProjectRoot());
    if (!g_launch.ok) {
      if (!g_launch.failure_code.empty()) {
        Blunder::MachineResult failed;
        failed.failure_code = g_launch.failure_code;
        failed.exit_code = 1;
        std::cout << Blunder::machineResultJson(failed) << '\n';
      }
      std::cerr << g_launch.error.c_str() << '\n';
      return SDL_APP_FAILURE;
    }

    g_mcp = g_launch.adapter == Blunder::MachineAdapterKind::mcp;
    if (g_mcp) {
      // Cursor closes stdio if JSON-RPC `initialize` is later than ~1.1s.
      // Answer handshake before Slang/project/GPU boot.
      *appstate = nullptr;
      while (!g_mcp_saw_initialize) {
        if (!pumpMcp(true)) {
          return SDL_APP_FAILURE;
        }
      }
      while (Blunder::mcpStdinHasBytes()) {
        if (!pumpMcp(false)) {
          break;
        }
      }
      return SDL_APP_CONTINUE;
    }

    g_engine = new Blunder::BlunderEngine();
    *appstate = g_engine;
    g_engine->startEngine(g_launch.project_root, Blunder::EngineHostMode::Editor,
                          {}, g_launch.headless);
    if (Blunder::g_runtime_global_context.isQuitRequested()) {
      return SDL_APP_SUCCESS;
    }
    const bool use_startup =
        g_launch.adapter == Blunder::MachineAdapterKind::none;
    g_engine->initialize(g_launch.scene, use_startup);

    if (g_launch.adapter == Blunder::MachineAdapterKind::cli) {
      for (int i = 0; i < 8; ++i) {
        g_engine->tickOneFrame(g_engine->calculateDeltaTime());
      }
      Blunder::MachineAdapterHost host = makeHost();
      Blunder::MachineResult result;
      Blunder::dispatchMachineAdapter(g_launch, host, result);
      std::cout << Blunder::machineResultJson(result) << '\n';
      return result.exit_code == 0 ? SDL_APP_SUCCESS : SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
  } catch (const std::exception& e) {
    std::cerr << "SDL_AppInit failed: " << e.what() << std::endl;
    return SDL_APP_FAILURE;
  } catch (...) {
    std::cerr << "SDL_AppInit failed: unknown exception" << std::endl;
    return SDL_APP_FAILURE;
  }
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto* engine = static_cast<Blunder::BlunderEngine*>(appstate);
  if (!engine) {
    engine = g_engine;
  }
  try {
    if (g_mcp) {
      if (Blunder::mcpStdinClosed()) {
        return SDL_APP_SUCCESS;
      }
      while (Blunder::mcpStdinHasBytes()) {
        if (!pumpMcp(false)) {
          return SDL_APP_SUCCESS;
        }
      }
      if (g_mcp_saw_tools_list) {
        (void)ensureMcpEngine();
        engine = g_engine;
      }
      if (!engine) {
        SDL_Delay(5);
        return SDL_APP_CONTINUE;
      }
    }
    if (!engine) {
      return SDL_APP_FAILURE;
    }
    const float delta_time = engine->calculateDeltaTime();
    static bool s_logged_first_tick = false;
    if (!s_logged_first_tick) {
      mcpBreadcrumb("tickOneFrame begin");
    }
    if (!engine->tickOneFrame(delta_time)) {
      return SDL_APP_SUCCESS;
    }
    if (!s_logged_first_tick) {
      mcpBreadcrumb("tickOneFrame done");
      s_logged_first_tick = true;
    }
    return SDL_APP_CONTINUE;
  } catch (const std::exception& e) {
    std::cerr << "SDL_AppIterate failed: " << e.what() << std::endl;
    return SDL_APP_FAILURE;
  } catch (...) {
    std::cerr << "SDL_AppIterate failed: unknown exception" << std::endl;
    return SDL_APP_FAILURE;
  }
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  if (!event) {
    return SDL_APP_CONTINUE;
  }
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }

  auto* engine = static_cast<Blunder::BlunderEngine*>(appstate);
  if (!engine) {
    engine = g_engine;
  }
  if (!engine) {
    return SDL_APP_CONTINUE;
  }
  try {
    engine->processSdlEvent(*event);
  } catch (const std::exception& e) {
    std::cerr << "SDL_AppEvent failed: " << e.what() << std::endl;
    return SDL_APP_FAILURE;
  } catch (...) {
    std::cerr << "SDL_AppEvent failed: unknown exception" << std::endl;
    return SDL_APP_FAILURE;
  }
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  (void)result;
  auto* engine = static_cast<Blunder::BlunderEngine*>(appstate);
  if (!engine) {
    engine = g_engine;
  }
  if (engine) {
    try {
      engine->clear();
      engine->shutdownEngine();
    } catch (...) {
    }
    delete engine;
  }
  g_engine = nullptr;
}
