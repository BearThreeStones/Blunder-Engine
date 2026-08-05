#include "core/math/fixed/fixed.h"

#include <cassert>
#include <cstdint>

namespace {

using Blunder::Fixed;
using Blunder::FixedQuat;
using Blunder::FixedVec3;

void scalar_add_one_plus_two_equals_three() {
  const Fixed a = Fixed::from_int(1);
  const Fixed b = Fixed::from_int(2);
  const Fixed c = a + b;
  assert(c == Fixed::from_int(3));
  assert(c.raw() == (3LL << Fixed::kFracBits));
}

void scalar_subtract_three_minus_one_equals_two() {
  const Fixed a = Fixed::from_int(3);
  const Fixed b = Fixed::from_int(1);
  assert(a - b == Fixed::from_int(2));
}

void scalar_multiply_three_times_four_equals_twelve() {
  const Fixed a = Fixed::from_int(3);
  const Fixed b = Fixed::from_int(4);
  assert(a * b == Fixed::from_int(12));
}

void scalar_divide_twelve_by_four_equals_three() {
  const Fixed a = Fixed::from_int(12);
  const Fixed b = Fixed::from_int(4);
  assert(a / b == Fixed::from_int(3));
}

void scalar_divide_one_by_two_has_expected_raw() {
  const Fixed half = Fixed::from_int(1) / Fixed::from_int(2);
  assert(half.raw() == (1LL << (Fixed::kFracBits - 1)));
}

void scalar_multiply_negative_three_times_four_equals_negative_twelve() {
  const Fixed a = -Fixed::from_int(3);
  const Fixed b = Fixed::from_int(4);
  const Fixed c = a * b;
  assert(c.raw() == (-12LL << Fixed::kFracBits));
}

void scalar_multiply_three_times_negative_four_equals_negative_twelve() {
  const Fixed a = Fixed::from_int(3);
  const Fixed b = -Fixed::from_int(4);
  const Fixed c = a * b;
  assert(c.raw() == (-12LL << Fixed::kFracBits));
}

void scalar_multiply_negative_three_times_negative_four_equals_twelve() {
  const Fixed a = -Fixed::from_int(3);
  const Fixed b = -Fixed::from_int(4);
  const Fixed c = a * b;
  assert(c.raw() == (12LL << Fixed::kFracBits));
}

void scalar_divide_negative_twelve_by_four_equals_negative_three() {
  const Fixed a = -Fixed::from_int(12);
  const Fixed b = Fixed::from_int(4);
  const Fixed c = a / b;
  assert(c.raw() == (-3LL << Fixed::kFracBits));
}

void scalar_divide_twelve_by_negative_four_equals_negative_three() {
  const Fixed a = Fixed::from_int(12);
  const Fixed b = -Fixed::from_int(4);
  const Fixed c = a / b;
  assert(c.raw() == (-3LL << Fixed::kFracBits));
}

void scalar_divide_negative_twelve_by_negative_four_equals_three() {
  const Fixed a = -Fixed::from_int(12);
  const Fixed b = -Fixed::from_int(4);
  const Fixed c = a / b;
  assert(c.raw() == (3LL << Fixed::kFracBits));
}

void sqrt_four_has_bit_pattern_of_two() {
  const Fixed two = sqrt(Fixed::from_int(4));
  assert(two.raw() == (2LL << Fixed::kFracBits));
}

void inv_sqrt_four_has_bit_pattern_of_half() {
  const Fixed half = inv_sqrt(Fixed::from_int(4));
  assert(half.raw() == (1LL << (Fixed::kFracBits - 1)));
}

void vec3_normalize_unit_x() {
  const FixedVec3 axis(Fixed::from_int(1), Fixed::zero(), Fixed::zero());
  const FixedVec3 unit = normalize(axis);
  assert(unit.x == Fixed::from_int(1));
  assert(unit.y.raw() == 0);
  assert(unit.z.raw() == 0);
}

void quat_normalize_identity_has_unit_length() {
  const FixedQuat identity =
      FixedQuat(Fixed::zero(), Fixed::zero(), Fixed::zero(), Fixed::from_int(1));
  const FixedQuat unit = normalize(identity);
  const Fixed len_sq = dot(unit, unit);
  assert(len_sq == Fixed::from_int(1));
}

}  // namespace

int main() {
  scalar_add_one_plus_two_equals_three();
  scalar_subtract_three_minus_one_equals_two();
  scalar_multiply_three_times_four_equals_twelve();
  scalar_divide_twelve_by_four_equals_three();
  scalar_divide_one_by_two_has_expected_raw();
  scalar_multiply_negative_three_times_four_equals_negative_twelve();
  scalar_multiply_three_times_negative_four_equals_negative_twelve();
  scalar_multiply_negative_three_times_negative_four_equals_twelve();
  scalar_divide_negative_twelve_by_four_equals_negative_three();
  scalar_divide_twelve_by_negative_four_equals_negative_three();
  scalar_divide_negative_twelve_by_negative_four_equals_three();
  sqrt_four_has_bit_pattern_of_two();
  inv_sqrt_four_has_bit_pattern_of_half();
  vec3_normalize_unit_x();
  quat_normalize_identity_has_unit_length();
  return 0;
}
