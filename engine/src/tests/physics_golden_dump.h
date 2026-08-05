#pragma once

#include "function/physics/physics_world.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace Blunder::PhysicsGolden {

struct BodyState {
  int64_t pos_x = 0;
  int64_t pos_y = 0;
  int64_t pos_z = 0;
  int64_t quat_x = 0;
  int64_t quat_y = 0;
  int64_t quat_z = 0;
  int64_t quat_w = Fixed::from_int(1).raw();
  int64_t vel_x = 0;
  int64_t vel_y = 0;
  int64_t vel_z = 0;
  uint8_t sleeping = 0;
};

struct ScenarioSnapshot {
  const char* name = nullptr;
  std::vector<BodyState> bodies;
};

[[nodiscard]] BodyState captureBody(const PhysicsWorld& world, RigidBodyHandle body);

bool writeDumpFile(const char* path, const std::vector<ScenarioSnapshot>& snapshots);
bool compareDumpFile(const char* path, const std::vector<ScenarioSnapshot>& snapshots, FILE* err);

}  // namespace Blunder::PhysicsGolden
