#include "runtime/function/render/vrs/vrs_rate.h"

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

void expect_float(const char* label, float got, float want) {
  if (std::fabs(got - want) > 1e-5f) {
    std::fprintf(stderr, "FAIL %s got %f want %f\n", label, got, want);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  expect_float("rec709 white", rec709Luminance(1.0f, 1.0f, 1.0f), 1.0f);
  expect_float("rec709 red", rec709Luminance(1.0f, 0.0f, 0.0f), 0.2126f);
  expect_float("rec709 green", rec709Luminance(0.0f, 1.0f, 0.0f), 0.7152f);
  expect_float("rec709 blue", rec709Luminance(0.0f, 0.0f, 1.0f), 0.0722f);

  expect_true("pack 1x1", packFragmentShadingRate(0, 0) == k_vrs_texel_1x1);
  expect_true("pack 2x2", packFragmentShadingRate(1, 1) == k_vrs_texel_2x2);

  uint32_t w = 0;
  uint32_t h = 0;
  unpackFragmentShadingRate(k_vrs_texel_1x1, &w, &h);
  expect_true("unpack 1x1", w == 1 && h == 1);
  unpackFragmentShadingRate(k_vrs_texel_2x2, &w, &h);
  expect_true("unpack 2x2", w == 2 && h == 2);

  expect_true("edge encodes 1x1",
              encodeSobelShadingRate(0.11f) == k_vrs_texel_1x1);
  expect_true("flat encodes 2x2",
              encodeSobelShadingRate(0.1f) == k_vrs_texel_2x2);
  expect_true("zero encodes 2x2", encodeSobelShadingRate(0.0f) == k_vrs_texel_2x2);

  float edge[9] = {0, 0, 1, 0, 0, 1, 0, 0, 1};
  expect_true("sobel vertical edge > 0.1",
              sobelGradientSq(edge) > k_vrs_sobel_threshold);
  float flat[9] = {0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f};
  expect_true("sobel flat <= 0.1",
              sobelGradientSq(flat) <= k_vrs_sobel_threshold);

  uint32_t texel_w = 99;
  uint32_t texel_h = 99;
  chooseFragmentShadingRateTexelSize(1, 1, 16, 16, &texel_w, &texel_h);
  expect_true("choose 1x1 when in range", texel_w == 1 && texel_h == 1);
  chooseFragmentShadingRateTexelSize(8, 8, 32, 32, &texel_w, &texel_h);
  expect_true("choose min when 1x1 not allowed", texel_w == 8 && texel_h == 8);
  chooseFragmentShadingRateTexelSize(16, 16, 16, 16, &texel_w, &texel_h);
  expect_true("choose min when only min", texel_w == 16 && texel_h == 16);

  expect_true("wanted needs all flags",
              fragmentShadingRateWanted(true, true, true, true));
  expect_true("missing extension not wanted",
              !fragmentShadingRateWanted(false, true, true, true));
  expect_true("missing attachment not wanted",
              !fragmentShadingRateWanted(true, false, true, true));
  expect_true("missing pipeline rate not wanted",
              !fragmentShadingRateWanted(true, true, false, true));
  expect_true("missing renderpass2 not wanted",
              !fragmentShadingRateWanted(true, true, true, false));

  if (g_failures != 0) {
    std::fprintf(stderr, "vrs_rate_test: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("vrs_rate_test: all passed\n");
  return 0;
}
