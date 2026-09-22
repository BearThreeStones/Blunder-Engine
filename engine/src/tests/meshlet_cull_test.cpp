#include "runtime/function/render/gpu_driven/gpu_driven_cull.h"

#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

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

  glm::mat4 view = glm::lookAt(glm::vec3(0.0f, -8.0f, 0.0f), glm::vec3(0.0f),
                               glm::vec3(0.0f, 0.0f, 1.0f));
  glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
  glm::mat4 vp = projection * view;
  glm::vec4 planes[6];
  extractFrustumPlanes(vp, planes);

  expect_true("origin sphere in frustum",
              sphereInsideFrustum(glm::vec3(0.0f), 0.5f, planes));
  expect_true("far behind camera rejected",
              !sphereInsideFrustum(glm::vec3(0.0f, -80.0f, 0.0f), 0.5f, planes));
  expect_true("side sphere rejected",
              !sphereInsideFrustum(glm::vec3(80.0f, 0.0f, 0.0f), 0.1f, planes));

  const glm::vec3 camera(0.0f, -8.0f, 0.0f);
  expect_true("front-facing cone accepted",
              !meshletConeCulled(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
                                 0.1f, 0.5f, camera));
  expect_true("back-facing cone rejected",
              meshletConeCulled(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                                0.1f, 0.5f, camera));
  expect_true("grazing cone with large radius kept",
              !meshletConeCulled(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                                 0.1f, 10.0f, camera));

  expect_true("hiz behind occluder",
              meshletHiZOccluded(glm::vec4(0.0f, 0.0f, 0.9f, 1.0f), 0.01f, 0.2f));
  expect_true("hiz in front of occluder",
              !meshletHiZOccluded(glm::vec4(0.0f, 0.0f, 0.1f, 1.0f), 0.01f, 0.5f));
  expect_true("far meshlet skips previous-frame Hi-Z",
              meshletSkipHiZWhenFar(5000.0f, 50.0f));
  expect_true("LOOKAT Sponza distance skips previous-frame Hi-Z",
              meshletSkipHiZWhenFar(9000.0f, 200.0f));
  expect_true("courtyard meshlet keeps Hi-Z",
              !meshletSkipHiZWhenFar(80.0f, 50.0f));

  const uint32_t compact[4] = {3u, 0u, 11u, 2u};
  expect_true("sum compact counts",
              sumCompactCountBuffer(compact, 4) == 16u);
  expect_true("sum compact null is 0",
              sumCompactCountBuffer(nullptr, 4) == 0u);
  expect_true("sum compact empty is 0",
              sumCompactCountBuffer(compact, 0) == 0u);
  expect_true("unique local wraps instance-major expanded",
              uniqueMeshletLocal(5u, 3u) == 2u);
  expect_true("unique index is first + local",
              uniqueMeshletIndex(10u, 5u, 3u) == 12u);
  expect_true("unique local zero count", uniqueMeshletLocal(4u, 0u) == 0u);

  glm::mat4 sponza_view = glm::lookAt(glm::vec3(0.0f, -1500.0f, 8000.0f),
                                     glm::vec3(0.0f, 0.0f, 500.0f),
                                     glm::vec3(0.0f, 0.0f, 1.0f));
  glm::mat4 sponza_proj = glm::perspectiveZO(glm::radians(45.0f), 1.4f, 0.1f,
                                            100000.0f);
  sponza_proj[1][1] *= -1.0f;
  glm::vec4 sponza_planes[6];
  extractFrustumPlanes(sponza_proj * sponza_view, sponza_planes);
  expect_true("overhead ZO frustum keeps Sponza origin",
              sphereInsideFrustum(glm::vec3(0.0f, 0.0f, 650.0f), 200.0f,
                                  sponza_planes));
  expect_true("overhead ZO frustum keeps south wing",
              sphereInsideFrustum(glm::vec3(0.0f, -1100.0f, 1400.0f), 200.0f,
                                  sponza_planes));
  expect_true("overhead ZO frustum keeps north wing",
              sphereInsideFrustum(glm::vec3(0.0f, 1180.0f, 1400.0f), 200.0f,
                                  sponza_planes));

  if (g_failures != 0) {
    std::fprintf(stderr, "%d test(s) failed\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "meshlet_cull_test: all passed\n");
  return 0;
}
