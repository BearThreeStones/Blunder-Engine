#pragma once

#include <cstdint>

#include "EASTL/vector.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

/// Result of async multi_peel delivery after sync-then-async left-click.
struct MultiPeelDeliveryAction {
  EntityId selection_entity{k_invalid_entity_id};
  bool apply_selection{false};
  bool request_redraw{true};
};

enum class DeferredPickTickAction : uint8_t {
  hold = 0,
  flush = 1,
};

/// Advances deferred multi_peel scheduling (skip click frame, flush next tickVulkan).
inline DeferredPickTickAction advanceDeferredMultiPeelTick(bool& pending,
                                                           bool& wait_one_tick) {
  if (!pending) {
    return DeferredPickTickAction::hold;
  }
  if (wait_one_tick) {
    wait_one_tick = false;
    return DeferredPickTickAction::hold;
  }
  pending = false;
  return DeferredPickTickAction::flush;
}

/// Decides whether async delivery should change selection or only refresh peel list.
inline MultiPeelDeliveryAction resolveMultiPeelDelivery(
    const EntityId current_primary, const eastl::vector<EntityId>& async_peel_hits) {
  MultiPeelDeliveryAction action{};
  action.request_redraw = true;

  if (async_peel_hits.empty()) {
    if (!isValid(current_primary)) {
      action.apply_selection = true;
      action.selection_entity = k_invalid_entity_id;
    }
    return action;
  }

  const EntityId async_front = async_peel_hits.front();
  if (async_front == current_primary) {
    return action;
  }

  action.apply_selection = true;
  action.selection_entity = async_front;
  return action;
}

}  // namespace Blunder
