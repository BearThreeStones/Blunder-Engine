#include "runtime/core/memory/allocators.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <new>

namespace Blunder {
namespace {

struct AllocationHeader {
  void* original;
};

HeapAllocator g_fallback_allocator;
IMemoryAllocator* g_default_allocator = &g_fallback_allocator;

size_t alignUp(size_t value, size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

size_t normalizeAlignment(size_t alignment) {
  alignment = std::max(alignment, alignof(void*));
  return std::has_single_bit(alignment) ? alignment : 0;
}

void* allocateRaw(size_t size, size_t alignment, size_t offset) {
  if (size == 0) {
    return nullptr;
  }

  alignment = normalizeAlignment(alignment);
  if (alignment == 0) {
    return nullptr;
  }

  const size_t total_size =
      size + alignment - 1 + offset + sizeof(AllocationHeader);
  void* raw_memory = ::operator new(total_size, std::nothrow);
  if (raw_memory == nullptr) {
    return nullptr;
  }

  const uintptr_t raw_address = reinterpret_cast<uintptr_t>(raw_memory);
  const uintptr_t aligned_address =
      alignUp(raw_address + sizeof(AllocationHeader) + offset, alignment) -
      offset;

  auto* header =
      reinterpret_cast<AllocationHeader*>(aligned_address) - 1;
  header->original = raw_memory;

  return reinterpret_cast<void*>(aligned_address);
}

void freeRaw(void* ptr) {
  if (ptr == nullptr) {
    return;
  }

  auto* header = reinterpret_cast<AllocationHeader*>(ptr) - 1;
  ::operator delete(header->original);
}

}  // namespace

void* HeapAllocator::allocate(size_t size, size_t alignment, size_t offset) {
  return allocateRaw(size, alignment, offset);
}

void HeapAllocator::deallocate(void* ptr, size_t) { freeRaw(ptr); }

SingleFrameAllocator::SingleFrameAllocator(size_t capacity) { initialize(capacity); }

SingleFrameAllocator::~SingleFrameAllocator() { shutdown(); }

void SingleFrameAllocator::initialize(size_t capacity) {
  shutdown();

  m_buffer = allocateRaw(capacity, alignof(std::max_align_t), 0);
  if (m_buffer == nullptr) {
    return;
  }

  m_capacity = capacity;
  m_offset = 0;
}

void SingleFrameAllocator::shutdown() {
  freeRaw(m_buffer);
  m_buffer = nullptr;
  m_capacity = 0;
  m_offset = 0;
}

void SingleFrameAllocator::reset() { m_offset = 0; }

void* SingleFrameAllocator::allocate(size_t size, size_t alignment,
                                     size_t offset) {
  if (m_buffer == nullptr || size == 0) {
    return nullptr;
  }

  alignment = normalizeAlignment(alignment);
  if (alignment == 0) {
    return nullptr;
  }

  const uintptr_t base_address = reinterpret_cast<uintptr_t>(m_buffer);
  const uintptr_t current_address = base_address + m_offset;
  const uintptr_t aligned_address =
      alignUp(current_address + offset, alignment) - offset;
  const size_t next_offset =
      static_cast<size_t>(aligned_address - base_address) + size;

  if (next_offset > m_capacity) {
    return nullptr;
  }

  m_offset = next_offset;
  return reinterpret_cast<void*>(aligned_address);
}

void SingleFrameAllocator::deallocate(void*, size_t) {}

PoolAllocator::PoolAllocator(size_t block_size, size_t block_count) {
  initialize(block_size, block_count);
}

PoolAllocator::~PoolAllocator() { shutdown(); }

void PoolAllocator::initialize(size_t block_size, size_t block_count) {
  shutdown();

  if (block_size == 0 || block_count == 0) {
    return;
  }

  m_block_alignment = normalizeAlignment(alignof(std::max_align_t));
  m_block_size = alignUp(std::max(block_size, sizeof(FreeNode)), m_block_alignment);
  m_block_count = block_count;
  m_free_block_count = block_count;
  m_buffer = allocateRaw(m_block_size * m_block_count, m_block_alignment, 0);

  if (m_buffer == nullptr) {
    m_block_size = 0;
    m_block_count = 0;
    m_free_block_count = 0;
    return;
  }

  auto* buffer_begin = reinterpret_cast<std::byte*>(m_buffer);
  m_free_list = nullptr;

  for (size_t index = 0; index < m_block_count; ++index) {
    auto* node = reinterpret_cast<FreeNode*>(buffer_begin + index * m_block_size);
    node->next = m_free_list;
    m_free_list = node;
  }
}

void PoolAllocator::shutdown() {
  freeRaw(m_buffer);
  m_buffer = nullptr;
  m_free_list = nullptr;
  m_block_size = 0;
  m_block_count = 0;
  m_free_block_count = 0;
}

void* PoolAllocator::allocate(size_t size, size_t alignment, size_t offset) {
  if (m_free_list == nullptr || size == 0 || size > m_block_size || offset != 0) {
    return nullptr;
  }

  alignment = normalizeAlignment(alignment);
  if (alignment == 0 || alignment > m_block_alignment) {
    return nullptr;
  }

  FreeNode* node = m_free_list;
  m_free_list = m_free_list->next;
  --m_free_block_count;
  return node;
}

void PoolAllocator::deallocate(void* ptr, size_t) {
  if (ptr == nullptr) {
    return;
  }

  auto* node = static_cast<FreeNode*>(ptr);
  node->next = m_free_list;
  m_free_list = node;
  ++m_free_block_count;
}

IMemoryAllocator& GetDefaultAllocator() { return *g_default_allocator; }

IMemoryAllocator* SetDefaultAllocator(IMemoryAllocator* allocator) {
  IMemoryAllocator* previous_allocator = g_default_allocator;
  g_default_allocator = allocator != nullptr ? allocator : &g_fallback_allocator;
  return previous_allocator;
}

}  // namespace Blunder
