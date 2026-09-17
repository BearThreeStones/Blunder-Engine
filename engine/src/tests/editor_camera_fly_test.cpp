#include "runtime/function/render/editor_camera.h"

#include "runtime/core/event/mouse_event.h"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include <cstdio>
#include <cmath>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  EditorCamera camera(nullptr);
  expect_true("no button is not viewport interacting",
              !camera.isViewportInteracting());

  const Vec3 before = camera.getFocalPoint();
  camera.onUpdate(0.25f);
  expect_true("hover without RMB/MMB does not fly",
              before == camera.getFocalPoint());

  camera.setViewportRect(0, 0, 1280.0f, 720.0f, 1280.0f, 720.0f);

  {
    const Vec3 pos_before = camera.getPosition();
    const Vec3 focal_before = camera.getFocalPoint();
    MouseButtonPressedEvent press(SDL_BUTTON_RIGHT, 100.0f, 100.0f);
    camera.onEvent(press);
    expect_true("viewport RMB starts interacting", camera.isViewportInteracting());
    MouseMovedEvent move(120.0f, 90.0f, 20.0f, -10.0f);
    camera.onEvent(move);
    camera.onUpdate(1.0f / 60.0f);
    expect_true("RMB orbit keeps the camera origin",
                glm::length(camera.getPosition() - pos_before) < 1e-3f);
    expect_true("RMB orbit moves the look target around the camera",
                glm::length(camera.getFocalPoint() - focal_before) > 1e-3f);
    MouseButtonReleasedEvent release(SDL_BUTTON_RIGHT, 120.0f, 90.0f);
    camera.onEvent(release);
    camera.onUpdate(1.0f / 60.0f);
    expect_true("RMB mouse-up ends viewport interacting",
                !camera.isViewportInteracting());
  }

  {
    camera.snapLookAt(Vec3(12.0f, 12.0f, 12.0f), Vec3(0.0f, 0.0f, 0.0f));
    MouseButtonPressedEvent press(SDL_BUTTON_MIDDLE, 100.0f, 100.0f);
    camera.onEvent(press);
    camera.onUpdate(1.0f / 60.0f);
    expect_true("viewport MMB starts interacting", camera.isViewportInteracting());
    const Vec3 pos_before = camera.getPosition();
    const float dist_before = glm::length(pos_before);
    MouseMovedEvent move(140.0f, 80.0f, 40.0f, -20.0f);
    camera.onEvent(move);
    camera.onUpdate(1.0f / 60.0f);
    expect_true("MMB orbits around the scene origin",
                glm::length(camera.getFocalPoint()) < 1e-3f);
    expect_true("MMB origin orbit moves the camera",
                pos_before != camera.getPosition());
    expect_true("MMB origin orbit keeps distance to origin",
                std::fabs(glm::length(camera.getPosition()) - dist_before) <
                    1e-2f);
    MouseButtonReleasedEvent release(SDL_BUTTON_MIDDLE, 140.0f, 80.0f);
    camera.onEvent(release);
    camera.onUpdate(1.0f / 60.0f);
    expect_true("MMB mouse-up ends viewport interacting",
                !camera.isViewportInteracting());
  }

  {
    camera.snapLookAt(Vec3(20.0f, 8.0f, 6.0f), Vec3(12.0f, 4.0f, 3.0f));
    const Vec3 pos_before = camera.getPosition();
    const Vec3 focal_before = camera.getFocalPoint();
    const float dist_before = glm::length(pos_before);
    MouseButtonPressedEvent press(SDL_BUTTON_MIDDLE, 100.0f, 100.0f);
    camera.onEvent(press);
    camera.onUpdate(1.0f / 60.0f);
    expect_true("MMB down does not teleport the eye",
                glm::length(camera.getPosition() - pos_before) < 1e-3f);
    expect_true("MMB down does not re-target look-at onto the origin",
                glm::length(camera.getFocalPoint() - focal_before) < 1e-3f);
    MouseMovedEvent move(160.0f, 70.0f, 60.0f, -30.0f);
    camera.onEvent(move);
    camera.onUpdate(1.0f / 60.0f);
    expect_true("MMB drag keeps distance to world origin",
                std::fabs(glm::length(camera.getPosition()) - dist_before) <
                    1e-2f);
    expect_true("MMB drag does not snap look-at to origin",
                glm::length(camera.getFocalPoint()) > 1.0f);
    expect_true("MMB drag moves the eye around origin",
                glm::length(camera.getPosition() - pos_before) > 1e-3f);
    MouseButtonReleasedEvent release(SDL_BUTTON_MIDDLE, 160.0f, 70.0f);
    camera.onEvent(release);
    camera.onUpdate(1.0f / 60.0f);
  }

  {
    SDL_SetModState(SDL_KMOD_LSHIFT);
    const Vec3 focal_before = camera.getFocalPoint();
    MouseButtonPressedEvent press(SDL_BUTTON_MIDDLE, 100.0f, 100.0f);
    camera.onEvent(press);
    MouseMovedEvent move(130.0f, 70.0f, 30.0f, -30.0f);
    camera.onEvent(move);
    camera.onUpdate(1.0f / 60.0f);
    expect_true("Shift+MMB pans the look target",
                glm::length(camera.getFocalPoint() - focal_before) > 1e-4f);
    MouseButtonReleasedEvent release(SDL_BUTTON_MIDDLE, 130.0f, 70.0f);
    camera.onEvent(release);
    SDL_SetModState(SDL_KMOD_NONE);
    camera.onUpdate(1.0f / 60.0f);
    expect_true("Shift+MMB mouse-up ends viewport interacting",
                !camera.isViewportInteracting());
  }

  {
    EditorCamera zoom_cam(nullptr);
    zoom_cam.setViewportRect(0, 0, 1280.0f, 720.0f, 1280.0f, 720.0f);
    expect_true("editor far clip reaches centimetre Sponza",
                zoom_cam.getFarClip() > 10000.0f);
    const float start = zoom_cam.getDistance();
    for (int i = 0; i < 120; ++i) {
      MouseScrolledEvent scroll(0.0f, -1.0f, 100.0f, 100.0f);
      zoom_cam.onEvent(scroll);
      zoom_cam.onUpdate(1.0f / 60.0f);
    }
    expect_true("wheel zoom-out exceeds old 2000 metre cap",
                zoom_cam.getDistance() > 2000.0f);
    expect_true("wheel zoom-out can frame full Sponza",
                zoom_cam.getDistance() >= 8000.0f);
    expect_true("wheel zoom-out still has an upper cap",
                zoom_cam.getDistance() <= 50000.0f + 1.0f);
    expect_true("zoom-out moved farther than start",
                zoom_cam.getDistance() > start);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stderr, "editor_camera_fly_test: all passed\n");
  return 0;
}
