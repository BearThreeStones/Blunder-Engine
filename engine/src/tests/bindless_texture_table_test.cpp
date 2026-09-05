#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/vulkan/bindless_texture_table.h"

#include "EASTL/shared_ptr.h"

#include <cstdint>
#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();

  {
    BindlessTextureIndexTable table;
    expect_true("null key is fallback 0",
                table.acquire(nullptr) ==
                    BindlessTextureIndexTable::k_fallback_index);

    int a = 0;
    int b = 0;
    int c = 0;
    const uint32_t index_a = table.acquire(&a);
    expect_true("first texture is slot 1", index_a == 1);
    expect_true("same texture keeps slot", table.acquire(&a) == index_a);
    const uint32_t index_b = table.acquire(&b);
    expect_true("second texture is slot 2", index_b == 2);
    expect_true("release returns that slot", table.release(&a) == index_a);
    expect_true("freed slot reused, others stay", table.acquire(&c) == index_a);
    expect_true("other texture index unchanged", table.acquire(&b) == index_b);
    expect_true("released key gets a new free slot", table.acquire(&a) == 3);
  }

  {
    BindlessTextureIndexTable table;
    int keys[BindlessTextureIndexTable::k_capacity];
    for (uint32_t i = 1; i < BindlessTextureIndexTable::k_capacity; ++i) {
      expect_true("fill unique slots", table.acquire(&keys[i]) == i);
    }
    int extra = 0;
    expect_true("overflow returns fallback 0",
                table.acquire(&extra) ==
                    BindlessTextureIndexTable::k_fallback_index);
    expect_true("overflow does not steal existing slot",
                table.acquire(&keys[1]) == 1);
    expect_true("release after full returns that slot",
                table.release(&keys[50]) == 50);
    expect_true("acquire after release reuses freed slot",
                table.acquire(&extra) == 50);
  }

  {
    BindlessTextureIndexTable table;
    bool inserted = true;
    expect_true("null acquire leaves inserted false",
                table.acquire(nullptr, &inserted) ==
                    BindlessTextureIndexTable::k_fallback_index &&
                    !inserted);

    int a = 0;
    inserted = false;
    expect_true("first insert sets inserted true",
                table.acquire(&a, &inserted) == 1 && inserted);
    inserted = true;
    expect_true("re-acquire leaves inserted false",
                table.acquire(&a, &inserted) == 1 && !inserted);

    expect_true("release null is capacity sentinel",
                table.release(nullptr) == BindlessTextureIndexTable::k_capacity);
    int ghost = 0;
    expect_true("release unknown key is capacity sentinel",
                table.release(&ghost) == BindlessTextureIndexTable::k_capacity);
    expect_true("release existing returns slot", table.release(&a) == 1);
    expect_true("double release is capacity sentinel",
                table.release(&a) == BindlessTextureIndexTable::k_capacity);

    int keys[BindlessTextureIndexTable::k_capacity];
    for (uint32_t i = 1; i < BindlessTextureIndexTable::k_capacity; ++i) {
      table.acquire(&keys[i]);
    }
    int extra = 0;
    inserted = true;
    expect_true("overflow leaves inserted false",
                table.acquire(&extra, &inserted) ==
                    BindlessTextureIndexTable::k_fallback_index &&
                    !inserted);
  }

  {
    BindlessTextureTable table;
    auto* fallback = reinterpret_cast<VulkanTexture*>(static_cast<uintptr_t>(1));
    auto* other = reinterpret_cast<VulkanTexture*>(static_cast<uintptr_t>(2));
    table.setFallback(fallback);
    expect_true("table acquire null is fallback",
                table.acquire(nullptr) ==
                    BindlessTextureIndexTable::k_fallback_index);
    expect_true("table acquire fallback pointer is fallback",
                table.acquire(fallback) ==
                    BindlessTextureIndexTable::k_fallback_index);
    expect_true("table acquire without device undoes insert",
                table.acquire(other) ==
                    BindlessTextureIndexTable::k_fallback_index);
    table.release(nullptr);
    expect_true("table re-acquire after failed insert still fallback",
                table.acquire(other) ==
                    BindlessTextureIndexTable::k_fallback_index);
  }

  g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
