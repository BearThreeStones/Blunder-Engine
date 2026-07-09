#include "runtime/function/render/overlay/outline_id_pack.h"

#include <cassert>

namespace {

void normalSelectionUsesObjectSelectOrange() {
  assert(Blunder::resolveOutlineColorId(false, false) == 1u);
}

void gizmoDraggingUsesTransformOrange() {
  assert(Blunder::resolveOutlineColorId(false, true) == 0u);
}

void translateModalSessionUsesTransformActiveColor() {
  assert(Blunder::resolveOutlineColorId(true, false) == 2u);
}

void translateModalSessionWinsOverGizmoDragging() {
  assert(Blunder::resolveOutlineColorId(true, true) == 2u);
}

}  // namespace

int main() {
  normalSelectionUsesObjectSelectOrange();
  gizmoDraggingUsesTransformOrange();
  translateModalSessionUsesTransformActiveColor();
  translateModalSessionWinsOverGizmoDragging();
  return 0;
}
