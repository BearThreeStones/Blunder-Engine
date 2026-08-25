#include "runtime/function/editor/mesh_material_inspector_commands.h"

#include "runtime/function/editor/inspector_asset_ops.h"
#include "runtime/function/editor/inspector_mesh_preview.h"
#include "runtime/resource/asset_manager/asset_manager.h"

namespace Blunder {

namespace {

class MeshMaterialOverrideCommand final : public IEditorCommand {
 public:
  FileSystem* file_system{nullptr};
  AssetManager* asset_manager{nullptr};
  InspectorMeshPreview* mesh_preview{nullptr};
  eastl::string descriptor_virtual_path;
  MeshAssetDescriptor before;
  MeshAssetDescriptor after;
  eastl::string command_label{"Edit Mesh Material"};

  void undo() override { apply(before); }
  void redo() override { apply(after); }
  eastl::string label() const override { return command_label; }

 private:
  void apply(const MeshAssetDescriptor& descriptor) {
    if (file_system == nullptr) {
      return;
    }
    if (!saveMeshAssetDescriptor(descriptor_virtual_path, file_system,
                                 descriptor)) {
      return;
    }
    if (asset_manager != nullptr) {
      asset_manager->refreshMeshMaterialOverride(descriptor_virtual_path);
    }
    if (mesh_preview != nullptr) {
      mesh_preview->markDirty();
    }
  }
};

}  // namespace

eastl::unique_ptr<IEditorCommand> makeMeshMaterialOverrideCommand(
    FileSystem* file_system, AssetManager* asset_manager,
    InspectorMeshPreview* mesh_preview, eastl::string descriptor_virtual_path,
    MeshAssetDescriptor before, MeshAssetDescriptor after,
    eastl::string label) {
  auto command = eastl::make_unique<MeshMaterialOverrideCommand>();
  command->file_system = file_system;
  command->asset_manager = asset_manager;
  command->mesh_preview = mesh_preview;
  command->descriptor_virtual_path = eastl::move(descriptor_virtual_path);
  command->before = eastl::move(before);
  command->after = eastl::move(after);
  if (!label.empty()) {
    command->command_label = eastl::move(label);
  }
  return command;
}

}  // namespace Blunder
