#include "runtime/function/editor/inspector_transform_ops.h"

#include <cmath>
#include <cstdio>

#include <glm/gtc/quaternion.hpp>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) <= eps;
}

bool near3(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f) {
  return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    const glm::vec3 linked =
        applyScaleLinkEdit(glm::vec3(2.0f, 1.0f, 0.5f), 0, 4.0f);
    expect_true("scale link doubles all axes",
                near3(linked, glm::vec3(4.0f, 2.0f, 1.0f)));
  }

  {
    const glm::vec3 linked =
        applyScaleLinkEdit(glm::vec3(0.0f, 1.0f, 1.0f), 0, 2.0f);
    expect_true("near-zero edited axis does not propagate",
                near3(linked, glm::vec3(2.0f, 1.0f, 1.0f)));
  }

  {
    expect_true("components equal => not mixed", !isMixedComponent(1.0f, 1.0f));
    expect_true("components differ => mixed", isMixedComponent(1.0f, 2.0f));
  }

  {
    float a = 1.0f;
    float b = 10.0f;
    applyAbsoluteComponent(5.0f, a);
    applyAbsoluteComponent(5.0f, b);
    expect_true("absolute sets both", near(a, 5.0f) && near(b, 5.0f));
  }

  {
    float a = 1.0f;
    float b = 10.0f;
    applyDeltaComponent(2.0f, a);
    applyDeltaComponent(2.0f, b);
    expect_true("delta adds both", near(a, 3.0f) && near(b, 12.0f));
  }

  {
    const Quat q0 = glm::identity<Quat>();
    const Quat q1 = applyEulerDeltaDegrees(q0, glm::vec3(10.0f, 0.0f, 0.0f));
    expect_true("euler delta changes rotation",
                std::fabs(glm::dot(q0, q1)) < 0.999f);
  }

  {
    Quat q(2.0f, 0.0f, 0.0f, 0.0f);
    q = normalizeQuaternion(q);
    expect_true("normalize unit length", near(glm::length(q), 1.0f));
  }

  {
    const Quat q0 = glm::identity<Quat>();
    const Quat q1 = applyEulerDeltaDegrees(q0, glm::vec3(10.0f, 0.0f, 0.0f));
    const Quat q2 = applyEulerDeltaDegrees(q1, glm::vec3(-10.0f, 0.0f, 0.0f));
    expect_true("euler delta cancel returns near identity",
                near(std::fabs(glm::dot(q0, q2)), 1.0f, 1e-3f));
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "inspector_transform_ops_test: %d failure(s)\n",
                 g_failures);
    return 1;
  }
  std::printf("inspector_transform_ops_test: all passed\n");
  return 0;
}
