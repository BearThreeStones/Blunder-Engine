#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/se_world_flatten.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/project/editor_session_restore.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void printUsage() {
  std::fprintf(
      stderr,
      "Usage: se_world_flatten --godot-root <path> --project-root <path>\n"
      "       [--layout <SE-world.gltf>] [--asset-index <asset_index.json>]\n"
      "       [--out <scene.asset>] [--no-import]\n");
}

std::string fileSha256OrSize(const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) {
    return "missing";
  }
  const auto size = std::filesystem::file_size(path, ec);
  return "size=" + std::to_string(static_cast<unsigned long long>(size));
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path godot_root;
  std::filesystem::path project_root;
  std::filesystem::path layout;
  std::filesystem::path asset_index;
  std::filesystem::path out_scene;
  bool do_import = true;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--godot-root") == 0 && i + 1 < argc) {
      godot_root = argv[++i];
    } else if (std::strcmp(argv[i], "--project-root") == 0 && i + 1 < argc) {
      project_root = argv[++i];
    } else if (std::strcmp(argv[i], "--layout") == 0 && i + 1 < argc) {
      layout = argv[++i];
    } else if (std::strcmp(argv[i], "--asset-index") == 0 && i + 1 < argc) {
      asset_index = argv[++i];
    } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
      out_scene = argv[++i];
    } else if (std::strcmp(argv[i], "--no-import") == 0) {
      do_import = false;
    } else if (std::strcmp(argv[i], "--help") == 0 ||
               std::strcmp(argv[i], "-h") == 0) {
      printUsage();
      return 0;
    }
  }

  if (godot_root.empty() || project_root.empty()) {
    printUsage();
    return 1;
  }
  if (layout.empty()) {
    layout = godot_root / "assets" / "sets" / "world" / "SE-world.gltf";
  }
  if (asset_index.empty()) {
    asset_index = godot_root / "asset_index.json";
  }
  if (out_scene.empty()) {
    out_scene = project_root / "Assets" / "Scenes" / "se-world.scene.asset";
  }

  const std::filesystem::path root_qc =
      project_root / "Assets" / "Scenes" / "root.scene.asset";
  const std::string root_before = fileSha256OrSize(root_qc);

  Blunder::g_runtime_global_context.m_memory_system.initialize();
  Blunder::g_runtime_global_context.m_logger_system =
      eastl::make_shared<Blunder::LogSystem>();

  auto file_system = eastl::make_shared<Blunder::FileSystem>();
  Blunder::FileSystemInitInfo fs_init{};
  fs_init.project_root = project_root;
  file_system->initialize(fs_init);

  Blunder::AssetRegistry registry;
  registry.initialize(file_system.get());

  Blunder::AssetImportService import_service;
  if (do_import) {
    Blunder::AssetImportServiceInit import_init{};
    import_init.file_system = file_system.get();
    import_init.asset_registry = &registry;
    import_service.initialize(import_init);
  }

  Blunder::SeWorldFlattenOptions options;
  options.layout_gltf = layout;
  options.asset_index_json = asset_index;
  options.godot_root = godot_root;
  options.output_scene = out_scene;
  options.file_system = file_system.get();
  options.asset_registry = &registry;
  if (do_import) {
    options.import_service = &import_service;
  }

  const Blunder::SeWorldFlattenStats stats = Blunder::bakeSeWorldFlatten(options);
  bool restore_ok = true;
  if (stats.success && !stats.scene_guid.empty()) {
    const eastl::string guid = stats.scene_guid;
    restore_ok = Blunder::persistEditorSessionRestore(*file_system, &guid, nullptr);
  }

  const std::string root_after = fileSha256OrSize(root_qc);

  std::printf("success=%d\n", stats.success ? 1 : 0);
  std::printf("scene=%s\n", out_scene.generic_string().c_str());
  std::printf("guid=%s\n", stats.scene_guid.c_str());
  std::printf("layout_instances=%d\n", stats.layout_instances);
  std::printf("nested_instances=%d\n", stats.nested_instances);
  std::printf("grouping_entities=%d\n", stats.grouping_entities);
  std::printf("set_geo_mesh_refs=%d\n", stats.set_geo_mesh_refs);
  std::printf("skipped_missing_id=%d\n", stats.skipped_missing_id);
  std::printf("skipped_missing_file=%d\n", stats.skipped_missing_file);
  std::printf("imported_mesh_assets=%d\n", stats.imported_mesh_assets);
  std::printf("root.scene.asset before=%s after=%s\n", root_before.c_str(),
              root_after.c_str());
  if (!stats.error_message.empty()) {
    std::fprintf(stderr, "error: %s\n", stats.error_message.c_str());
  }

  if (do_import) {
    import_service.shutdown();
  }
  registry.shutdown();
  file_system->shutdown();
  Blunder::g_runtime_global_context.m_logger_system.reset();
  Blunder::g_runtime_global_context.m_memory_system.shutdown();

  if (!stats.success) {
    return 2;
  }
  if (root_before != root_after) {
    std::fprintf(stderr, "root.scene.asset changed; abort\n");
    return 3;
  }
  if (!restore_ok) {
    std::fprintf(stderr,
                 "failed to persist se-world.scene.asset as last_live_guid\n");
    return 4;
  }
  return 0;
}
