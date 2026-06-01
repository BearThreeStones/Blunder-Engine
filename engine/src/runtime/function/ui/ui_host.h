#pragma once

#include <cstdint>
#include <optional>

#include "EASTL/shared_ptr.h"

#include "runtime/function/render/blinn_phong_editor_settings.h"
#include "runtime/function/ui/editor_preview_settings.h"
#include "runtime/function/ui/editor_service_handles.h"
#include "runtime/function/ui/ui_context.h"
#include "runtime/function/ui/ui_event_queue.h"
#include "runtime/function/ui/view_models/editor_panels_view_model.h"

namespace Blunder {

class IEditorUiPresentation;
class MaterialAsset;

class UiHost final : public eastl::enable_shared_from_this<UiHost> {
 public:
  UiHost();
  ~UiHost() = default;

  void bindEditorServices(const EditorServiceHandles& handles);
  void setPresentation(IEditorUiPresentation* presentation);
  void shutdown();

  bool isAcceptingCallbacks() const { return m_context.isRunning(); }
  const UiContext& context() const { return m_context; }
  UiContext& context() { return m_context; }

  /// Resolves editor services through weak_ptr; nullopt if shutting down / gone.
  std::optional<UiContext::LockedServices> lockEditorServices() const {
    return m_context.lockEditorServices();
  }

  void enqueue(UiEvent event);
  void drainEventQueue();
  void tickEditorPanels();
  void syncPreviewSettingsFromPresentation();

  const EditorPreviewSettings& previewSettings() const { return m_preview_settings; }
  EditorPreviewSettings& previewSettings() { return m_preview_settings; }

  void setBlinnPhongMaterialSource(const MaterialAsset* material);
  void syncBlinnPhongFromMaterialSource();

  EditorPanelsViewModel& panels() { return m_panels; }

 private:
  void dispatch(const UiEvent& event, const UiContext::LockedServices& services);

  UiContext m_context;
  UiEventQueue m_event_queue;
  EditorPreviewSettings m_preview_settings;
  EditorPanelsViewModel m_panels;
  IEditorUiPresentation* m_presentation{nullptr};
  const MaterialAsset* m_blinn_phong_material_source{nullptr};
};

}  // namespace Blunder
