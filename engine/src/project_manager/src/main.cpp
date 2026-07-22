#include <exception>
#include <iostream>

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include "runtime/project/project_manager_app.h"

namespace {

Blunder::ProjectManagerApp* g_project_manager = nullptr;

}  // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
  (void)argc;
  (void)argv;
  try {
    g_project_manager = new Blunder::ProjectManagerApp();
    *appstate = g_project_manager;
    if (!g_project_manager->start()) {
      std::cerr << "Project Manager failed to start\n";
      return SDL_APP_FAILURE;
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
  auto* app = static_cast<Blunder::ProjectManagerApp*>(appstate);
  if (!app) {
    return SDL_APP_FAILURE;
  }
  try {
    return app->tick();
  } catch (const std::exception& e) {
    std::cerr << "Project Manager tick failed: " << e.what() << std::endl;
    return SDL_APP_FAILURE;
  } catch (...) {
    std::cerr << "Project Manager tick failed: unknown exception\n";
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

  auto* app = static_cast<Blunder::ProjectManagerApp*>(appstate);
  if (!app) {
    return SDL_APP_CONTINUE;
  }
  try {
    app->processSdlEvent(*event);
  } catch (const std::exception& e) {
    std::cerr << "Project Manager event failed: " << e.what() << std::endl;
    return SDL_APP_FAILURE;
  } catch (...) {
    std::cerr << "Project Manager event failed: unknown exception\n";
    return SDL_APP_FAILURE;
  }
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  (void)result;
  auto* app = static_cast<Blunder::ProjectManagerApp*>(appstate);
  if (!app) {
    app = g_project_manager;
  }
  if (app) {
    try {
      app->shutdown();
    } catch (...) {
    }
    delete app;
  }
  g_project_manager = nullptr;
}
