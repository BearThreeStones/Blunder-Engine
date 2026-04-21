#include "runtime/core/memory/memory_system.h"

namespace Blunder {

void MemorySystem::initialize(size_t frame_allocator_size, size_t pool_block_size,
                              size_t pool_block_count) {
  if (m_is_initialized) {
    return;
  }

  m_single_frame_allocator.initialize(frame_allocator_size);
  m_pool_allocator.initialize(pool_block_size, pool_block_count);
  SetDefaultAllocator(&m_heap_allocator);
  m_is_initialized = true;
}

void MemorySystem::shutdown() {
  if (!m_is_initialized) {
    return;
  }

  SetDefaultAllocator(nullptr);
  m_pool_allocator.shutdown();
  m_single_frame_allocator.shutdown();
  m_is_initialized = false;
}

void MemorySystem::beginFrame() {
  if (!m_is_initialized) {
    return;
  }

  m_single_frame_allocator.reset();
}

}  // namespace Blunder
