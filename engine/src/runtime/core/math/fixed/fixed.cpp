#include "core/math/fixed/fixed.h"

#include <cassert>
#include <cstdint>

#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#endif

#if defined(__SIZEOF_INT128__)
#define BLUNDER_FIXED_HAS_INT128 1
#endif

namespace Blunder {
namespace {

#if defined(BLUNDER_FIXED_HAS_INT128)
using Int128 = __int128;
using UInt128 = unsigned __int128;
#endif

#if defined(_MSC_VER) && defined(_M_X64)
uint64_t abs_u64(int64_t value) {
  if (value >= 0) {
    return static_cast<uint64_t>(value);
  }
  if (value == INT64_MIN) {
    return static_cast<uint64_t>(INT64_MAX) + 1ULL;
  }
  return static_cast<uint64_t>(-value);
}

int64_t mul_shift_right_32(int64_t lhs, int64_t rhs) {
  int64_t high = 0;
  const int64_t low = _mul128(lhs, rhs, &high);
  const uint64_t low_u = static_cast<uint64_t>(low);
  const uint64_t high_u = static_cast<uint64_t>(high);
  return static_cast<int64_t>((high_u << 32) | (low_u >> 32));
}

int64_t div_shift_left_32(int64_t lhs, int64_t rhs) {
  assert(rhs != 0);
  const bool negate = (lhs < 0) != (rhs < 0);
  const uint64_t lhs_abs = abs_u64(lhs);
  const uint64_t rhs_abs = abs_u64(rhs);

  int64_t remainder = 0;
  const uint64_t div_hi = lhs_abs >> 32;
  const uint64_t div_lo = lhs_abs << 32;
  int64_t quotient =
      _div128(static_cast<int64_t>(div_hi), static_cast<int64_t>(div_lo), static_cast<int64_t>(rhs_abs), &remainder);
  return negate ? -quotient : quotient;
}

bool u128_square_le(uint64_t value, uint64_t high, uint64_t low) {
  uint64_t sq_hi = 0;
  const uint64_t sq_lo = _umul128(value, value, &sq_hi);
  if (sq_hi != high) {
    return sq_hi < high;
  }
  return sq_lo <= low;
}
#elif defined(BLUNDER_FIXED_HAS_INT128)
int64_t mul_shift_right_32(int64_t lhs, int64_t rhs) {
  return static_cast<int64_t>((static_cast<Int128>(lhs) * static_cast<Int128>(rhs)) >> Fixed::kFracBits);
}

int64_t div_shift_left_32(int64_t lhs, int64_t rhs) {
  assert(rhs != 0);
  return static_cast<int64_t>((static_cast<Int128>(lhs) << Fixed::kFracBits) / static_cast<Int128>(rhs));
}

bool u128_square_le(uint64_t value, uint64_t high, uint64_t low) {
  const UInt128 square = static_cast<UInt128>(value) * static_cast<UInt128>(value);
  const UInt128 target = (static_cast<UInt128>(high) << 64) | low;
  return square <= target;
}
#endif

// Shared deterministic integer sqrt on all platforms (binary search).
uint64_t isqrt_u128(uint64_t high, uint64_t low) {
  if (high == 0 && low == 0) {
    return 0;
  }

  uint64_t lo_bound = 0;
  uint64_t hi_bound = 1;
  while (u128_square_le(hi_bound, high, low)) {
    if (hi_bound > (UINT64_MAX >> 1)) {
      break;
    }
    hi_bound <<= 1;
  }

  while (lo_bound < hi_bound) {
    const uint64_t mid = lo_bound + ((hi_bound - lo_bound + 1) >> 1);
    if (u128_square_le(mid, high, low)) {
      lo_bound = mid;
    } else {
      hi_bound = mid - 1;
    }
  }
  return lo_bound;
}

void u128_from_u64_shift32(uint64_t value, uint64_t& high, uint64_t& low) {
  high = value >> 32;
  low = value << 32;
}

}  // namespace

Fixed Fixed::operator*(Fixed rhs) const { return from_raw(mul_shift_right_32(m_raw, rhs.m_raw)); }

Fixed Fixed::operator/(Fixed rhs) const { return from_raw(div_shift_left_32(m_raw, rhs.m_raw)); }

Fixed sqrt(Fixed value) {
  if (value.raw() <= 0) {
    return Fixed::zero();
  }

  const uint64_t raw_u = static_cast<uint64_t>(value.raw());
  uint64_t high = 0;
  uint64_t low = 0;
  u128_from_u64_shift32(raw_u, high, low);
  return Fixed::from_raw(static_cast<int64_t>(isqrt_u128(high, low)));
}

Fixed inv_sqrt(Fixed value) {
  if (value.raw() <= 0) {
    return Fixed::zero();
  }
  return Fixed::from_int(1) / sqrt(value);
}

Fixed dot(FixedVec3 lhs, FixedVec3 rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

FixedVec3 normalize(FixedVec3 value) {
  const Fixed length_sq = dot(value, value);
  if (length_sq.raw() == 0) {
    return value;
  }
  const Fixed length = sqrt(length_sq);
  return value / length;
}

Fixed dot(FixedQuat lhs, FixedQuat rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

FixedQuat normalize(FixedQuat value) {
  const Fixed length_sq = dot(value, value);
  if (length_sq.raw() == 0) {
    return value;
  }
  const Fixed length = sqrt(length_sq);
  return FixedQuat(value.x / length, value.y / length, value.z / length, value.w / length);
}

}  // namespace Blunder
