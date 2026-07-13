#include "runtime/core/reflection/variant.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool near(float a, float b, float eps = 1e-5f) {
  return std::fabs(a - b) <= eps;
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    const Variant v(true);
    expect_true("bool type", v.getType() == VariantType::Bool);
    expect_true("bool value", v.asBool());
  }
  {
    const Variant v(static_cast<int64_t>(42));
    expect_true("int type", v.getType() == VariantType::Int);
    expect_true("int value", v.asInt() == 42);
  }
  {
    const Variant v(1.5f);
    expect_true("float type", v.getType() == VariantType::Float);
    expect_true("float value", near(v.asFloat(), 1.5f));
  }
  {
    const Variant v(Vec3(1.0f, 2.0f, 3.0f));
    expect_true("vec3 type", v.getType() == VariantType::Vec3);
    expect_true("vec3 value", v.asVec3() == Vec3(1.0f, 2.0f, 3.0f));
  }
  {
    const Quat q = glm::identity<Quat>();
    const Variant v(q);
    expect_true("quat type", v.getType() == VariantType::Quat);
    expect_true("quat equal", v.asQuat() == q);
  }
  {
    const Variant v(eastl::string("hello"));
    expect_true("string type", v.getType() == VariantType::String);
    expect_true("string value", v.asString() == "hello");
  }
  {
    expect_true("equal variants",
                Variant(static_cast<int64_t>(7)) ==
                    Variant(static_cast<int64_t>(7)));
    expect_true("unequal variants",
                !(Variant(true) == Variant(static_cast<int64_t>(1))));
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
