#pragma once

namespace Blunder::rhi {

class ICommandList {
 public:
  virtual ~ICommandList() = default;

  virtual void begin() = 0;
  virtual void end() = 0;
  virtual void submit() = 0;
};

}  // namespace Blunder::rhi
