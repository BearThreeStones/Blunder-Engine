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

  {
    EditorCamera cam(nullptr);
    cam.setViewportRect(0, 0, 1280.0f, 720.0f, 1280.0f, 720.0f);
    cam.snapLookAt(Vec3(20.0f, 8.0f, 6.0f), Vec3(12.0f, 4.0f, 3.0f));
    const Vec3 pos_before = cam.getPosition();
    const float dist_before = glm::length(pos_before);
    cam.orbitAroundWorldOrigin(Vec2(60.0f, -30.0f));
    expect_true("API origin orbit keeps distance to world origin",
                std::fabs(glm::length(cam.getPosition()) - dist_before) < 1e-2f);
    expect_true("API origin orbit does not snap look-at to origin",
                glm::length(cam.getFocalPoint()) > 1.0f);
    expect_true("API origin orbit moves the eye",
                glm::length(cam.getPosition() - pos_before) > 1e-3f);

    const Vec3 rmb_eye = cam.getPosition();
    const Vec3 rmb_focal = cam.getFocalPoint();
    cam.orbitAroundCamera(Vec2(20.0f, -10.0f));
    expect_true("API RMB orbit keeps the camera origin",
                glm::length(cam.getPosition() - rmb_eye) < 1e-3f);
    expect_true("API RMB orbit moves the look target",
                glm::length(cam.getFocalPoint() - rmb_focal) > 1e-3f);

    const Vec3 pan_focal = cam.getFocalPoint();
    cam.panByMouseDelta(Vec2(30.0f, -30.0f));
    expect_true("API pan moves the look target",
                glm::length(cam.getFocalPoint() - pan_focal) > 1e-4f);

    const float zoom_before = cam.getDistance();
    cam.zoomByWheel(-8.0f);
    expect_true("API wheel zoom-out increases distance",
                cam.getDistance() > zoom_before);
  }

  {
    EditorCamera frame_cam(nullptr);
    frame_cam.setViewportRect(0, 0, 1280.0f, 720.0f, 1280.0f, 720.0f);
    AABB sponza{};
    sponza.min = Vec3(-1921.0f, -1105.0f, -126.0f);
    sponza.max = Vec3(1800.0f, 1183.0f, 1429.0f);
    frame_cam.snapFocusOnAABB(sponza);
    const Vec3 eye = frame_cam.getPosition();
    const Vec3 focal = frame_cam.getFocalPoint();
    const float radius = glm::length(sponza.extents());
    expect_true("Sponza snap looks at courtyard center",
                glm::length(focal - sponza.center()) < 1.0f);
    expect_true("Sponza snap keeps the eye above the roof",
                eye.z > sponza.max.z);
    expect_true("Sponza snap distance frames the whole mesh",
                frame_cam.getDistance() >= radius * 3.0f);
    const Mat4 vp = frame_cam.getProjectionMatrix() * frame_cam.getViewMatrix();
    const Vec3 corners[8] = {
        sponza.min,
        Vec3(sponza.max.x, sponza.min.y, sponza.min.z),
        Vec3(sponza.min.x, sponza.max.y, sponza.min.z),
        Vec3(sponza.max.x, sponza.max.y, sponza.min.z),
        Vec3(sponza.min.x, sponza.min.y, sponza.max.z),
        Vec3(sponza.max.x, sponza.min.y, sponza.max.z),
        Vec3(sponza.min.x, sponza.max.y, sponza.max.z),
        sponza.max,
    };
    bool all_in_view = true;
    for (const Vec3& corner : corners) {
      const glm::vec4 clip = vp * glm::vec4(corner, 1.0f);
      if (clip.w <= 1e-4f) {
        all_in_view = false;
        break;
      }
      const glm::vec3 ndc = glm::vec3(clip) / clip.w;
      if (ndc.x < -1.15f || ndc.x > 1.15f || ndc.y < -1.15f || ndc.y > 1.15f) {
        all_in_view = false;
        break;
      }
    }
    expect_true("Sponza snap keeps every AABB corner in the Viewport", all_in_view);
  }

  {
    // Chocomel / dog-scale sphere (~1 m radius). Old floor of 10 m never zoomed in.
    EditorCamera dog_cam(nullptr);
    dog_cam.setViewportRect(0, 0, 1280.0f, 720.0f, 1280.0f, 720.0f);
    dog_cam.snapLookAt(Vec3(40.0f, -30.0f, 20.0f), Vec3(0.0f, 0.0f, 0.0f));
    const float dist_before = dog_cam.getDistance();
    AABB dog{};
    dog.min = Vec3(-0.5f, -0.5f, 0.0f);
    dog.max = Vec3(0.5f, 0.5f, 1.5f);
    dog_cam.snapFocusOnAABB(dog);
    const float radius = glm::length(dog.extents());
    expect_true("Dog snap looks at selection center",
                glm::length(dog_cam.getFocalPoint() - dog.center()) < 1e-3f);
    expect_true("Dog snap zooms closer than world-scale orbit",
                dog_cam.getDistance() < dist_before &&
                    dog_cam.getDistance() < 8.0f);
    expect_true("Dog snap distance tracks selection radius",
                dog_cam.getDistance() >= radius * 2.0f &&
                    dog_cam.getDistance() <= radius * 3.5f);
  }

  {
    EditorCamera zoom_cam(nullptr);
    zoom_cam.snapLookAt(Vec3(-5500.0f, -6200.0f, 4200.0f),
                        Vec3(-60.0f, 40.0f, 650.0f));
    expect_true("LOOKAT near grows with orbit so depth is not sky",
                zoom_cam.getNearClip() > 10.0f &&
                    zoom_cam.getNearClip() < zoom_cam.getFarClip() * 0.5f);
    const Mat4 vp =
        zoom_cam.getProjectionMatrix() * zoom_cam.getViewMatrix();
    const glm::vec4 clip = vp * glm::vec4(-60.0f, 40.0f, 650.0f, 1.0f);
    const float ndc_z = clip.z / clip.w;
    expect_true("LOOKAT courtyard ndc.z is not the far-plane sky",
                clip.w > 1e-4f && ndc_z > 0.0f && ndc_z < 0.999f);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stderr, "editor_camera_fly_test: all passed\n");
  return 0;
}
