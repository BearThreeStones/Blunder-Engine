#include "runtime/resource/thumbnail/thumbnail_generation_queue.h"

#include "runtime/resource/thumbnail/thumbnail_generator.h"

namespace Blunder {

void ThumbnailGenerationQueue::bind(ThumbnailGenerator* generator) {
  m_generator = generator;
}

void ThumbnailGenerationQueue::clear() {
  m_items.clear();
  m_index.clear();
}

void ThumbnailGenerationQueue::rebuildIndex() {
  m_index.clear();
  for (size_t i = 0; i < m_items.size(); ++i) {
    m_index[m_items[i].entry.virtual_path] = i;
  }
}

void ThumbnailGenerationQueue::upsert(const ContentEntry& entry,
                                      ThumbnailQueuePriority priority) {
  const auto it = m_index.find(entry.virtual_path);
  if (it != m_index.end()) {
    Item& existing = m_items[it->second];
    if (priority > existing.priority) {
      existing.priority = priority;
    }
    existing.entry = entry;
    return;
  }

  m_index[entry.virtual_path] = m_items.size();
  m_items.push_back(Item{entry, priority});
}

void ThumbnailGenerationQueue::enqueue(const ContentEntry& entry,
                                       ThumbnailQueuePriority priority) {
  upsert(entry, priority);
}

void ThumbnailGenerationQueue::setPriority(const eastl::string& virtual_path,
                                           ThumbnailQueuePriority priority) {
  const auto it = m_index.find(virtual_path);
  if (it == m_index.end()) {
    return;
  }
  Item& existing = m_items[it->second];
  if (priority > existing.priority) {
    existing.priority = priority;
  }
}

void ThumbnailGenerationQueue::demoteAll(ThumbnailQueuePriority priority) {
  for (Item& item : m_items) {
    item.priority = priority;
  }
}

size_t ThumbnailGenerationQueue::findBestItemIndex() const {
  size_t best = 0;
  ThumbnailQueuePriority best_priority = m_items[0].priority;
  for (size_t i = 1; i < m_items.size(); ++i) {
    if (m_items[i].priority > best_priority) {
      best_priority = m_items[i].priority;
      best = i;
    }
  }
  return best;
}

eastl::vector<ThumbnailQueueCompleted> ThumbnailGenerationQueue::tick(
    uint32_t max_items) {
  eastl::vector<ThumbnailQueueCompleted> completed;
  if (m_generator == nullptr || max_items == 0) {
    return completed;
  }

  while (max_items > 0 && !m_items.empty()) {
    const size_t best = findBestItemIndex();
    const Item item = m_items[best];
    m_items.erase(m_items.begin() + static_cast<ptrdiff_t>(best));
    rebuildIndex();

    ThumbnailQueueCompleted done{};
    done.virtual_path = item.entry.virtual_path;
    done.result = m_generator->ensureThumbnail(item.entry);
    completed.push_back(done);
    --max_items;
  }

  return completed;
}

}  // namespace Blunder
