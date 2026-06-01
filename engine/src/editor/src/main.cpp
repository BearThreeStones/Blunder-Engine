#include <exception>
#include <iostream>

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include "runtime/engine.h"

namespace {

Blunder::BlunderEngine* g_engine = nullptr;

}  // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
  (void)argc;
  (void)argv;
  try {
    g_engine = new Blunder::BlunderEngine();
    *appstate = g_engine;
    g_engine->startEngine();
    g_engine->initialize();
    return SDL_APP_CONTINUE;
  } catch (const std::exception& e) {
    std::cerr << "SDL_AppInit failed: " << e.what() << std::endl;
    // Do NOT delete g_engine here.  SDL always calls SDL_AppQuit after
    // SDL_AppInit returns (even on failure).  SDL_AppQuit performs orderly
    // shutdown (shutdownEngine → shutdownSystems) and then deletes the
    // engine.  Deleting here would skip shutdownSystems(), leaving the
    // global context with live shared_ptrs that crash during static
    // destruction.
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
  auto* engine = static_cast<Blunder::BlunderEngine*>(appstate);
  if (!engine || !event) {
    return SDL_APP_CONTINUE;
  }
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
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
