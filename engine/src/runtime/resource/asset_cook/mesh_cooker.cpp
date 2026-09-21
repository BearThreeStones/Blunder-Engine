#include "runtime/resource/asset_cook/mesh_cooker.h"

#include <cstring>
#include <fstream>

#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/meshlet.h"

namespace Blunder {

namespace fs = std::filesystem;

namespace {

bool readMeshCookHeader(std::istream& stream, MeshCookHeader& header) {
  stream.read(reinterpret_cast<char*>(&header), kMeshCookHeaderV1Size);
  if (!stream) {
    return false;
  }
  if (header.version >= kMeshCookVersionSkin) {
    stream.read(reinterpret_cast<char*>(&header.flags), sizeof(header.flags));
    if (!stream) {
      return false;
    }
  } else {
    header.flags = 0;
  }
  return true;
}

bool writeSkinPayload(std::ostream& stream, const MeshSkinData& skin_data,
                      uint32_t vertex_count) {
  if (!skin_data.isValid() || skin_data.influences.size() != vertex_count) {
    return false;
  }

  const uint32_t joint_count =
      static_cast<uint32_t>(skin_data.joint_to_bone.size());
  stream.write(reinterpret_cast<const char*>(&joint_count), sizeof(joint_count));
  if (!joint_count) {
    return false;
  }

  stream.write(reinterpret_cast<const char*>(skin_data.joint_to_bone.data()),
               static_cast<std::streamsize>(joint_count * sizeof(int)));
  stream.write(reinterpret_cast<const char*>(skin_data.influences.data()),
               static_cast<std::streamsize>(vertex_count *
                                            sizeof(MeshSkinInfluence)));
  return stream.good();
}

bool readSkinPayload(std::istream& stream, uint32_t vertex_count,
                     MeshSkinData& out_skin_data) {
  uint32_t joint_count = 0;
  stream.read(reinterpret_cast<char*>(&joint_count), sizeof(joint_count));
  if (!stream || joint_count == 0) {
    return false;
  }

  out_skin_data.joint_to_bone.resize(joint_count);
  stream.read(reinterpret_cast<char*>(out_skin_data.joint_to_bone.data()),
              static_cast<std::streamsize>(joint_count * sizeof(int)));
  if (!stream) {
    return false;
  }

  out_skin_data.influences.resize(vertex_count);
  stream.read(reinterpret_cast<char*>(out_skin_data.influences.data()),
              static_cast<std::streamsize>(vertex_count *
                                            sizeof(MeshSkinInfluence)));
  return stream.good() && out_skin_data.isValid();
}

bool writeMeshletPayload(std::ostream& stream, const MeshletPayload& payload) {
  const uint32_t meshlet_count =
      static_cast<uint32_t>(payload.meshlets.size());
  const uint32_t vertex_count =
      static_cast<uint32_t>(payload.vertices.size());
  const uint32_t triangle_count =
      static_cast<uint32_t>(payload.triangles.size());
  stream.write(reinterpret_cast<const char*>(&meshlet_count),
               sizeof(meshlet_count));
  stream.write(reinterpret_cast<const char*>(&vertex_count),
               sizeof(vertex_count));
  stream.write(reinterpret_cast<const char*>(&triangle_count),
               sizeof(triangle_count));
  if (meshlet_count > 0) {
    stream.write(reinterpret_cast<const char*>(payload.meshlets.data()),
                 static_cast<std::streamsize>(meshlet_count *
                                              sizeof(MeshletRecord)));
  }
  if (vertex_count > 0) {
    stream.write(reinterpret_cast<const char*>(payload.vertices.data()),
                 static_cast<std::streamsize>(vertex_count * sizeof(uint32_t)));
  }
  if (triangle_count > 0) {
    stream.write(reinterpret_cast<const char*>(payload.triangles.data()),
                 static_cast<std::streamsize>(triangle_count));
  }
  return stream.good();
}

bool readMeshletPayload(std::istream& stream, MeshletPayload& out_payload) {
  uint32_t meshlet_count = 0;
  uint32_t vertex_count = 0;
  uint32_t triangle_count = 0;
  stream.read(reinterpret_cast<char*>(&meshlet_count), sizeof(meshlet_count));
  stream.read(reinterpret_cast<char*>(&vertex_count), sizeof(vertex_count));
  stream.read(reinterpret_cast<char*>(&triangle_count), sizeof(triangle_count));
  if (!stream) {
    return false;
  }
  out_payload.meshlets.resize(meshlet_count);
  out_payload.vertices.resize(vertex_count);
  out_payload.triangles.resize(triangle_count);
  if (meshlet_count > 0) {
    stream.read(reinterpret_cast<char*>(out_payload.meshlets.data()),
                static_cast<std::streamsize>(meshlet_count *
                                             sizeof(MeshletRecord)));
  }
  if (vertex_count > 0) {
    stream.read(reinterpret_cast<char*>(out_payload.vertices.data()),
                static_cast<std::streamsize>(vertex_count * sizeof(uint32_t)));
  }
  if (triangle_count > 0) {
    stream.read(reinterpret_cast<char*>(out_payload.triangles.data()),
                static_cast<std::streamsize>(triangle_count));
  }
  return stream.good();
}

}  // namespace

std::filesystem::path cookedRoot(FileSystem& file_system) {
  return file_system.resolve(".blunder/cooked");
}

std::filesystem::path cookedMeshPath(FileSystem& file_system,
                                     const eastl::string& guid) {
  fs::path path = cookedRoot(file_system);
  path /= eastl::string((guid + ".meshbin").c_str()).c_str();
  return path;
}

std::filesystem::path cookedMeshMetaPath(FileSystem& file_system,
                                         const eastl::string& guid) {
  fs::path path = cookedRoot(file_system);
  path /= eastl::string((guid + ".meshbin.meta").c_str()).c_str();
  return path;
}

std::filesystem::path cookedMeshMaterialPath(FileSystem& file_system,
                                             const eastl::string& guid) {
  fs::path path = cookedRoot(file_system);
  path /= eastl::string((guid + ".meshmat.yaml").c_str()).c_str();
  return path;
}

std::filesystem::path cookedTexturePath(FileSystem& file_system,
                                        const eastl::string& guid) {
  fs::path path = cookedRoot(file_system);
  path /= eastl::string((guid + ".texbin").c_str()).c_str();
  return path;
}

std::filesystem::path cookedTextureMetaPath(FileSystem& file_system,
                                            const eastl::string& guid) {
  fs::path path = cookedRoot(file_system);
  path /= eastl::string((guid + ".texbin.meta").c_str()).c_str();
  return path;
}

bool writeMeshCookFile(const fs::path& output_path,
                       const eastl::vector<MeshVertex>& vertices,
                       const eastl::vector<uint32_t>& indices,
                       const MeshSkinData* skin_data,
                       const MeshletPayload* meshlets) {
  const bool has_skin =
      skin_data != nullptr && skin_data->isValid() &&
      skin_data->influences.size() == vertices.size();
  const bool has_meshlets =
      !has_skin && meshlets != nullptr && !meshlets->empty();

  MeshCookHeader header{};
  std::memcpy(header.magic, kMeshCookMagic, sizeof(header.magic));
  header.vertex_count = static_cast<uint32_t>(vertices.size());
  header.index_count = static_cast<uint32_t>(indices.size());
  header.vertex_stride = sizeof(MeshVertex);
  header.flags = 0;
  if (has_skin) {
    header.version = kMeshCookVersionSkin;
    header.flags = kMeshCookFlag_HasSkin;
  } else if (has_meshlets) {
    header.version = kMeshCookVersion;
    header.flags = kMeshCookFlag_HasMeshlets;
  } else {
    header.version = kMeshCookVersionLegacy;
  }

  std::ofstream stream(output_path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return false;
  }

  if (header.version >= kMeshCookVersionSkin) {
    stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
  } else {
    stream.write(reinterpret_cast<const char*>(&header), kMeshCookHeaderV1Size);
  }
  if (!vertices.empty()) {
    stream.write(reinterpret_cast<const char*>(vertices.data()),
                 static_cast<std::streamsize>(vertices.size() * sizeof(MeshVertex)));
  }
  if (!indices.empty()) {
    stream.write(reinterpret_cast<const char*>(indices.data()),
                 static_cast<std::streamsize>(indices.size() * sizeof(uint32_t)));
  }
  if (has_skin && !writeSkinPayload(stream, *skin_data, header.vertex_count)) {
    return false;
  }
  if (has_meshlets && !writeMeshletPayload(stream, *meshlets)) {
    return false;
  }
  return stream.good();
}

bool readMeshCookFile(const fs::path& input_path,
                      eastl::vector<MeshVertex>& out_vertices,
                      eastl::vector<uint32_t>& out_indices,
                      MeshSkinData* out_skin_data,
                      MeshletPayload* out_meshlets) {
  std::ifstream stream(input_path, std::ios::binary);
  if (!stream) {
    return false;
  }

  MeshCookHeader header{};
  if (!readMeshCookHeader(stream, header)) {
    return false;
  }
  if (std::memcmp(header.magic, kMeshCookMagic, sizeof(header.magic)) != 0 ||
      header.vertex_stride != sizeof(MeshVertex) ||
      header.version < kMeshCookVersionLegacy ||
      header.version > kMeshCookVersion) {
    return false;
  }

  out_vertices.resize(header.vertex_count);
  out_indices.resize(header.index_count);

  if (header.vertex_count > 0) {
    stream.read(reinterpret_cast<char*>(out_vertices.data()),
                static_cast<std::streamsize>(header.vertex_count *
                                             sizeof(MeshVertex)));
  }
  if (header.index_count > 0) {
    stream.read(reinterpret_cast<char*>(out_indices.data()),
                static_cast<std::streamsize>(header.index_count *
                                             sizeof(uint32_t)));
  }
  if (!stream) {
    return false;
  }

  if (out_skin_data != nullptr) {
    out_skin_data->influences.clear();
    out_skin_data->joint_to_bone.clear();
    if ((header.flags & kMeshCookFlag_HasSkin) != 0) {
      if (!readSkinPayload(stream, header.vertex_count, *out_skin_data)) {
        return false;
      }
    }
  } else if ((header.flags & kMeshCookFlag_HasSkin) != 0) {
    MeshSkinData skip_skin;
    if (!readSkinPayload(stream, header.vertex_count, skip_skin)) {
      return false;
    }
  }

  if (out_meshlets != nullptr) {
    *out_meshlets = MeshletPayload{};
    if ((header.flags & kMeshCookFlag_HasMeshlets) != 0) {
      if (!readMeshletPayload(stream, *out_meshlets)) {
        return false;
      }
    }
  }

  return stream.good();
}

bool writeCookMetaFile(const fs::path& meta_path, const CookedAssetMeta& meta) {
  std::ofstream stream(meta_path, std::ios::trunc);
  if (!stream) {
    return false;
  }
  stream << "source_mtime: " << meta.source_mtime << '\n';
  stream << "descriptor_mtime: " << meta.descriptor_mtime << '\n';
  stream << "cook_format: " << meta.cook_format << '\n';
  return stream.good();
}

bool readCookMetaFile(const fs::path& meta_path, CookedAssetMeta& out_meta) {
  std::ifstream stream(meta_path);
  if (!stream) {
    return false;
  }

  std::string line;
  bool found_source = false;
  bool found_descriptor = false;
  out_meta.cook_format = 0;  // missing key means 0
  while (std::getline(stream, line)) {
    const size_t colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    const std::string key = line.substr(0, colon);
    const std::string value = line.substr(colon + 1);
    const uint64_t parsed = std::strtoull(value.c_str(), nullptr, 10);
    if (key == "source_mtime") {
      out_meta.source_mtime = parsed;
      found_source = true;
    } else if (key == "descriptor_mtime") {
      out_meta.descriptor_mtime = parsed;
      found_descriptor = true;
    } else if (key == "cook_format") {
      out_meta.cook_format = static_cast<uint32_t>(parsed);
    }
  }
  return found_source && found_descriptor;
}

}  // namespace Blunder
