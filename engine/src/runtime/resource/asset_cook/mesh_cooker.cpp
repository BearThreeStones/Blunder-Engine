#include "runtime/resource/asset_cook/mesh_cooker.h"

#include <cstring>
#include <fstream>

#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"

namespace Blunder {

namespace fs = std::filesystem;

namespace {

bool readMeshCookHeader(std::istream& stream, MeshCookHeader& header) {
  stream.read(reinterpret_cast<char*>(&header), kMeshCookHeaderV1Size);
  if (!stream) {
    return false;
  }
  if (header.version >= kMeshCookVersion) {
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
                       const MeshSkinData* skin_data) {
  const bool has_skin =
      skin_data != nullptr && skin_data->isValid() &&
      skin_data->influences.size() == vertices.size();

  MeshCookHeader header{};
  std::memcpy(header.magic, kMeshCookMagic, sizeof(header.magic));
  header.vertex_count = static_cast<uint32_t>(vertices.size());
  header.index_count = static_cast<uint32_t>(indices.size());
  header.vertex_stride = sizeof(MeshVertex);
  if (has_skin) {
    header.version = kMeshCookVersion;
    header.flags = kMeshCookFlag_HasSkin;
  } else {
    header.version = kMeshCookVersionLegacy;
    header.flags = 0;
  }

  std::ofstream stream(output_path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return false;
  }

  if (has_skin) {
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
  return stream.good();
}

bool readMeshCookFile(const fs::path& input_path,
                      eastl::vector<MeshVertex>& out_vertices,
                      eastl::vector<uint32_t>& out_indices,
                      MeshSkinData* out_skin_data) {
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
      (header.version != kMeshCookVersionLegacy &&
       header.version != kMeshCookVersion)) {
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
    }
  }
  return found_source && found_descriptor;
}

}  // namespace Blunder
