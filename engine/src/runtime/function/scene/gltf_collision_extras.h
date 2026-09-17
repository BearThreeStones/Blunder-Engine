#pragma once

#include "runtime/function/scene/collider_component.h"

#include <cstddef>

namespace Blunder {

/// Parse glTF extras JSON for blender-studio / Godot `collision_info`.
/// Returns true only when a usable triangle list is present (Static trimesh).
bool parseCollisionExtrasJson(const char* json, size_t length, ColliderComponent& out);

}  // namespace Blunder
