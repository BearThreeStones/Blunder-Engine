#include "runtime/core/log/log_system.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/editor/hierarchy_row_icons.h"
#include "runtime/function/editor/inspector_add_ops.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"

#include <glm/gtc/quaternion.hpp>

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

int countKind(const eastl::vector<Blunder::HierarchyRowIconSlot>& icons,
              Blunder::HierarchyRowIconKind kind) {
  int count = 0;
  for (const Blunder::HierarchyRowIconSlot& slot : icons) {
    if (slot.kind == kind) {
      ++count;
    }
  }
  return count;
}

bool hasKind(const eastl::vector<Blunder::HierarchyRowIconSlot>& icons,
             Blunder::HierarchyRowIconKind kind) {
  return countKind(icons, kind) > 0;
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();
  ObjectDB::clear();

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Empty", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    eastl::vector<HierarchyRowIconSlot> icons;
    fillHierarchyRowIcons(scene, id, icons);
    expect_true("empty transform only size", icons.size() == 1);
    expect_true("empty transform kind",
                icons.size() == 1 &&
                    icons[0].kind == HierarchyRowIconKind::Transform);
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Full", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    applyInspectorUniqueAdd(nullptr, scene, id, InspectorUniqueKind::Camera);
    applyInspectorUniqueAdd(nullptr, scene, id, InspectorUniqueKind::Light);
    applyInspectorUniqueAdd(nullptr, scene, id, InspectorUniqueKind::AnimationTree);
    eastl::vector<HierarchyRowIconSlot> icons;
    fillHierarchyRowIcons(scene, id, icons);
    expect_true("uniques transform", hasKind(icons, HierarchyRowIconKind::Transform));
    expect_true("uniques camera", hasKind(icons, HierarchyRowIconKind::Camera));
    expect_true("uniques light", hasKind(icons, HierarchyRowIconKind::Light));
    expect_true("uniques skeleton", hasKind(icons, HierarchyRowIconKind::Skeleton));
    expect_true("uniques tree", hasKind(icons, HierarchyRowIconKind::AnimationTree));
    expect_true("uniques no extra", icons.size() == 5);
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Behaviours", Vec3(0, 0, 0), glm::identity<Quat>(),
                           Vec3(1));
    Object* object = scene.ensureBoundObject(id);
    expect_true("behaviour object", object != nullptr);
    if (object != nullptr) {
      object->addBehaviour("TypeA");
      object->addBehaviour("TypeB");
    }
    eastl::vector<HierarchyRowIconSlot> icons;
    fillHierarchyRowIcons(scene, id, icons);
    expect_true("two behaviour icons",
                countKind(icons, HierarchyRowIconKind::Behaviour) == 2);
    int first = -1;
    int second = -1;
    for (const HierarchyRowIconSlot& slot : icons) {
      if (slot.kind != HierarchyRowIconKind::Behaviour) {
        continue;
      }
      if (first < 0) {
        first = slot.index;
      } else {
        second = slot.index;
      }
    }
    expect_true("behaviour order", first == 0 && second == 1);
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Player", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    applyInspectorUniqueAdd(nullptr, scene, id, InspectorUniqueKind::AnimationPlayer);
    eastl::vector<HierarchyRowIconSlot> icons;
    fillHierarchyRowIcons(scene, id, icons);
    expect_true("player still has skeleton",
                hasKind(icons, HierarchyRowIconKind::Skeleton));
    expect_true("player no tree unless added",
                !hasKind(icons, HierarchyRowIconKind::AnimationTree));
    expect_true("player icon absent", icons.size() == 2);
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Clips", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    applyInspectorUniqueAdd(nullptr, scene, id, InspectorUniqueKind::AnimationTree);
    eastl::vector<AnimationPlayer::ClipBinding> before;
    eastl::vector<AnimationPlayer::ClipBinding> after;
    expect_true(
        "clip binding added",
        applyInspectorAddClipBinding(scene, id, "idle",
                                     "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", before,
                                     after));
    eastl::vector<HierarchyRowIconSlot> icons;
    fillHierarchyRowIcons(scene, id, icons);
    expect_true("clip map still tree icon",
                hasKind(icons, HierarchyRowIconKind::AnimationTree));
    expect_true("clip map no extra icons", icons.size() == 3);
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Mesh", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.setMeshRenderer(id, MeshRendererComponent{});
    eastl::vector<HierarchyRowIconSlot> icons;
    fillHierarchyRowIcons(scene, id, icons);
    expect_true("mesh icon", hasKind(icons, HierarchyRowIconKind::MeshRenderer));
    expect_true("mesh plus transform", icons.size() == 2);
  }

  {
    expect_true("strip empty", hierarchyRowIconStripWidth(0) == 0.0f);
    expect_true("strip one", hierarchyRowIconStripWidth(1) == 16.0f);
    expect_true("strip two", hierarchyRowIconStripWidth(2) == 34.0f);
    expect_true("hit name", hitTestHierarchyRowIconIndex(10.0f, 200.0f, 1) < 0);
    expect_true("hit transform",
                hitTestHierarchyRowIconIndex(190.0f, 200.0f, 1) == 0);
  }

  {
    SceneInstance scene;
    const EntityId a =
        scene.createEntity("A", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const EntityId b =
        scene.createEntity("B", Vec3(2, 0, 0), glm::identity<Quat>(), Vec3(1));
    Entity* entity_b = scene.getEntity(b);
    expect_true("entity b", entity_b != nullptr);
    const Vec3 before = entity_b->getPosition();
    const Vec3 after(5.0f, 0.0f, 0.0f);
    entity_b->setPosition(after);

    DocumentHistory document;
    DocumentHistory global;
    document.push(makeSetEntityTransformCommand(
        &scene, b, before, glm::identity<Quat>(), Vec3(1), after,
        glm::identity<Quat>(), Vec3(1), SelectionSnapshot{a},
        SelectionSnapshot{a}));

    struct MarkerCommand final : IEditorCommand {
      int* value{nullptr};
      void undo() override {
        if (value != nullptr) {
          --(*value);
        }
      }
      void redo() override {
        if (value != nullptr) {
          ++(*value);
        }
      }
    };
    int global_marker = 1;
    auto marker = eastl::make_unique<MarkerCommand>();
    marker->value = &global_marker;
    global.push(eastl::move(marker));

    expect_true("pinned card commit applied",
                scene.getEntity(b) != nullptr &&
                    scene.getEntity(b)->getPosition() == after);
    expect_true("document undo restores other entity", document.undo());
    expect_true("b restored", scene.getEntity(b) != nullptr &&
                                  scene.getEntity(b)->getPosition() == before);
    expect_true("global untouched", global_marker == 1 && global.canUndo());

    expect_true("preview focus is document",
                resolveUndoScope(false, false, true, true) ==
                    EditorUndoScope::document);
    expect_true("asset inspector still global without preview",
                resolveUndoScope(false, false, true, false) ==
                    EditorUndoScope::global);
    expect_true("preview does not pop global", global.canUndo());
  }

  std::fprintf(stdout, "hierarchy_row_icons_test ok\n");
  ObjectDB::clear();
  g_runtime_global_context.m_logger_system.reset();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failures\n", g_failures);
    return 1;
  }
  return 0;
}
