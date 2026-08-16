#include "runtime/resource/content_browser/content_browser_view.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace Blunder {

namespace {

bool endsWithSuffix(const eastl::string& value, const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  return value.compare(value.size() - suffix_length, suffix_length, suffix) ==
         0;
}

int compareKeys(const ContentBrowserGridItem& a,
                const ContentBrowserGridItem& b,
                BrowserGridSortColumn column) {
  switch (column) {
    case BrowserGridSortColumn::type: {
      const int type_cmp = a.type_label.compare(b.type_label);
      if (type_cmp != 0) {
        return type_cmp;
      }
      break;
    }
    case BrowserGridSortColumn::size:
      if (a.size_bytes < b.size_bytes) {
        return -1;
      }
      if (a.size_bytes > b.size_bytes) {
        return 1;
      }
      break;
    case BrowserGridSortColumn::date:
      if (a.modified_time < b.modified_time) {
        return -1;
      }
      if (a.modified_time > b.modified_time) {
        return 1;
      }
      break;
    case BrowserGridSortColumn::name:
    default:
      break;
  }
  return a.display_name.compare(b.display_name);
}

}  // namespace

BrowserEntryKind classifyBrowserEntry(bool is_directory,
                                      const eastl::string& virtual_path) {
  if (is_directory) {
    return BrowserEntryKind::folder;
  }
  if (endsWithSuffix(virtual_path, ".mesh.yaml") ||
      endsWithSuffix(virtual_path, ".mesh.asset")) {
    return BrowserEntryKind::mesh;
  }
  if (endsWithSuffix(virtual_path, ".scene.asset")) {
    return BrowserEntryKind::scene;
  }
  if (endsWithSuffix(virtual_path, ".texture.yaml")) {
    return BrowserEntryKind::texture;
  }
  if (endsWithSuffix(virtual_path, ".animation.yaml")) {
    return BrowserEntryKind::animation_clip;
  }
  return BrowserEntryKind::file;
}

const char* browserEntryTypeLabel(BrowserEntryKind kind) {
  switch (kind) {
    case BrowserEntryKind::folder:
      return "Folder";
    case BrowserEntryKind::mesh:
      return "Mesh";
    case BrowserEntryKind::scene:
      return "Scene";
    case BrowserEntryKind::texture:
      return "Texture";
    case BrowserEntryKind::animation_clip:
      return "AnimationClip";
    case BrowserEntryKind::file:
    default:
      return "File";
  }
}

eastl::string formatBrowserSize(uint64_t size_bytes, bool is_directory) {
  if (is_directory) {
    return {};
  }
  char buf[64];
  if (size_bytes < 1024ull) {
    std::snprintf(buf, sizeof(buf), "%llu B",
                  static_cast<unsigned long long>(size_bytes));
  } else if (size_bytes < 1024ull * 1024ull) {
    std::snprintf(buf, sizeof(buf), "%.1f KB",
                  static_cast<double>(size_bytes) / 1024.0);
  } else if (size_bytes < 1024ull * 1024ull * 1024ull) {
    std::snprintf(buf, sizeof(buf), "%.1f MB",
                  static_cast<double>(size_bytes) / (1024.0 * 1024.0));
  } else {
    std::snprintf(buf, sizeof(buf), "%.1f GB",
                  static_cast<double>(size_bytes) /
                      (1024.0 * 1024.0 * 1024.0));
  }
  return eastl::string(buf);
}

eastl::string formatBrowserDate(uint64_t modified_time, bool is_directory) {
  if (is_directory || modified_time == 0) {
    return {};
  }
  using file_clock = std::chrono::file_clock;
  const file_clock::duration duration{
      static_cast<file_clock::duration::rep>(modified_time)};
  const file_clock::time_point file_time{duration};
  const std::chrono::system_clock::time_point sys_time =
      std::chrono::clock_cast<std::chrono::system_clock>(file_time);
  const std::time_t time = std::chrono::system_clock::to_time_t(sys_time);
  std::tm local{};
#ifdef _WIN32
  if (localtime_s(&local, &time) != 0) {
    return {};
  }
#else
  if (localtime_r(&time, &local) == nullptr) {
    return {};
  }
#endif
  char buf[32];
  if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &local) == 0) {
    return {};
  }
  return eastl::string(buf);
}

void fillBrowserGridItemMeta(ContentBrowserGridItem& item) {
  item.type_kind = classifyBrowserEntry(item.is_directory, item.virtual_path);
  item.type_label = browserEntryTypeLabel(item.type_kind);
  item.size_text = formatBrowserSize(item.size_bytes, item.is_directory);
  item.date_text = formatBrowserDate(item.modified_time, item.is_directory);
}

void toggleBrowserGridSort(BrowserGridSortColumn& column, bool& ascending,
                           BrowserGridSortColumn clicked) {
  if (column == clicked) {
    ascending = !ascending;
    return;
  }
  column = clicked;
  ascending = true;
}

void sortBrowserGridItems(eastl::vector<ContentBrowserGridItem>& items,
                          BrowserGridSortColumn column, bool ascending) {
  std::stable_sort(
      items.begin(), items.end(),
      [column, ascending](const ContentBrowserGridItem& a,
                          const ContentBrowserGridItem& b) {
        if (a.is_directory != b.is_directory) {
          return a.is_directory;
        }
        const int cmp = compareKeys(a, b, column);
        if (cmp == 0) {
          return false;
        }
        return ascending ? cmp < 0 : cmp > 0;
      });
}

}  // namespace Blunder
