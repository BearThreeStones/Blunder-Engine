#pragma once

namespace Blunder {

class Texture2DAsset;

namespace rhi {

class IGpuTexture {
 public:
  virtual ~IGpuTexture() = default;
};

}  // namespace rhi
}  // namespace Blunder
