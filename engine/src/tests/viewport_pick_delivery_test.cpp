#include <cstdio>

#include "runtime/function/editor/viewport_pick_delivery.h"

namespace {

int g_failures = 0;

void expect_bool(const char* label, const bool actual, const bool expected) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL %s: expected %d got %d\n", label, expected ? 1 : 0,
                 actual ? 1 : 0);
    ++g_failures;
  }
}

void expect_entity(const char* label, const Blunder::EntityId actual,
                   const Blunder::EntityId expected) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL %s: expected entity %u got %u\n", label,
                 static_cast<unsigned>(expected), static_cast<unsigned>(actual));
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    const eastl::vector<EntityId> async_hits{EntityId{10}};
    const MultiPeelDeliveryAction action =
        resolveMultiPeelDelivery(EntityId{10}, async_hits);
    expect_bool("sync match skips apply", action.apply_selection, false);
    expect_bool("sync match redraw", action.request_redraw, true);
  }

  {
    const eastl::vector<EntityId> async_hits{EntityId{20}, EntityId{30}};
    const MultiPeelDeliveryAction action =
        resolveMultiPeelDelivery(EntityId{10}, async_hits);
    expect_bool("async front differs applies", action.apply_selection, true);
    expect_entity("async front entity", action.selection_entity, EntityId{20});
  }

  {
    const eastl::vector<EntityId> async_hits{};
    const MultiPeelDeliveryAction action =
        resolveMultiPeelDelivery(EntityId{10}, async_hits);
    expect_bool("empty async with selection skips apply", action.apply_selection, false);
  }

  {
    const eastl::vector<EntityId> async_hits{};
    const MultiPeelDeliveryAction action =
        resolveMultiPeelDelivery(k_invalid_entity_id, async_hits);
    expect_bool("empty async without selection clears", action.apply_selection, true);
    expect_entity("clear entity", action.selection_entity, k_invalid_entity_id);
  }

  {
    const eastl::vector<EntityId> async_hits{EntityId{42}};
    const MultiPeelDeliveryAction action =
        resolveMultiPeelDelivery(k_invalid_entity_id, async_hits);
    expect_bool("async fallback when sync missed", action.apply_selection, true);
    expect_entity("async fallback entity", action.selection_entity, EntityId{42});
  }

  {
    bool pending = true;
    bool wait_one_tick = true;
    expect_bool("defer tick 0 holds",
                advanceDeferredMultiPeelTick(pending, wait_one_tick) ==
                    DeferredPickTickAction::hold,
                true);
    expect_bool("defer still pending", pending, true);
    expect_bool("defer wait cleared", wait_one_tick, false);
    expect_bool("defer tick 1 flushes",
                advanceDeferredMultiPeelTick(pending, wait_one_tick) ==
                    DeferredPickTickAction::flush,
                true);
    expect_bool("defer cleared", pending, false);
  }

  if (g_failures == 0) {
    std::printf("viewport_pick_delivery_test: all passed\n");
    return 0;
  }
  std::fprintf(stderr, "viewport_pick_delivery_test: %d failure(s)\n", g_failures);
  return 1;
}
