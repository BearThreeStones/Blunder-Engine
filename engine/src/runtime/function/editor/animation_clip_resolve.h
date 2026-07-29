#pragma once

#include "EASTL/string.h"

#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

class AnimationPlayer;

/// Resolve AnimationClip Intermediate YAML by GUID via AssetManager + registry.
bool resolveAnimationClipFromAssets(void* /*userdata*/, const eastl::string& guid,
                                    AnimationClipData& out_clip);

/// Installs the asset-backed clip resolver on a player (idempotent).
void wireAnimationPlayerAssetResolver(AnimationPlayer& player);

}  // namespace Blunder
