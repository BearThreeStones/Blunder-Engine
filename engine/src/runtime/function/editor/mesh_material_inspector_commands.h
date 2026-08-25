#pragma once

#include "EASTL/string.h"
#include "EASTL/unique_ptr.h"

#include "runtime/function/editor/document_history.h"
#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

class AssetManager;
class FileSystem;
class InspectorMeshPreview;

eastl::unique_ptr<IEditorCommand> makeMeshMaterialOverrideCommand(
    FileSystem* file_system, AssetManager* asset_manager,
    InspectorMeshPreview* mesh_preview, eastl::string descriptor_virtual_path,
    MeshAssetDescriptor before, MeshAssetDescriptor after,
    eastl::string label);

}  // namespace Blunder
