#include "runtime/resource/content/content_index.h"

#include <cstring>

#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/content/content_entry.h"
#include "runtime/resource/thumbnail/thumbnail_generator.h"

namespace Blunder {

namespace {

bool endsWith(const eastl::string& value, const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  return value.compare(value.size() - suffix_length, suffix_length, suffix) == 0;
}

bool shouldSkipEntry(const DirectoryEntry& entry) {
  if (!entry.is_directory && endsWith(entry.relative_path, ".gitkeep")) {
    return true;
  }
  return false;
}

eastl::string buildVirtualPath(ContentRoot root, const eastl::string& relative) {
  const char* prefix =
      root == ContentRoot::Resources ? "resources/" : "assets/";
  eastl::string virtual_path(prefix);
  virtual_path.append(relative);
  return virtual_path;
}

}  // namespace

eastl::vector<ContentEntry> ContentIndex::scan(const FileSystem& file_system,
                                               int32_t max_depth) {
  eastl::vector<ContentEntry> entries;

  const auto scan_root = [&](ContentRoot root,
                             const std::filesystem::path& absolute_root) {
    if (!file_system.isDirectory(absolute_root)) {
      return;
    }

    const eastl::vector<DirectoryEntry> discovered =
        file_system.listDirectoryRecursive(absolute_root, absolute_root,
                                           max_depth);
    for (const DirectoryEntry& dir_entry : discovered) {
      if (shouldSkipEntry(dir_entry)) {
        continue;
      }

      ContentEntry content_entry{};
      content_entry.root = root;
      content_entry.is_directory = dir_entry.is_directory;
      content_entry.virtual_path =
          buildVirtualPath(root, dir_entry.relative_path);
      if (!dir_entry.is_directory) {
        content_entry.size_bytes =
            file_system.fileSize(dir_entry.absolute_path);
        content_entry.modified_time =
            file_system.lastWriteTime(dir_entry.absolute_path);
      }
      entries.push_back(content_entry);
    }
  };

  scan_root(ContentRoot::Assets, file_system.getAssetRoot());
  return entries;
}

eastl::vector<ContentEntry> buildContentIndexWithThumbnails(
    FileSystem& file_system, AssetManager& asset_manager,
    ThumbnailGenerator& thumbnail_generator, int32_t max_depth) {
  (void)asset_manager;
  eastl::vector<ContentEntry> entries = ContentIndex::scan(file_system, max_depth);
  for (ContentEntry& entry : entries) {
    const ThumbnailResult thumb = thumbnail_generator.ensureThumbnail(entry);
    entry.thumbnail_status = thumb.status;
    entry.thumbnail_cache_path = thumb.cache_path;
  }
  return entries;
}

}  // namespace Blunder
