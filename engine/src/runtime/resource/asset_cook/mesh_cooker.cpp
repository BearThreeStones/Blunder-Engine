#include "runtime/resource/asset_cook/mesh_cooker.h"

#include <cstring>
#include <fstream>

#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"

namespace Blunder {

namespace fs = std::filesystem;

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
                       const eastl::vector<uint32_t>& indices) {
  MeshCookHeader header{};
  std::memcpy(header.magic, kMeshCookMagic, sizeof(header.magic));
  header.version = kMeshCookVersion;
  header.vertex_count = static_cast<uint32_t>(vertices.size());
  header.index_count = static_cast<uint32_t>(indices.size());
  header.vertex_stride = sizeof(MeshVertex);

  std::ofstream stream(output_path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return false;
  }

  stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
  if (!vertices.empty()) {
    stream.write(reinterpret_cast<const char*>(vertices.data()),
                 static_cast<std::streamsize>(vertices.size() * sizeof(MeshVertex)));
  }
  if (!indices.empty()) {
    stream.write(reinterpret_cast<const char*>(indices.data()),
                 static_cast<std::streamsize>(indices.size() * sizeof(uint32_t)));
  }
  return stream.good();
}

bool readMeshCookFile(const fs::path& input_path,
                      eastl::vector<MeshVertex>& out_vertices,
                      eastl::vector<uint32_t>& out_indices) {
  std::ifstream stream(input_path, std::ios::binary);
  if (!stream) {
    return false;
  }

  MeshCookHeader header{};
  stream.read(reinterpret_cast<char*>(&header), sizeof(header));
  if (!stream) {
    return false;
  }
  if (std::memcmp(header.magic, kMeshCookMagic, sizeof(header.magic)) != 0 ||
      header.version != kMeshCookVersion ||
      header.vertex_stride != sizeof(MeshVertex)) {
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
