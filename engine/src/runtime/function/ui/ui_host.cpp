#include "runtime/function/ui/ui_host.h"

#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/hierarchy_system.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_render_bridge.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/function/ui/editor_ui_presentation.h"
#include "runtime/resource/content_browser/content_browser_system.h"

namespace Blunder {

UiHost::UiHost() = default;

void UiHost::bindEditorServices(const EditorServiceHandles& handles) {
  m_context.bindServices(handles);
}

void UiHost::setPresentation(IEditorUiPresentation* presentation) {
  m_presentation = presentation;
}

void UiHost::shutdown() {
  m_event_queue.clear();
  m_context.beginShutdown();
  m_presentation = nullptr;
  m_blinn_phong_material_source = nullptr;
}

void UiHost::enqueue(UiEvent event) {
  if (!isAcceptingCallbacks()) {
    return;
  }
  m_event_queue.enqueue(eastl::move(event));
}

void UiHost::drainEventQueue() {
  if (!m_context.isRunning()) {
    m_event_queue.clear();
    return;
  }

  const auto services = m_context.lockEditorServices();
  if (!services.has_value()) {
    m_event_queue.clear();
    return;
  }

  UiEvent event;
  while (m_event_queue.tryDequeue(event)) {
    dispatch(event, *services);
  }
}

void UiHost::dispatch(const UiEvent& event, const UiContext::LockedServices& services) {
  switch (event.kind) {
    case UiEventKind::selectEntity: {
      if (!services.selection) {
        return;
      }
      switch (event.selection_mode) {
        case UiSelectionMode::add:
          services.selection->addToSelection(event.entity_id);
          break;
        case UiSelectionMode::toggle:
          services.selection->toggleSelection(event.entity_id);
          break;
        case UiSelectionMode::replace:
        default:
          services.selection->setSelection(event.entity_id);
          break;
      }
      m_panels.markDirty(EditorPanelDirty::inspector);
      m_panels.markDirty(EditorPanelDirty::hierarchy);
      break;
    }
    case UiEventKind::toggleHierarchyNode: {
      if (!services.hierarchy || !services.scene) {
        return;
      }
      services.hierarchy->toggleExpanded(event.entity_id);
      if (SceneInstance* scene = services.scene->getActiveInstance()) {
        services.hierarchy->rebuildVisibleTree(scene);
      }
      m_panels.markDirty(EditorPanelDirty::hierarchy);
      break;
    }
    case UiEventKind::inspectorTransformEdited:
      if (m_presentation) {
        m_presentation->applyInspectorTransform();
      }
      break;
    case UiEventKind::saveScene:
      if (services.editor_scene_edit) {
        services.editor_scene_edit->saveActiveScene();
      }
      break;
    case UiEventKind::undo:
    case UiEventKind::redo: {
      DocumentHistory* history = g_runtime_global_context.m_document_history.get();
      if (history == nullptr) {
        break;
      }
      const bool ok = event.kind == UiEventKind::undo ? history->undo()
                                                       : history->redo();
      if (!ok) {
        break;
      }
      if (services.editor_scene_edit) {
        if (history->isDirtyRelativeToSave()) {
          services.editor_scene_edit->markDirty();
        } else {
          services.editor_scene_edit->clearDirty();
        }
      }
      if (services.hierarchy && services.scene) {
        if (SceneInstance* scene = services.scene->getActiveInstance()) {
          services.hierarchy->rebuildVisibleTree(scene);
          services.hierarchy->markDirty();
        }
      }
      if (m_presentation) {
        m_presentation->syncInspectorFromSelection();
        m_presentation->refreshEditorScenePanels();
      }
      if (services.render_system) {
        services.render_system->requestViewportRedraw();
      }
      m_panels.markDirty(EditorPanelDirty::inspector);
      m_panels.markDirty(EditorPanelDirty::hierarchy);
      break;
    }
    case UiEventKind::browserRefresh:
      if (services.content_browser) {
        services.content_browser->refresh();
        m_panels.markDirty(EditorPanelDirty::content_browser);
      }
      break;
    case UiEventKind::browserFolderSelected:
    case UiEventKind::browserPathSegmentClicked:
      if (services.content_browser && !event.path.empty()) {
        services.content_browser->setSelectedFolder(event.path);
        m_panels.markDirty(EditorPanelDirty::content_browser);
      }
      break;
    case UiEventKind::browserFolderToggle:
      if (services.content_browser && !event.path.empty()) {
        services.content_browser->toggleFolderExpanded(event.path);
        m_panels.markDirty(EditorPanelDirty::content_browser);
      }
      break;
    case UiEventKind::browserSearchChanged:
      if (services.content_browser) {
        services.content_browser->setSearchFilter(event.path);
        m_panels.markDirty(EditorPanelDirty::content_browser);
      }
      break;
    case UiEventKind::openSceneAsset:
      if (services.editor_scene_edit && !event.path.empty()) {
        if (services.editor_scene_edit->openScene(event.path)) {
          if (services.render_system && services.scene) {
            syncSceneToRender(services.render_system.get(),
                              services.scene->getActiveInstance());
          }
          if (m_presentation) {
            m_presentation->refreshEditorScenePanels();
          }
        }
      }
      break;
    case UiEventKind::syncShadingFromAsset:
      syncBlinnPhongFromMaterialSource();
      break;
    case UiEventKind::none:
      break;
  }
}

void UiHost::tickEditorPanels() {
  if (!m_presentation) {
    return;
  }

  const auto services = m_context.lockEditorServices();
  if (!services.has_value()) {
    return;
  }

  if (services->selection && services->selection->isDirty()) {
    m_panels.markDirty(EditorPanelDirty::inspector);
  }
  if (services->hierarchy && services->hierarchy->isDirty()) {
    m_panels.markDirty(EditorPanelDirty::hierarchy);
  }

  if (m_panels.consumeDirty(EditorPanelDirty::inspector)) {
    m_presentation->syncInspectorFromSelection();
  }
  if (m_panels.consumeDirty(EditorPanelDirty::hierarchy)) {
    m_presentation->syncHierarchy();
  }
  if (m_panels.consumeDirty(EditorPanelDirty::content_browser)) {
    m_presentation->syncContentBrowser();
  }

  m_presentation->tickContentBrowserTreePointerPoll();
}

void UiHost::syncPreviewSettingsFromPresentation() {
  if (!m_presentation) {
    return;
  }
  m_preview_settings.set(m_presentation->pullPreviewSettingsFromSlint());
}

void UiHost::setBlinnPhongMaterialSource(const MaterialAsset* material) {
  m_blinn_phong_material_source = material;
}

void UiHost::syncBlinnPhongFromMaterialSource() {
  if (!m_presentation) {
    return;
  }
  m_presentation->setBlinnPhongMaterialSource(m_blinn_phong_material_source);
  m_presentation->syncBlinnPhongFromMaterialSource();
  syncPreviewSettingsFromPresentation();
}

}  // namespace Blunder
