#include <exception>
#include <iostream>
#include <memory>

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include "runtime/engine.h"
#include "runtime/function/global/engine_host_mode.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/window/window_system.h"
#include "runtime/project/play_ipc.h"
#include "runtime/project/player_launch.h"

namespace {

Blunder::BlunderEngine* g_engine = nullptr;
Blunder::PlayerLaunch g_launch{};
std::unique_ptr<Blunder::PlayIpcClient> g_play_ipc;

void handlePlayIpcCommand(Blunder::PlayIpcCommand command) {
  using Blunder::PlayIpcCommand;
  switch (command) {
    case PlayIpcCommand::Pause:
      Blunder::g_runtime_global_context.setPlayPaused(true);
      break;
    case PlayIpcCommand::Resume:
      Blunder::g_runtime_global_context.setPlayPaused(false);
      break;
    case PlayIpcCommand::Stop:
      if (Blunder::g_runtime_global_context.m_window_system) {
        Blunder::g_runtime_global_context.m_window_system->requestClose();
      }
      break;
    case PlayIpcCommand::Unknown:
    default:
      break;
  }
}

bool connectPlayIpc(const Blunder::PlayIpcEndpoint& endpoint) {
  g_play_ipc = std::make_unique<Blunder::PlayIpcClient>();
  // Editor already holds the listen socket; retry briefly while Starting.
  for (int attempt = 0; attempt < 50; ++attempt) {
    if (g_play_ipc->connect(endpoint)) {
      g_play_ipc->setCommandHandler(handlePlayIpcCommand);
      return true;
    }
    SDL_Delay(20);
  }
  g_play_ipc.reset();
  return false;
}

}  // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
  try {
    g_launch = Blunder::parsePlayerLaunch(argc, argv);
    if (!g_launch.ok) {
      std::cerr << g_launch.error.c_str() << '\n';
      return SDL_APP_FAILURE;
    }

    Blunder::PlayIpcEndpoint ipc_endpoint{};
    if (!g_launch.play_ipc.empty()) {
      ipc_endpoint = Blunder::parsePlayIpcEndpoint(g_launch.play_ipc);
      if (!ipc_endpoint.ok) {
        std::cerr << "[engine_player] invalid --play-ipc: "
                  << ipc_endpoint.error.c_str() << '\n';
        return SDL_APP_FAILURE;
      }
    }

    g_engine = new Blunder::BlunderEngine();
    *appstate = g_engine;
    g_engine->startEngine(g_launch.project_root, Blunder::EngineHostMode::Player,
                          eastl::string(g_launch.scene.c_str()));
    g_engine->initialize(eastl::string(g_launch.scene.c_str()));

    if (ipc_endpoint.ok) {
      if (!connectPlayIpc(ipc_endpoint)) {
        std::cerr << "[engine_player] failed to connect play-ipc "
                  << Blunder::formatPlayIpcEndpoint(ipc_endpoint).c_str()
                  << '\n';
        return SDL_APP_FAILURE;
      }
      if (!g_play_ipc->announceReady()) {
        std::cerr << "[engine_player] failed to announce play-ipc ready\n";
        return SDL_APP_FAILURE;
      }
      std::cerr << "[engine_player] play IPC ready on "
                << Blunder::formatPlayIpcEndpoint(ipc_endpoint).c_str() << '\n';
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
    return SDL_APP_FAILURE;
  }
  try {
    if (g_play_ipc) {
      g_play_ipc->poll();
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
  g_play_ipc.reset();
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
