#pragma once

#include <cstddef>

namespace Blunder {

class IMemoryAllocator {
 public:
  virtual ~IMemoryAllocator() = default;

  virtual void* allocate(size_t size,
                         size_t alignment = alignof(std::max_align_t),
                         size_t offset = 0) = 0;
  virtual void deallocate(void* ptr, size_t size = 0) = 0;
};

class HeapAllocator final : public IMemoryAllocator {
 public:
  void* allocate(size_t size, size_t alignment = alignof(std::max_align_t),
                 size_t offset = 0) override;
  void deallocate(void* ptr, size_t size = 0) override;
};

class SingleFrameAllocator final : public IMemoryAllocator {
 public:
  SingleFrameAllocator() = default;
  explicit SingleFrameAllocator(size_t capacity);
  ~SingleFrameAllocator() override;

  void initialize(size_t capacity);
  void shutdown();
  void reset();

  void* allocate(size_t size, size_t alignment = alignof(std::max_align_t),
                 size_t offset = 0) override;
  void deallocate(void* ptr, size_t size = 0) override;

  size_t capacity() const { return m_capacity; }
  size_t used() const { return m_offset; }

 private:
  void* m_buffer{nullptr};
  size_t m_capacity{0};
  size_t m_offset{0};
};

class PoolAllocator final : public IMemoryAllocator {
 public:
  PoolAllocator() = default;
  PoolAllocator(size_t block_size, size_t block_count);
  ~PoolAllocator() override;

  void initialize(size_t block_size, size_t block_count);
  void shutdown();

  void* allocate(size_t size, size_t alignment = alignof(std::max_align_t),
                 size_t offset = 0) override;
  void deallocate(void* ptr, size_t size = 0) override;

  size_t blockSize() const { return m_block_size; }
  size_t blockCount() const { return m_block_count; }
  size_t freeBlockCount() const { return m_free_block_count; }

 private:
  struct FreeNode {
    FreeNode* next;
  };

  void* m_buffer{nullptr};
  FreeNode* m_free_list{nullptr};
  size_t m_block_size{0};
  size_t m_block_count{0};
  size_t m_free_block_count{0};
  size_t m_block_alignment{alignof(std::max_align_t)};
};

IMemoryAllocator& GetDefaultAllocator();
IMemoryAllocator* SetDefaultAllocator(IMemoryAllocator* allocator);

}  // namespace Blunder
