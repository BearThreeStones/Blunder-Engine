#pragma once

#include <cstdint>

namespace Blunder::rhi {

enum class RenderBackendType : uint8_t {
  Vulkan = 0,
  D3D12 = 1,
};

enum class PixelFormat : uint8_t {
  R8G8B8A8_UNORM = 0,
};

enum class ShaderBytecodeFormat : uint8_t {
  Spirv = 0,
  Dxil = 1,
};

enum class ShaderStage : uint8_t {
  Vertex = 0,
  Fragment = 1,
};

enum class ResourceState : uint8_t {
  Undefined = 0,
  RenderTarget = 1,
  ShaderRead = 2,
  CopySource = 3,
};

enum class PrimitiveTopology : uint8_t {
  TriangleList = 0,
  LineList = 1,
};

enum class CompareOp : uint8_t {
  LessOrEqual = 0,
};

enum class CullMode : uint8_t {
  None = 0,
  Back = 1,
};

struct Extent2D {
  uint32_t width{0};
  uint32_t height{0};
};

struct ClearColorValue {
  float r{0.0f};
  float g{0.0f};
  float b{0.0f};
  float a{1.0f};
};

struct ClearDepthStencilValue {
  float depth{1.0f};
  uint32_t stencil{0};
};

struct ClearValue {
  ClearColorValue color{};
  ClearDepthStencilValue depth_stencil{};
};

}  // namespace Blunder::rhi
