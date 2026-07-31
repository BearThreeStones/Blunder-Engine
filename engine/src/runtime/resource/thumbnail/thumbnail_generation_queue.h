#pragma once

#include <cstdint>

#include "EASTL/hash_map.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/content/content_entry.h"
#include "runtime/resource/thumbnail/thumbnail_types.h"

namespace Blunder {

class ThumbnailGenerator;

enum class ThumbnailQueuePriority : uint8_t {
  Background = 0,
  Visible = 1,
};

struct ThumbnailQueueCompleted {
  eastl::string virtual_path;
  ThumbnailResult result;
};

/// Async thumbnail work queue with visible-grid priority over background items.
class ThumbnailGenerationQueue final {
 public:
  void bind(ThumbnailGenerator* generator);
  void clear();

  void enqueue(const ContentEntry& entry, ThumbnailQueuePriority priority);
  void setPriority(const eastl::string& virtual_path,
                   ThumbnailQueuePriority priority);

  uint32_t pendingCount() const {
    return static_cast<uint32_t>(m_items.size());
  }

  /// Process up to max_items highest-priority entries.
  eastl::vector<ThumbnailQueueCompleted> tick(uint32_t max_items);

 private:
  struct Item {
    ContentEntry entry;
    ThumbnailQueuePriority priority{ThumbnailQueuePriority::Background};
  };

  void upsert(const ContentEntry& entry, ThumbnailQueuePriority priority);
  void rebuildIndex();
  size_t findBestItemIndex() const;

  ThumbnailGenerator* m_generator{nullptr};
  eastl::vector<Item> m_items;
  eastl::hash_map<eastl::string, size_t> m_index;
};

}  // namespace Blunder
