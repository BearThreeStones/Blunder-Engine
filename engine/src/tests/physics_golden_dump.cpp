#include "physics_golden_dump.h"

#include <cstring>
#include <string>

namespace Blunder::PhysicsGolden {

static constexpr char kMagic[8] = {'B', 'L', 'D', 'R', 'P', 'H', 'Y', 'S'};
static constexpr uint32_t kVersion = 1;

BodyState captureBody(const PhysicsWorld& world, RigidBodyHandle body) {
  BodyState state{};
  const PhysicsTransform pose = world.getPose(body);
  state.pos_x = pose.position.x.raw();
  state.pos_y = pose.position.y.raw();
  state.pos_z = pose.position.z.raw();
  state.quat_x = pose.rotation.x.raw();
  state.quat_y = pose.rotation.y.raw();
  state.quat_z = pose.rotation.z.raw();
  state.quat_w = pose.rotation.w.raw();
  const FixedVec3 velocity = world.getLinearVelocity(body);
  state.vel_x = velocity.x.raw();
  state.vel_y = velocity.y.raw();
  state.vel_z = velocity.z.raw();
  state.sleeping = world.isBodySleeping(body) ? 1 : 0;
  return state;
}

static bool writeU32(FILE* file, uint32_t value) {
  return std::fwrite(&value, sizeof(value), 1, file) == 1;
}

static bool readU32(FILE* file, uint32_t& value) {
  return std::fread(&value, sizeof(value), 1, file) == 1;
}

static bool writeI64(FILE* file, int64_t value) {
  return std::fwrite(&value, sizeof(value), 1, file) == 1;
}

static bool readI64(FILE* file, int64_t& value) {
  return std::fread(&value, sizeof(value), 1, file) == 1;
}

static bool writeBody(FILE* file, const BodyState& body) {
  return writeI64(file, body.pos_x) && writeI64(file, body.pos_y) && writeI64(file, body.pos_z) &&
         writeI64(file, body.quat_x) && writeI64(file, body.quat_y) && writeI64(file, body.quat_z) &&
         writeI64(file, body.quat_w) && writeI64(file, body.vel_x) && writeI64(file, body.vel_y) &&
         writeI64(file, body.vel_z) && std::fwrite(&body.sleeping, sizeof(body.sleeping), 1, file) == 1;
}

static bool readBody(FILE* file, BodyState& body) {
  return readI64(file, body.pos_x) && readI64(file, body.pos_y) && readI64(file, body.pos_z) &&
         readI64(file, body.quat_x) && readI64(file, body.quat_y) && readI64(file, body.quat_z) &&
         readI64(file, body.quat_w) && readI64(file, body.vel_x) && readI64(file, body.vel_y) &&
         readI64(file, body.vel_z) && std::fread(&body.sleeping, sizeof(body.sleeping), 1, file) == 1;
}

static bool bodiesEqual(const BodyState& lhs, const BodyState& rhs) {
  return lhs.pos_x == rhs.pos_x && lhs.pos_y == rhs.pos_y && lhs.pos_z == rhs.pos_z &&
         lhs.quat_x == rhs.quat_x && lhs.quat_y == rhs.quat_y && lhs.quat_z == rhs.quat_z &&
         lhs.quat_w == rhs.quat_w && lhs.vel_x == rhs.vel_x && lhs.vel_y == rhs.vel_y &&
         lhs.vel_z == rhs.vel_z && lhs.sleeping == rhs.sleeping;
}

bool writeDumpFile(const char* path, const std::vector<ScenarioSnapshot>& snapshots) {
  FILE* file = std::fopen(path, "wb");
  if (!file) {
    return false;
  }

  bool ok = std::fwrite(kMagic, 1, sizeof(kMagic), file) == sizeof(kMagic) && writeU32(file, kVersion) &&
            writeU32(file, static_cast<uint32_t>(snapshots.size()));

  for (const ScenarioSnapshot& snapshot : snapshots) {
    const std::string name = snapshot.name ? snapshot.name : "";
    const uint32_t name_len = static_cast<uint32_t>(name.size());
    ok = ok && writeU32(file, name_len);
    if (name_len > 0) {
      ok = ok && std::fwrite(name.data(), 1, name_len, file) == name_len;
    }
    ok = ok && writeU32(file, static_cast<uint32_t>(snapshot.bodies.size()));
    for (const BodyState& body : snapshot.bodies) {
      ok = ok && writeBody(file, body);
    }
  }

  std::fclose(file);
  return ok;
}

bool compareDumpFile(const char* path, const std::vector<ScenarioSnapshot>& snapshots, FILE* err) {
  FILE* file = std::fopen(path, "rb");
  if (!file) {
    if (err) {
      std::fprintf(err, "physics_golden_suite: cannot open dump '%s'\n", path);
    }
    return false;
  }

  char magic[8]{};
  uint32_t version = 0;
  uint32_t scenario_count = 0;
  bool ok = std::fread(magic, 1, sizeof(magic), file) == sizeof(magic) && readU32(file, version) &&
            readU32(file, scenario_count);

  if (!ok || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0 || version != kVersion) {
    if (err) {
      std::fprintf(err, "physics_golden_suite: invalid dump header in '%s'\n", path);
    }
    std::fclose(file);
    return false;
  }

  if (scenario_count != snapshots.size()) {
    if (err) {
      std::fprintf(err, "physics_golden_suite: scenario count mismatch (%u vs %zu)\n", scenario_count,
                   snapshots.size());
    }
    std::fclose(file);
    return false;
  }

  for (size_t scenario_index = 0; scenario_index < snapshots.size(); ++scenario_index) {
    uint32_t name_len = 0;
    ok = readU32(file, name_len);
    std::string name;
    if (ok && name_len > 0) {
      name.resize(name_len);
      ok = std::fread(name.data(), 1, name_len, file) == name_len;
    }
    uint32_t body_count = 0;
    ok = ok && readU32(file, body_count);
    if (!ok) {
      break;
    }

    const ScenarioSnapshot& expected = snapshots[scenario_index];
    if (name != (expected.name ? expected.name : "") || body_count != expected.bodies.size()) {
      if (err) {
        std::fprintf(err, "physics_golden_suite: metadata mismatch for scenario %zu\n", scenario_index);
      }
      std::fclose(file);
      return false;
    }

    for (size_t body_index = 0; body_index < expected.bodies.size(); ++body_index) {
      BodyState actual{};
      ok = readBody(file, actual);
      if (!ok) {
        break;
      }
      if (!bodiesEqual(actual, expected.bodies[body_index])) {
        if (err) {
          std::fprintf(err, "physics_golden_suite: bit mismatch scenario '%s' body %zu\n",
                       expected.name ? expected.name : "", body_index);
        }
        std::fclose(file);
        return false;
      }
    }
  }

  std::fclose(file);
  return ok;
}

}  // namespace Blunder::PhysicsGolden
