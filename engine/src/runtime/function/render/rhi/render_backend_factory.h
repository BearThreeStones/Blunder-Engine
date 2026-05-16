#pragma once

#include "EASTL/unique_ptr.h"

#include "runtime/function/render/rhi/rhi_desc.h"
#include "runtime/function/render/rhi/rhi_types.h"

namespace Blunder::rhi {

class IRenderBackend;

class RenderBackendFactory {
 public:
  static eastl::unique_ptr<IRenderBackend> create(
      RenderBackendType type, const RenderBackendInitInfo& init);
  static eastl::unique_ptr<IRenderBackend> createFromSettings(
      const RenderBackendInitInfo& init);
  static RenderBackendType defaultBackendType();
};

}  // namespace Blunder::rhi
