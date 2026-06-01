#pragma once

#include "EASTL/shared_ptr.h"
#include "EASTL/weak_ptr.h"

namespace Blunder {

class EditorSelectionSystem;
class HierarchySystem;
class SceneSystem;
class ContentBrowserSystem;
class EditorSceneEditSystem;
class RenderSystem;
class AssetCompilerService;
class AssetImportService;

struct EditorServiceHandles {
  eastl::weak_ptr<EditorSelectionSystem> selection;
  eastl::weak_ptr<HierarchySystem> hierarchy;
  eastl::weak_ptr<SceneSystem> scene;
  eastl::weak_ptr<ContentBrowserSystem> content_browser;
  eastl::weak_ptr<EditorSceneEditSystem> editor_scene_edit;
  eastl::weak_ptr<RenderSystem> render_system;
  eastl::weak_ptr<AssetCompilerService> asset_compiler;
  eastl::weak_ptr<AssetImportService> asset_import;
};

}  // namespace Blunder
