#pragma once

#include <cstddef>

#include "runtime/core/memory/allocators.h"

namespace Blunder {

class MemorySystem final {
 public:
  void initialize(size_t frame_allocator_size = 2 * 1024 * 1024,
                  size_t pool_block_size = 256,
                  size_t pool_block_count = 1024);
  void shutdown();
  void beginFrame();

  IMemoryAllocator& getDefaultAllocator() { return m_heap_allocator; }
  SingleFrameAllocator& getSingleFrameAllocator() { return m_single_frame_allocator; }
  PoolAllocator& getPoolAllocator() { return m_pool_allocator; }

 private:
  bool m_is_initialized{false};
  HeapAllocator m_heap_allocator;
  SingleFrameAllocator m_single_frame_allocator;
  PoolAllocator m_pool_allocator;
};

}  // namespace Blunder
