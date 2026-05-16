#pragma once

#include <cstdint>

namespace Blunder::rhi {

class IGpuBuffer {
 public:
  virtual ~IGpuBuffer() = default;

  virtual void upload(const void* data, uint64_t size) = 0;
  virtual uint64_t size() const = 0;
};

}  // namespace Blunder::rhi
