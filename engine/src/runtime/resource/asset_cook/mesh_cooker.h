#pragma once

#include <filesystem>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_cook/asset_cook_types.h"

namespace Blunder {

class FileSystem;

bool writeMeshCookFile(const std::filesystem::path& output_path,
                       const eastl::vector<MeshVertex>& vertices,
                       const eastl::vector<uint32_t>& indices);

bool readMeshCookFile(const std::filesystem::path& input_path,
                      eastl::vector<MeshVertex>& out_vertices,
                      eastl::vector<uint32_t>& out_indices);

bool writeCookMetaFile(const std::filesystem::path& meta_path,
                       const CookedAssetMeta& meta);

bool readCookMetaFile(const std::filesystem::path& meta_path,
                      CookedAssetMeta& out_meta);

std::filesystem::path cookedMeshPath(FileSystem& file_system,
                                     const eastl::string& guid);
std::filesystem::path cookedMeshMetaPath(FileSystem& file_system,
                                         const eastl::string& guid);
std::filesystem::path cookedTexturePath(FileSystem& file_system,
                                        const eastl::string& guid);
std::filesystem::path cookedTextureMetaPath(FileSystem& file_system,
                                            const eastl::string& guid);

std::filesystem::path cookedRoot(FileSystem& file_system);

}  // namespace Blunder
