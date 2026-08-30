#include "runtime/resource/asset/mesh_material_override.h"

#include "runtime/function/editor/document_history.h"
#include "runtime/function/render/blinn_phong_editor_settings.h"
#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/forward/forward_shading.h"
#include "runtime/resource/asset/material_asset.h"
#include "runtime/resource/asset/mesh_asset.h"

#include "EASTL/unique_ptr.h"

#include <cmath>
#include <cstdio>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void overlayScalarsAndClearSlot() {
  using namespace Blunder;
  Asset::Meta meta;
  meta.virtual_path = "assets/Meshes/hero.mesh.yaml#mat";
  auto material = eastl::make_shared<MaterialAsset>(
      eastl::move(meta), glm::vec4(0.2f, 0.3f, 0.4f, 1.0f), AssetHandle{},
      nullptr, nullptr, nullptr, nullptr, glm::vec3(0.1f), glm::vec3(0.8f),
      glm::vec3(0.2f), 16.0f, 0.1f, 0.9f, cgltf_alpha_mode_opaque, 0.5f, false,
      false);

  MeshMaterialOverride overlay{};
  overlay.unlit.present = true;
  overlay.unlit.value = true;
  overlay.diffuse.present = true;
  overlay.diffuse.value = glm::vec3(1.0f, 0.0f, 0.0f);
  overlay.shininess.present = true;
  overlay.shininess.value = 48.0f;
  overlay.base_color_texture.present = true;
  overlay.base_color_texture.guid.clear();

  applyMeshMaterialOverride(*material, overlay, {});
  expect_true("overlay unlit", material->isUnlit());
  expect_true("overlay diffuse r", material->getDiffuseColor().x == 1.0f);
  expect_true("overlay shininess", material->getShininess() == 48.0f);
  expect_true("empty slot clears base color texture",
              !material->hasBaseColorTexture());
  expect_true("untouched metallic stays import",
              material->getMetallicFactor() == 0.1f);
}

void applyBlinnPhongIgnoresEditorBag() {
  using namespace Blunder;
  Asset::Meta meta;
  meta.virtual_path = "assets/Meshes/hero.mesh.yaml#mat";
  MaterialAsset material(eastl::move(meta), glm::vec4(1.0f), AssetHandle{},
                         nullptr, nullptr, nullptr, nullptr, glm::vec3(0.05f),
                         glm::vec3(0.1f, 0.2f, 0.3f), glm::vec3(0.7f), 64.0f,
                         1.0f, 1.0f, cgltf_alpha_mode_opaque, 0.5f, false,
                         false);

  BlinnPhongEditorSettings editor{};
  editor.diffuse_color = glm::vec3(9.0f);
  editor.specular_color = glm::vec3(8.0f);
  editor.shininess = 4.0f;
  editor.unlit = true;
  editor.ambient_color = glm::vec3(0.9f);

  ForwardMeshUniformData ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = false;
  applyBlinnPhongToMeshUniforms(ubo, &material, editor, frame);

  expect_true("kd from material not editor",
              ubo.diffuse_color.x == 0.1f && ubo.diffuse_color.y == 0.2f);
  expect_true("shininess from material",
              ubo.specular_color_and_shininess.w == 64.0f);
  expect_true("unlit from material", ubo.material_flags.x == 0.0f);
  expect_true("studio ka from material", ubo.ambient_color.x == 0.05f);

  ForwardMeshUniformData defaults{};
  applyBlinnPhongToMeshUniforms(defaults, nullptr, editor, frame);
  expect_true("null material white kd", defaults.diffuse_color.x == 1.0f);
  expect_true("null material spec 0.4",
              defaults.specular_color_and_shininess.x == 0.4f);
  expect_true("null material shininess 32",
              defaults.specular_color_and_shininess.w == 32.0f);
  expect_true("null material ka 0", defaults.ambient_color.x == 0.0f);
  expect_true("null material not unlit", defaults.material_flags.x == 0.0f);
}

void gltfSpecDefaultMetalWithoutMrMapShadesAsDielectric() {
  using namespace Blunder;
  Asset::Meta meta;
  meta.virtual_path = "assets/Meshes/chocomel.mesh.yaml#mat";
  MaterialAsset omitted_factors(
      eastl::move(meta), glm::vec4(1.0f), AssetHandle{}, nullptr, nullptr, nullptr,
      nullptr, glm::vec3(0.15f), glm::vec3(1.0f), glm::vec3(0.4f), 32.0f, 1.0f,
      1.0f, cgltf_alpha_mode_opaque, 0.5f, false, false);

  ForwardMeshUniformData ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = true;
  applyPbrToMeshUniforms(ubo, &omitted_factors, {}, frame, cgltf_alpha_mode_opaque,
                         0.5f, false);
  expect_true("omitted metal/rough without MR map is dielectric",
              ubo.metallic_roughness_factors.x < 0.01f);
  expect_true("omitted roughness stays 1",
              ubo.metallic_roughness_factors.y > 0.99f);

  Asset::Meta chrome_meta;
  chrome_meta.virtual_path = "assets/Meshes/chrome.mesh.yaml#mat";
  MaterialAsset chrome(eastl::move(chrome_meta), glm::vec4(1.0f), AssetHandle{},
                       nullptr, nullptr, nullptr, nullptr, glm::vec3(0.0f),
                       glm::vec3(1.0f), glm::vec3(1.0f), 256.0f, 1.0f, 0.2f,
                       cgltf_alpha_mode_opaque, 0.5f, false, false);
  ForwardMeshUniformData chrome_ubo{};
  applyPbrToMeshUniforms(chrome_ubo, &chrome, {}, frame, cgltf_alpha_mode_opaque,
                         0.5f, false);
  expect_true("authored metal roughness is kept",
              chrome_ubo.metallic_roughness_factors.x > 0.99f &&
                  std::fabs(chrome_ubo.metallic_roughness_factors.y - 0.2f) <
                      1e-4f);
}

void extraMaterialStaysImport() {
  using namespace Blunder;
  Asset::Meta first_meta;
  first_meta.virtual_path = "assets/Meshes/hero.mesh.yaml#mat0";
  auto first = eastl::make_shared<MaterialAsset>(
      eastl::move(first_meta), glm::vec4(0.2f, 0.3f, 0.4f, 1.0f), AssetHandle{},
      nullptr, nullptr, nullptr, nullptr, glm::vec3(0.1f), glm::vec3(0.8f),
      glm::vec3(0.2f), 16.0f, 0.1f, 0.9f, cgltf_alpha_mode_opaque, 0.5f, false,
      false);
  Asset::Meta second_meta;
  second_meta.virtual_path = "assets/Meshes/hero.mesh.yaml#mat1";
  auto second = eastl::make_shared<MaterialAsset>(
      eastl::move(second_meta), glm::vec4(0.5f), AssetHandle{}, nullptr, nullptr,
      nullptr, nullptr, glm::vec3(0.2f), glm::vec3(0.6f), glm::vec3(0.3f), 8.0f,
      0.2f, 0.8f, cgltf_alpha_mode_opaque, 0.5f, false, false);

  MeshMaterialOverride overlay{};
  overlay.unlit.present = true;
  overlay.unlit.value = true;
  overlay.diffuse.present = true;
  overlay.diffuse.value = glm::vec3(1.0f, 0.0f, 0.0f);
  applyMeshMaterialOverride(*first, overlay, {});
  expect_true("first overlay unlit", first->isUnlit());
  expect_true("second stays lit", !second->isUnlit());
  expect_true("second diffuse unchanged", second->getDiffuseColor().x == 0.6f);
}

void assetInspectorRoutesGlobalHistory() {
  using namespace Blunder;
  expect_true("viewport still document",
              resolveUndoScope(false, false, false) ==
                  EditorUndoScope::document);
  expect_true("browser still global",
              resolveUndoScope(true, false, false) == EditorUndoScope::global);
  expect_true("asset inspector global",
              resolveUndoScope(false, false, true) == EditorUndoScope::global);
  expect_true("rename still text",
              resolveUndoScope(true, true, true) == EditorUndoScope::text);
}

void undoFieldAndResetRestoreBag() {
  using namespace Blunder;

  struct BagCommand final : IEditorCommand {
    MeshMaterialOverride* bag{nullptr};
    MeshMaterialOverride before{};
    MeshMaterialOverride after{};
    void undo() override {
      if (bag != nullptr) {
        *bag = before;
      }
    }
    void redo() override {
      if (bag != nullptr) {
        *bag = after;
      }
    }
  };

  DocumentHistory global;
  DocumentHistory document;

  MeshMaterialOverride bag{};
  auto field = eastl::make_unique<BagCommand>();
  field->bag = &bag;
  field->after.shininess.present = true;
  field->after.shininess.value = 48.0f;
  field->redo();
  global.push(eastl::move(field));
  expect_true("field applied",
              bag.shininess.present && bag.shininess.value == 48.0f);

  MeshMaterialOverride scene_bag{};
  auto scene = eastl::make_unique<BagCommand>();
  scene->bag = &scene_bag;
  scene->after.unlit.present = true;
  scene->after.unlit.value = true;
  scene->redo();
  document.push(eastl::move(scene));

  expect_true("asset inspector routes global",
              resolveUndoScope(false, false, true) == EditorUndoScope::global);
  expect_true("global undo field", global.undo());
  expect_true("field bag restored", !bag.shininess.present);
  expect_true("document history untouched", scene_bag.unlit.present);
  expect_true("document still can undo", document.canUndo());

  bag.shininess.present = true;
  bag.shininess.value = 48.0f;
  auto reset = eastl::make_unique<BagCommand>();
  reset->bag = &bag;
  reset->before = bag;
  reset->after = {};
  reset->redo();
  global.push(eastl::move(reset));
  expect_true("reset cleared bag", bag.empty());
  expect_true("undo reset", global.undo());
  expect_true("reset restored shininess",
              bag.shininess.present && bag.shininess.value == 48.0f);
  expect_true("ctrl+z from asset inspector would not pop document",
              document.canUndo() && scene_bag.unlit.present);
}

}  // namespace

int main() {
  overlayScalarsAndClearSlot();
  extraMaterialStaysImport();
  applyBlinnPhongIgnoresEditorBag();
  gltfSpecDefaultMetalWithoutMrMapShadesAsDielectric();
  assetInspectorRoutesGlobalHistory();
  undoFieldAndResetRestoreBag();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
