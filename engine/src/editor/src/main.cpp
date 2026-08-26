#include <exception>
#include <filesystem>
#include <iostream>

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

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
  host.pump = []() {
    if (g_engine) {
      g_engine->tickOneFrame(1.0f / 60.0f);
    }
  };
  return host;
}

}  // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
  try {
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

    g_engine = new Blunder::BlunderEngine();
    *appstate = g_engine;
    g_engine->startEngine(g_launch.project_root, Blunder::EngineHostMode::Editor,
                          {}, g_launch.headless);
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

    g_mcp = g_launch.adapter == Blunder::MachineAdapterKind::mcp;
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
    return SDL_APP_FAILURE;
  }
  try {
    if (g_mcp && Blunder::mcpStdinHasBytes()) {
      std::string message;
      if (Blunder::mcpReadMessage(message)) {
        Blunder::MachineAdapterHost host = makeHost();
        const std::string reply =
            Blunder::mcpHandleMessage(message, g_launch, host);
        if (!reply.empty()) {
          Blunder::mcpWriteMessage(reply);
        }
      }
    }
    const float delta_time = engine->calculateDeltaTime();
    if (!engine->tickOneFrame(delta_time)) {
      return SDL_APP_SUCCESS;
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
