#include <cstddef>

#include "EASTL/allocator.h"
#include "runtime/core/memory/allocators.h"

namespace eastl {
namespace {
allocator g_default_allocator;
allocator* g_default_allocator_ptr = &g_default_allocator;
}  // namespace

allocator::allocator(const char* EASTL_NAME(pName)) {
#if EASTL_NAME_ENABLED
  mpName = pName ? pName : EASTL_ALLOCATOR_DEFAULT_NAME;
#endif
}

allocator::allocator(const allocator& EASTL_NAME(alloc)) {
#if EASTL_NAME_ENABLED
  mpName = alloc.mpName;
#endif
}

allocator::allocator(const allocator&, const char* EASTL_NAME(pName)) {
#if EASTL_NAME_ENABLED
  mpName = pName ? pName : EASTL_ALLOCATOR_DEFAULT_NAME;
#endif
}

allocator& allocator::operator=(const allocator& EASTL_NAME(alloc)) {
#if EASTL_NAME_ENABLED
  mpName = alloc.mpName;
#endif
  return *this;
}

void* allocator::allocate(size_t n, int) {
  return Blunder::GetDefaultAllocator().allocate(n, alignof(std::max_align_t), 0);
}

void* allocator::allocate(size_t n, size_t alignment, size_t offset, int) {
  return Blunder::GetDefaultAllocator().allocate(n, alignment, offset);
}

void allocator::deallocate(void* p, size_t n) {
  Blunder::GetDefaultAllocator().deallocate(p, n);
}

const char* allocator::get_name() const {
#if EASTL_NAME_ENABLED
  return mpName;
#else
  return EASTL_ALLOCATOR_DEFAULT_NAME;
#endif
}

void allocator::set_name(const char* EASTL_NAME(pName)) {
#if EASTL_NAME_ENABLED
  mpName = pName;
#endif
}

allocator* GetDefaultAllocator() { return g_default_allocator_ptr; }

allocator* SetDefaultAllocator(allocator* pAllocator) {
  allocator* previous_allocator = g_default_allocator_ptr;
  g_default_allocator_ptr = pAllocator ? pAllocator : &g_default_allocator;
  return previous_allocator;
}

bool operator==(const allocator&, const allocator&) { return true; }

#if !defined(EA_COMPILER_HAS_THREE_WAY_COMPARISON)
bool operator!=(const allocator&, const allocator&) { return false; }
#endif

}  // namespace eastl
