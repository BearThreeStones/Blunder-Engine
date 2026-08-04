#include "runtime/resource/asset/asset_yaml.h"

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void roundTripAnimationClipMethodKeys() {
  using namespace Blunder;

  AnimationClipData in;
  in.name = "Events";
  in.duration = 1.0f;

  AnimationTrack track;
  track.bone = "Hips";
  track.channel = AnimationChannel::Translation;
  track.interpolation = AnimationInterpolation::Constant;
  track.keys.push_back({0.0f, {0.0f, 0.0f, 0.0f}});
  track.keys.push_back({1.0f, {0.0f, 0.0f, 0.0f}});
  in.tracks.push_back(track);

  AnimationMethodKey key;
  key.name = "Footstep";
  key.time = 0.5f;
  key.args.push_back(1.5f);
  in.method_keys.push_back(key);

  const eastl::string yaml = AssetYaml::serializeAnimationClipData(in);
  expect_true("yaml contains method_keys",
              yaml.find("method_keys") != eastl::string::npos);

  AnimationClipData out;
  expect_true("parse method_keys yaml",
              AssetYaml::parseAnimationClipData(yaml, out));
  expect_true("method_keys size", out.method_keys.size() == 1);
  expect_true("method key name", out.method_keys[0].name == "Footstep");
  expect_true("method key time", out.method_keys[0].time == 0.5f);
  expect_true("method key arg",
              out.method_keys[0].args.size() == 1 &&
                  out.method_keys[0].args[0] == 1.5f);
}

}  // namespace

int main() {
  roundTripAnimationClipMethodKeys();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
