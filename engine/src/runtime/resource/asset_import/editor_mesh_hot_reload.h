#pragma once

#include "EASTL/string.h"

namespace Blunder {

/// After successful Mesh Reimport: drop CPU/GPU mesh caches and rebind
/// scene MeshRendererComponents that reference the Mesh GUID/path.
/// Fail-soft: logs and leaves prior presentation on error.
void editorMeshHotReloadAfterReimport(const eastl::string& guid,
                                      const eastl::string& descriptor_virtual);

}  // namespace Blunder
