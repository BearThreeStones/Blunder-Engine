#include "runtime/function/ui/ui_host.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/object/object.h"
#include "runtime/function/editor/align_camera_actions.h"
#include "runtime/function/editor/animation_preview_controller.h"
#include "runtime/function/editor/animation_sync_cine_preview_controller.h"
#include "runtime/function/editor/animation_clip_resolve.h"
#include "runtime/function/editor/animation_tree_canvas_document.h"
#include "runtime/function/editor/content_browser_commands.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/document_history_helpers.h"
#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/hierarchy_system.h"
#include "runtime/function/editor/inspector_asset_ops.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_render_bridge.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/function/ui/editor_ui_presentation.h"
#include "runtime/resource/content_browser/content_browser_system.h"
#include "runtime/project/play_session_controller.h"
#include "runtime/project/play_preflight.h"
#include "runtime/function/script/scripts_builder.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <filesystem>
#include <string>

namespace Blunder {
namespace {

void installScriptsPreflight(PlaySessionController& session,
                             const std::filesystem::path& project_root) {
  session.setScriptsPreflight(
      [project_root]() { return areProjectScriptsDirty(project_root); },
      [project_root](std::string& error) {
        const ScriptsBuildResult built = buildProjectScripts(project_root);
        if (!built.ok) {
          error = built.error.empty() ? "scripts build failed"
                                      : built.error.c_str();
          return false;
        }
        return true;
      });
}

bool startPlaySession(PlaySessionController& session, FileSystem& fs,
                      EditorSceneEditSystem& scene_edit) {
  PlaySessionRequest req;
  req.project_root = fs.getProjectRoot();
  req.scene = scene_edit.activeScenePath().c_str();
  if (req.scene.empty()) {
    LOG_ERROR("[Play] aborted: no active scene path");
    return false;
  }

  AssetManager* asset_manager = g_runtime_global_context.m_asset_manager.get();
  if (asset_manager == nullptr) {
    LOG_ERROR("[Play] aborted: asset manager unavailable");
    return false;
  }
  const auto scene_asset =
      asset_manager->loadScene(eastl::string(req.scene.c_str()));
  if (!scene_asset) {
    LOG_ERROR("[Play] aborted: could not load scene {}", req.scene);
    return false;
  }
  const PlayCameraGateResult cam = runPlayCameraGate(scene_asset->getScene());
  if (!cam.ok) {
    session.setLastIssues(cam.issues);
    LOG_ERROR("[Play] aborted: {}", cam.error.c_str());
    return false;
  }

  installScriptsPreflight(session, req.project_root);
  if (!session.play(req) && !session.lastError().empty()) {
    // Errors stay on the controller; toast surfacing is out of Task 5 scope.
    LOG_ERROR("[Play] failed: {}", session.lastError().c_str());
    return false;
  }
  return session.state() != PlaySessionState::Stopped;
}

}  // namespace

UiHost::UiHost() = default;

void UiHost::bindEditorServices(const EditorServiceHandles& handles) {
  m_context.bindServices(handles);
}

void UiHost::setPresentation(IEditorUiPresentation* presentation) {
  m_presentation = presentation;
}

void UiHost::openSceneAssetPath(const UiContext::LockedServices& services,
                                const eastl::string& path) {
  if (!services.editor_scene_edit || path.empty()) {
    return;
  }
  if (services.editor_scene_edit->isDirty()) {
    setPendingOpenScenePath(path);
    if (m_presentation) {
      m_presentation->showOpenDirtySceneDialog();
    }
    return;
  }
  if (services.editor_scene_edit->openScene(path)) {
    if (services.render_system && services.scene) {
      syncSceneToRender(services.render_system.get(),
                        services.scene->getActiveInstance());
    }
    if (m_presentation) {
      m_presentation->refreshEditorScenePanels();
    }
  }
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
      if (m_presentation) {
        m_presentation->clearAssetInspectorSelection();
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
      if (AnimationPreviewController* preview =
              g_runtime_global_context.m_animation_preview.get()) {
        SceneInstance* scene =
            services.scene ? services.scene->getActiveInstance() : nullptr;
        preview->bindSelection(scene, services.selection->getPrimarySelection());
      }
      if (AnimationSyncCinePreviewController* sync_cine_preview =
              g_runtime_global_context.m_animation_sync_cine_preview.get()) {
        SceneInstance* scene =
            services.scene ? services.scene->getActiveInstance() : nullptr;
        eastl::vector<Object*> objects;
        if (scene != nullptr && services.selection) {
          for (EntityId entity_id : services.selection->getSelectedIds()) {
            if (!isValid(entity_id)) {
              continue;
            }
            Object* object = scene->findBoundObject(entity_id);
            if (object == nullptr) {
              object = scene->ensureBoundObject(entity_id);
            }
            if (object != nullptr && object->hasAnimationPlayer()) {
              wireAnimationPlayerAssetResolver(*object->getAnimationPlayer());
              objects.push_back(object);
            }
          }
        }
        sync_cine_preview->bindObjects(objects);
      }
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
    case UiEventKind::saveSceneAs:
      if (services.editor_scene_edit) {
        const SceneAssetOpResult saved =
            services.editor_scene_edit->saveActiveSceneAs();
        if (saved.success) {
          if (services.content_browser) {
            services.content_browser->refresh();
            m_panels.markDirty(EditorPanelDirty::content_browser);
          }
          if (m_presentation) {
            m_presentation->refreshEditorScenePanels();
          }
        }
      }
      break;
    case UiEventKind::newSceneAsset: {
      if (!services.editor_scene_edit || !services.content_browser) {
        break;
      }
      eastl::string folder = event.path;
      if (folder.empty()) {
        folder = services.content_browser->selectedFolder();
      }
      const SceneAssetOpResult created =
          services.editor_scene_edit->createNewSceneAsset(folder);
      if (!created.success) {
        break;
      }
      services.content_browser->refresh();
      m_panels.markDirty(EditorPanelDirty::content_browser);
      if (m_presentation) {
        m_presentation->syncContentBrowser();
      }
      openSceneAssetPath(services, created.path);
      break;
    }
    case UiEventKind::newFolder: {
      if (!services.content_browser) {
        break;
      }
      eastl::string folder = event.path;
      if (folder.empty()) {
        folder = services.content_browser->selectedFolder();
      }
      g_runtime_global_context.setContentBrowserHasInputFocus(true);
      const ContentBrowserMutateResult created =
          services.content_browser->createFolder(folder);
      if (!created.success) {
        break;
      }
      services.content_browser->setSelectedFolder(folder);
      pushGlobalCommand(makeCreateFolderCommand(services.content_browser.get(),
                                                created.virtual_path));
      m_panels.markDirty(EditorPanelDirty::content_browser);
      if (m_presentation) {
        m_presentation->syncContentBrowser();
        m_presentation->selectBrowserGridPath(created.virtual_path);
      }
      break;
    }
    case UiEventKind::browserRename: {
      if (!services.content_browser || event.path.empty()) {
        break;
      }
      g_runtime_global_context.setContentBrowserHasInputFocus(true);
      services.content_browser->beginInlineRename(event.path);
      g_runtime_global_context.setInlineRenameActive(true);
      m_panels.markDirty(EditorPanelDirty::content_browser);
      if (m_presentation) {
        m_presentation->syncContentBrowser();
      }
      break;
    }
    case UiEventKind::duplicateSceneAsset: {
      if (!services.editor_scene_edit || event.path.empty()) {
        break;
      }
      const SceneAssetOpResult duplicated =
          services.editor_scene_edit->duplicateSceneAsset(event.path);
      if (!duplicated.success) {
        break;
      }
      if (services.content_browser) {
        services.content_browser->refresh();
        m_panels.markDirty(EditorPanelDirty::content_browser);
      }
      if (m_presentation) {
        m_presentation->syncContentBrowser();
        m_presentation->selectBrowserGridPath(duplicated.path);
      }
      break;
    }
    case UiEventKind::undo:
    case UiEventKind::redo: {
      const EditorUndoScope scope = resolveUndoScope(
          g_runtime_global_context.contentBrowserHasInputFocus(),
          g_runtime_global_context.inlineRenameActive(),
          g_runtime_global_context.assetInspectorHasUndoFocus(),
          g_runtime_global_context.attachmentPreviewHasInputFocus());
      if (scope == EditorUndoScope::text) {
        break;
      }
      DocumentHistory* history =
          scope == EditorUndoScope::global
              ? g_runtime_global_context.m_global_history.get()
              : g_runtime_global_context.m_document_history.get();
      if (history == nullptr) {
        break;
      }
      const bool ok = event.kind == UiEventKind::undo ? history->undo()
                                                       : history->redo();
      if (!ok) {
        break;
      }
      if (scope == EditorUndoScope::global) {
        if (services.content_browser) {
          services.content_browser->refresh();
          m_panels.markDirty(EditorPanelDirty::content_browser);
        }
        if (m_presentation) {
          m_presentation->syncContentBrowser();
          m_presentation->syncInspectorFromSelection();
        }
        m_panels.markDirty(EditorPanelDirty::inspector);
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
    case UiEventKind::play: {
      if (AnimationPreviewController* preview =
              g_runtime_global_context.m_animation_preview.get()) {
        preview->stop();
      }
      if (AnimationSyncCinePreviewController* sync_cine_preview =
              g_runtime_global_context.m_animation_sync_cine_preview.get()) {
        sync_cine_preview->stop();
        sync_cine_preview->endCine();
      }
      PlaySessionController* session =
          g_runtime_global_context.m_play_session.get();
      FileSystem* fs = g_runtime_global_context.m_file_system.get();
      if (session == nullptr || fs == nullptr || !services.editor_scene_edit) {
        break;
      }
      const PlayDirtySceneDecision decision = decidePlayDirtyScene(
          services.editor_scene_edit->isDirty(), std::nullopt);
      if (decision.needs_prompt) {
        if (m_presentation) {
          m_presentation->showPlayDirtySceneDialog();
        }
        break;
      }
      (void)startPlaySession(*session, *fs, *services.editor_scene_edit);
      break;
    }
    case UiEventKind::playDirtySaveAndPlay: {
      PlaySessionController* session =
          g_runtime_global_context.m_play_session.get();
      FileSystem* fs = g_runtime_global_context.m_file_system.get();
      if (m_presentation) {
        m_presentation->hidePlayDirtySceneDialog();
      }
      if (session == nullptr || fs == nullptr || !services.editor_scene_edit) {
        break;
      }
      const PlayDirtySceneDecision decision = decidePlayDirtyScene(
          true, PlayDirtySceneChoice::SaveAndPlay);
      if (!decision.proceed) {
        break;
      }
      if (decision.save_first) {
        if (!services.editor_scene_edit->saveActiveScene()) {
          session->setLastError("failed to save active scene before Play");
          break;
        }
      }
      (void)startPlaySession(*session, *fs, *services.editor_scene_edit);
      break;
    }
    case UiEventKind::playDirtyPlayLastSaved: {
      PlaySessionController* session =
          g_runtime_global_context.m_play_session.get();
      FileSystem* fs = g_runtime_global_context.m_file_system.get();
      if (m_presentation) {
        m_presentation->hidePlayDirtySceneDialog();
      }
      if (session == nullptr || fs == nullptr || !services.editor_scene_edit) {
        break;
      }
      const PlayDirtySceneDecision decision = decidePlayDirtyScene(
          true, PlayDirtySceneChoice::PlayLastSaved);
      if (!decision.proceed) {
        break;
      }
      (void)startPlaySession(*session, *fs, *services.editor_scene_edit);
      break;
    }
    case UiEventKind::playDirtyCancel: {
      if (m_presentation) {
        m_presentation->hidePlayDirtySceneDialog();
      }
      break;
    }
    case UiEventKind::detectionReimportAll: {
      if (m_presentation) {
        m_presentation->hideDetectionReimportDialog();
      }
      if (g_runtime_global_context.m_content_browser) {
        (void)g_runtime_global_context.m_content_browser
            ->confirmPendingDetectionReimport();
      }
      break;
    }
    case UiEventKind::detectionReimportDismiss: {
      if (m_presentation) {
        m_presentation->hideDetectionReimportDialog();
      }
      if (g_runtime_global_context.m_content_browser) {
        g_runtime_global_context.m_content_browser
            ->dismissPendingDetectionPrompt();
      }
      break;
    }
    case UiEventKind::playPause: {
      PlaySessionController* session =
          g_runtime_global_context.m_play_session.get();
      if (session == nullptr) {
        break;
      }
      if (session->state() == PlaySessionState::Paused) {
        session->resume();
      } else {
        session->pause();
      }
      break;
    }
    case UiEventKind::playStop: {
      PlaySessionController* session =
          g_runtime_global_context.m_play_session.get();
      if (session != nullptr) {
        session->stop();
      }
      break;
    }
    case UiEventKind::animPreviewPlay: {
      AnimationPreviewController* preview =
          g_runtime_global_context.m_animation_preview.get();
      if (preview == nullptr) {
        break;
      }
      if (!preview->play()) {
        break;
      }
      if (services.render_system) {
        services.render_system->requestViewportRedraw();
      }
      break;
    }
    case UiEventKind::animPreviewPause: {
      AnimationPreviewController* preview =
          g_runtime_global_context.m_animation_preview.get();
      if (preview == nullptr) {
        break;
      }
      if (preview->isPaused()) {
        preview->resume();
      } else {
        preview->pause();
      }
      if (services.render_system) {
        services.render_system->requestViewportRedraw();
      }
      break;
    }
    case UiEventKind::animPreviewStop: {
      AnimationPreviewController* preview =
          g_runtime_global_context.m_animation_preview.get();
      if (preview == nullptr) {
        break;
      }
      preview->stop();
      if (services.render_system) {
        services.render_system->requestViewportRedraw();
      }
      break;
    }
    case UiEventKind::animPreviewLoopToggle: {
      AnimationPreviewController* preview =
          g_runtime_global_context.m_animation_preview.get();
      if (preview == nullptr) {
        break;
      }
      preview->toggleLoop();
      break;
    }
    case UiEventKind::animPreviewParamsEdited:
      if (m_presentation) {
        m_presentation->applyAnimationPreviewParams();
      }
      break;
    case UiEventKind::animPreviewSyncFire:
      if (m_presentation) {
        m_presentation->fireAnimationSyncPreview();
      }
      break;
    case UiEventKind::animPreviewEnterCine: {
      AnimationSyncCinePreviewController* sync_cine_preview =
          g_runtime_global_context.m_animation_sync_cine_preview.get();
      if (sync_cine_preview != nullptr) {
        sync_cine_preview->enterCine(false);
      }
      if (services.render_system) {
        services.render_system->requestViewportRedraw();
      }
      break;
    }
    case UiEventKind::animPreviewEndCine: {
      AnimationSyncCinePreviewController* sync_cine_preview =
          g_runtime_global_context.m_animation_sync_cine_preview.get();
      if (sync_cine_preview != nullptr) {
        sync_cine_preview->endCine();
      }
      if (services.render_system) {
        services.render_system->requestViewportRedraw();
      }
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
    case UiEventKind::browserMeshAssetSelected:
      if (!event.path.empty() &&
          shouldEnterAssetInspectorForBrowserPath(event.path)) {
        if (services.selection &&
            shouldClearEntitySelectionForBrowserAssetPath(event.path)) {
          services.selection->clearSelection();
        }
        if (m_presentation) {
          m_presentation->setAssetInspectorSelection(event.path);
        }
        m_panels.markDirty(EditorPanelDirty::hierarchy);
      }
      m_panels.markDirty(EditorPanelDirty::inspector);
      break;
    case UiEventKind::openSceneAsset:
      openSceneAssetPath(services, event.path);
      break;
    case UiEventKind::openDirtySaveAndOpen: {
      if (m_presentation) {
        m_presentation->hideOpenDirtySceneDialog();
      }
      const eastl::string path = pendingOpenScenePath();
      clearPendingOpenScenePath();
      if (!services.editor_scene_edit || path.empty()) {
        break;
      }
      if (!services.editor_scene_edit->saveActiveScene()) {
        break;
      }
      if (services.editor_scene_edit->openScene(path)) {
        if (services.render_system && services.scene) {
          syncSceneToRender(services.render_system.get(),
                            services.scene->getActiveInstance());
        }
        if (m_presentation) {
          m_presentation->refreshEditorScenePanels();
        }
      }
      break;
    }
    case UiEventKind::openDirtyDiscardAndOpen: {
      if (m_presentation) {
        m_presentation->hideOpenDirtySceneDialog();
      }
      const eastl::string path = pendingOpenScenePath();
      clearPendingOpenScenePath();
      if (!services.editor_scene_edit || path.empty()) {
        break;
      }
      services.editor_scene_edit->clearDirty();
      if (services.editor_scene_edit->openScene(path)) {
        if (services.render_system && services.scene) {
          syncSceneToRender(services.render_system.get(),
                            services.scene->getActiveInstance());
        }
        if (m_presentation) {
          m_presentation->refreshEditorScenePanels();
        }
      }
      break;
    }
    case UiEventKind::openDirtyCancel:
      if (m_presentation) {
        m_presentation->hideOpenDirtySceneDialog();
      }
      clearPendingOpenScenePath();
      break;
    case UiEventKind::openAnimationTreeAsset: {
      FileSystem* file_system = g_runtime_global_context.m_file_system.get();
      if (file_system != nullptr && !event.path.empty()) {
        if (AnimationTreeCanvasSession::instance().openDescriptor(*file_system,
                                                                 event.path)) {
          LOG_INFO("[AnimationTreeCanvas] opened Asset {} ({})",
                   AnimationTreeCanvasSession::instance()
                       .document()
                       .assetGuid()
                       .c_str(),
                   event.path.c_str());
          if (m_presentation) {
            m_presentation->syncAnimationTreeCanvas();
          }
        } else {
          LOG_WARN("[AnimationTreeCanvas] failed to open {}", event.path.c_str());
        }
      }
      break;
    }
    case UiEventKind::openAnimationTreeCanvasFromGuid: {
      FileSystem* file_system = g_runtime_global_context.m_file_system.get();
      AssetManager* asset_manager =
          g_runtime_global_context.m_asset_manager.get();
      AssetRegistry* asset_registry =
          g_runtime_global_context.m_asset_registry.get();
      if (file_system != nullptr && asset_manager != nullptr &&
          asset_registry != nullptr && !event.path.empty()) {
        const eastl::string descriptor_path =
            asset_manager->resolveGuidPath(event.path, *asset_registry);
        if (descriptor_path.empty()) {
          LOG_WARN("[AnimationTreeCanvas] no descriptor for GUID {}",
                   event.path.c_str());
          break;
        }
        if (AnimationTreeCanvasSession::instance().openGuid(
                *file_system, event.path, descriptor_path)) {
          LOG_INFO("[AnimationTreeCanvas] opened from Object GUID {}",
                   event.path.c_str());
          if (m_presentation) {
            m_presentation->syncAnimationTreeCanvas();
          }
        } else {
          LOG_WARN("[AnimationTreeCanvas] openGuid failed for {}",
                   event.path.c_str());
        }
      }
      break;
    }
    case UiEventKind::syncShadingFromAsset:
      syncBlinnPhongFromMaterialSource();
      break;
    case UiEventKind::alignViewToCamera:
    case UiEventKind::alignCameraToView: {
      if (!services.render_system || !services.scene) {
        break;
      }
      EditorCamera* editor_camera = services.render_system->getEditorCamera();
      SceneInstance* scene = services.scene->getActiveInstance();
      if (editor_camera == nullptr || scene == nullptr) {
        break;
      }
      eastl::vector<EntityId> selected_ids;
      if (services.selection) {
        selected_ids = services.selection->getSelectedIds();
      }
      const eastl::span<const EntityId> selection_span(
          selected_ids.data(), selected_ids.size());
      bool applied = false;
      if (event.kind == UiEventKind::alignCameraToView) {
        applied = alignCameraToView(scene, *editor_camera, selection_span);
      } else {
        applied = alignViewToCamera(*editor_camera, *scene, selection_span);
      }
      if (!applied) {
        break;
      }
      services.render_system->requestViewportRedraw();
      if (m_presentation) {
        m_presentation->syncInspectorFromSelection();
      }
      m_panels.markDirty(EditorPanelDirty::inspector);
      break;
    }
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
