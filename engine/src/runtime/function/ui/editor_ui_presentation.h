#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/render/blinn_phong_editor_settings.h"
#include "runtime/resource/asset_import/asset_import_service.h"

namespace Blunder {

class MaterialAsset;

/// Slint-facing presentation API (implemented by SlintSystem).
class IEditorUiPresentation {
 public:
  virtual ~IEditorUiPresentation() = default;

  virtual void syncHierarchy() = 0;
  virtual void syncInspectorFromSelection() = 0;
  virtual void syncContentBrowser() = 0;
  virtual void applyInspectorTransform() = 0;
  virtual void refreshEditorScenePanels() = 0;

  virtual void setBlinnPhongMaterialSource(const MaterialAsset* material) = 0;
  virtual void syncBlinnPhongFromMaterialSource() = 0;
  virtual BlinnPhongEditorSettings pullPreviewSettingsFromSlint() const = 0;
  virtual void pushPreviewSettingsToSlint(const BlinnPhongEditorSettings& settings) = 0;

  virtual void tickContentBrowserTreePointerPoll() = 0;

  virtual void showPlayDirtySceneDialog() = 0;
  virtual void hidePlayDirtySceneDialog() = 0;
};

}  // namespace Blunder
