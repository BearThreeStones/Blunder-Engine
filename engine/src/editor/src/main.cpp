#include <exception>
#include <filesystem>
#include <iostream>

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include "runtime/engine.h"
#include "runtime/project/editor_launch.h"

namespace {

Blunder::BlunderEngine* g_engine = nullptr;

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

}  // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
  try {
    const Blunder::EditorSessionLaunch launch =
        Blunder::resolveEditorSessionLaunch(argc, argv, isDebugLaunchBuild(),
                                            compiledProjectRoot());
    if (!launch.ok) {
      std::cerr << launch.error.c_str() << '\n';
      return SDL_APP_FAILURE;
    }

    g_engine = new Blunder::BlunderEngine();
    *appstate = g_engine;
    g_engine->startEngine(launch.project_root);
    g_engine->initialize();
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
