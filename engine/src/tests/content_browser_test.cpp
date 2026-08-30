#include "runtime/core/log/log_system.h"
#include "runtime/function/editor/content_browser_commands.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/guid.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"
#include "runtime/resource/content/content_entry.h"
#include "runtime/resource/content/content_index.h"
#include "runtime/resource/content_browser/content_browser_delete.h"
#include "runtime/resource/content_browser/content_browser_names.h"
#include "runtime/resource/content_browser/content_browser_system.h"
#include "runtime/resource/content_browser/content_browser_view.h"
#include "runtime/resource/thumbnail/thumbnail_generator.h"

#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_eq_string(const char* label, const eastl::string& got, const char* want) {
  if (got != want) {
    std::fprintf(stderr, "FAIL %s: got '%s' want '%s'\n", label, got.c_str(),
                 want);
    ++g_failures;
  }
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

fs::path makeTempProject() {
  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_content_browser_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Scenes");
  fs::create_directories(root / "Resources");
  {
    std::ofstream out(root / "Assets" / "README.md", std::ios::binary);
    out << "hello\n";
  }
  {
    std::ofstream out(root / "Assets" / "Meshes" / "note.txt", std::ios::binary);
    out << "cube\n";
  }
  {
    std::ofstream out(root / "Assets" / "Scenes" / "pick_test.scene.asset",
                      std::ios::binary);
    out << "{}\n";
  }
  return root;
}

bool hasVirtualPath(const eastl::vector<Blunder::ContentEntry>& entries,
                    const char* path) {
  for (const Blunder::ContentEntry& entry : entries) {
    if (entry.virtual_path == path) {
      return true;
    }
  }
  return false;
}

bool nameTakenNewFolder(const eastl::string& name, void* user) {
  const auto* taken = static_cast<const eastl::vector<eastl::string>*>(user);
  for (const eastl::string& existing : *taken) {
    if (existing == name) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  const eastl::vector<ContentEntry> entries = ContentIndex::scan(file_system);
  expect_true("scan includes synthetic assets/ root",
              hasVirtualPath(entries, "assets/"));
  expect_true("scan directories end with slash (Meshes)",
              hasVirtualPath(entries, "assets/Meshes/"));
  expect_true("scan directories end with slash (Scenes)",
              hasVirtualPath(entries, "assets/Scenes/"));
  expect_true("scan includes file under assets/",
              hasVirtualPath(entries, "assets/README.md"));
  expect_true("scan includes nested file",
              hasVirtualPath(entries, "assets/Meshes/note.txt"));

  bool meshes_is_dir = false;
  for (const ContentEntry& entry : entries) {
    if (entry.virtual_path == "assets/Meshes/") {
      meshes_is_dir = entry.is_directory;
      break;
    }
  }
  expect_true("Meshes entry is directory", meshes_is_dir);

  expect_true(
      "folder classifies as Folder",
      classifyBrowserEntry(true, eastl::string("assets/Meshes/")) ==
          BrowserEntryKind::folder);
  expect_true(
      "mesh.yaml classifies as Mesh",
      classifyBrowserEntry(false, eastl::string("assets/Meshes/a.mesh.yaml")) ==
          BrowserEntryKind::mesh);
  expect_true(
      "mesh.asset classifies as Mesh",
      classifyBrowserEntry(false,
                           eastl::string("assets/Meshes/a.mesh.asset")) ==
          BrowserEntryKind::mesh);
  expect_true(
      "scene.asset classifies as Scene",
      classifyBrowserEntry(false,
                           eastl::string("assets/Scenes/x.scene.asset")) ==
          BrowserEntryKind::scene);
  expect_true(
      "texture.yaml classifies as Texture",
      classifyBrowserEntry(
          false, eastl::string("assets/Textures/a.texture.yaml")) ==
          BrowserEntryKind::texture);
  expect_true(
      "animation.yaml classifies as AnimationClip",
      classifyBrowserEntry(
          false, eastl::string("assets/Animations/w.animation.yaml")) ==
          BrowserEntryKind::animation_clip);
  expect_true(
      "README classifies as File",
      classifyBrowserEntry(false, eastl::string("assets/README.md")) ==
          BrowserEntryKind::file);
  expect_true(
      "animationtree.yaml classifies as File",
      classifyBrowserEntry(
          false, eastl::string("assets/Trees/t.animationtree.yaml")) ==
          BrowserEntryKind::file);
  expect_eq_string("Folder label",
                   eastl::string(browserEntryTypeLabel(BrowserEntryKind::folder)),
                   "Folder");
  expect_eq_string(
      "AnimationClip label",
      eastl::string(browserEntryTypeLabel(BrowserEntryKind::animation_clip)),
      "AnimationClip");

  expect_eq_string("folder size blank", formatBrowserSize(4096, true), "");
  expect_eq_string("folder date blank", formatBrowserDate(1, true), "");
  expect_eq_string("zero date blank", formatBrowserDate(0, false), "");
  expect_eq_string("5 bytes", formatBrowserSize(5, false), "5 B");
  expect_eq_string("12.9 KB", formatBrowserSize(13210, false), "12.9 KB");

  {
    const auto now = std::chrono::file_clock::now();
    const uint64_t ticks =
        static_cast<uint64_t>(now.time_since_epoch().count());
    const eastl::string date_text = formatBrowserDate(ticks, false);
    expect_true("date formatted YYYY-MM-DD HH:MM", date_text.size() == 16);
    expect_true("date has dash", date_text.find("-") != eastl::string::npos);
  }

  {
    eastl::vector<ContentBrowserGridItem> items(4);
    items[0].display_name = "zeta.txt";
    items[0].is_directory = false;
    items[0].type_label = "File";
    items[0].size_bytes = 30;
    items[0].modified_time = 3;
    items[1].display_name = "Meshes";
    items[1].is_directory = true;
    items[1].type_label = "Folder";
    items[1].size_bytes = 0;
    items[1].modified_time = 9;
    items[2].display_name = "alpha.txt";
    items[2].is_directory = false;
    items[2].type_label = "File";
    items[2].size_bytes = 10;
    items[2].modified_time = 1;
    items[3].display_name = "Scenes";
    items[3].is_directory = true;
    items[3].type_label = "Folder";
    items[3].size_bytes = 0;
    items[3].modified_time = 8;

    sortBrowserGridItems(items, BrowserGridSortColumn::name, true);
    expect_eq_string("name folders first 0", items[0].display_name, "Meshes");
    expect_eq_string("name folders first 1", items[1].display_name, "Scenes");
    expect_eq_string("name files 2", items[2].display_name, "alpha.txt");
    expect_eq_string("name files 3", items[3].display_name, "zeta.txt");

    BrowserGridSortColumn column = BrowserGridSortColumn::name;
    bool ascending = true;
    toggleBrowserGridSort(column, ascending, BrowserGridSortColumn::size);
    expect_true("size sort starts ascending",
                column == BrowserGridSortColumn::size && ascending);
    sortBrowserGridItems(items, column, ascending);
    expect_true("size folders first", items[0].is_directory && items[1].is_directory);
    expect_eq_string("size files small first", items[2].display_name, "alpha.txt");
    expect_eq_string("size files large last", items[3].display_name, "zeta.txt");

    toggleBrowserGridSort(column, ascending, BrowserGridSortColumn::size);
    expect_true("second size click reverses",
                column == BrowserGridSortColumn::size && !ascending);
    sortBrowserGridItems(items, column, ascending);
    expect_true("reverse size folders first",
                items[0].is_directory && items[1].is_directory);
    expect_eq_string("reverse size files large first", items[2].display_name,
                     "zeta.txt");
    expect_eq_string("reverse size files small last", items[3].display_name,
                     "alpha.txt");

    toggleBrowserGridSort(column, ascending, BrowserGridSortColumn::date);
    expect_true("date click resets ascending",
                column == BrowserGridSortColumn::date && ascending);
    sortBrowserGridItems(items, column, ascending);
    expect_true("date folders first", items[0].is_directory && items[1].is_directory);
    expect_eq_string("date files old first", items[2].display_name, "alpha.txt");
    expect_eq_string("date files new last", items[3].display_name, "zeta.txt");

    sortBrowserGridItems(items, BrowserGridSortColumn::type, true);
    expect_true("type folders first", items[0].is_directory && items[1].is_directory);
    expect_eq_string("type folder Meshes", items[0].display_name, "Meshes");
    expect_eq_string("type folder Scenes", items[1].display_name, "Scenes");
  }

  {
    const fs::path created = project / "Assets" / "_mkdir_test";
    expect_true("createDirectory succeeds", file_system.createDirectory(created));
    expect_true("createDirectory exists", file_system.isDirectory(created));
    expect_true("createDirectory existing fails",
                !file_system.createDirectory(created));
    expect_true("createDirectory is empty", file_system.isEmptyDirectory(created));
    expect_true("removeEmptyDirectory succeeds",
                file_system.removeEmptyDirectory(created));
    expect_true("removeEmptyDirectory gone", !file_system.exists(created));
  }

  {
    expect_eq_string("trim spaces", trimBrowserEntryName("  Chars  "), "Chars");
    expect_true("legal Chars", isLegalBrowserEntryName("Chars"));
    expect_true("illegal colon", !isLegalBrowserEntryName("Hero:2"));
    expect_true("illegal reserved CON", !isLegalBrowserEntryName("CON"));
    expect_true("illegal trailing dot", !isLegalBrowserEntryName("Hero."));
    expect_true("illegal dot", !isLegalBrowserEntryName("."));
    eastl::vector<eastl::string> taken;
    expect_eq_string("unique first", uniqueNewFolderName(nameTakenNewFolder, &taken),
                     "New Folder");
    taken.push_back("New Folder");
    expect_eq_string("unique second",
                     uniqueNewFolderName(nameTakenNewFolder, &taken),
                     "New Folder_1");
    eastl::string stem;
    eastl::string suffix;
    splitBrowserFileName("Hero.mesh.yaml", stem, suffix);
    expect_eq_string("mesh stem", stem, "Hero");
    expect_eq_string("mesh suffix", suffix, ".mesh.yaml");
  }

  {
    AssetManager asset_manager;
    AssetManagerInitInfo am_init{};
    am_init.file_system = &file_system;
    asset_manager.initialize(am_init);

    ThumbnailGenerator thumbs;
    ThumbnailGeneratorInit thumb_init{};
    thumb_init.file_system = &file_system;
    thumb_init.asset_manager = &asset_manager;
    thumb_init.thumbnail_size = 16;
    thumbs.initialize(thumb_init);

    AssetRegistry registry;
    registry.initialize(&file_system);

    ContentBrowserSystem browser;
    ContentBrowserInit browser_init{};
    browser_init.file_system = &file_system;
    browser_init.asset_manager = &asset_manager;
    browser_init.thumbnail_generator = &thumbs;
    browser_init.asset_registry = &registry;
    browser.initialize(browser_init);
    browser.refresh();

    browser.beginInlineRename("assets/Scenes/pick_test.scene.asset");
    expect_eq_string(
        "closed scene file starts inline rename",
        browser.pendingInlineRenamePath(),
        "assets/Scenes/pick_test.scene.asset");
    browser.beginInlineRename("assets/Scenes/pick_test.scene.asset");
    expect_eq_string(
        "F2-style beginInlineRename is idempotent on the same path",
        browser.pendingInlineRenamePath(),
        "assets/Scenes/pick_test.scene.asset");
    browser.beginInlineRename("assets");
    expect_true("F2 on Assets root clears pending rename",
                browser.pendingInlineRenamePath().empty());
    browser.beginInlineRename("assets/Scenes/pick_test.scene.asset");
    browser.clearPendingInlineRename();
    expect_true("clear pending inline rename",
                browser.pendingInlineRenamePath().empty());

    const ContentBrowserMutateResult created =
        browser.createFolder("assets/");
    expect_true("createFolder success", created.success);
    expect_eq_string("createFolder path", created.virtual_path, "assets/New Folder/");
    expect_eq_string("pending inline rename", browser.pendingInlineRenamePath(),
                     "assets/New Folder/");
    expect_true("new folder on disk",
                file_system.isDirectory(project / "Assets" / "New Folder"));

    {
      DocumentHistory global;
      global.push(makeCreateFolderCommand(&browser, created.virtual_path));
      expect_true("undo create removes dir", global.undo());
      expect_true("dir gone after undo create",
                  !file_system.exists(project / "Assets" / "New Folder"));
      expect_true("redo create restores dir", global.redo());
      expect_true("dir back after redo create",
                  file_system.isDirectory(project / "Assets" / "New Folder"));
    }

    const ContentBrowserMutateResult created_second =
        browser.createFolder("assets/");
    expect_true("createFolder unique suffix success", created_second.success);
    expect_eq_string("createFolder unique suffix", created_second.virtual_path,
                     "assets/New Folder_1/");
    expect_true("remove unique suffix folder",
                browser.removeEmptyFolder(created_second.virtual_path));

    const ContentBrowserMutateResult renamed =
        browser.renameEntry(created.virtual_path, "Chars");
    expect_true("renameFolder success", renamed.success);
    expect_eq_string("renameFolder path", renamed.virtual_path, "assets/Chars/");
    expect_true("renamed folder on disk",
                file_system.isDirectory(project / "Assets" / "Chars"));
    expect_true("old auto name gone",
                !file_system.exists(project / "Assets" / "New Folder"));

    const ContentBrowserMutateResult illegal =
        browser.renameEntry("assets/Chars/", "Hero:2");
    expect_true("illegal rename fails", !illegal.success);
    expect_true("illegal keeps Chars",
                file_system.isDirectory(project / "Assets" / "Chars"));

    browser.createFolder("assets/");
    const ContentBrowserMutateResult collide =
        browser.renameEntry("assets/New Folder/", "Chars");
    expect_true("collision rename fails", !collide.success);

    fs::create_directories(project / "Resources" / "Models" / "Hero");
    {
      std::ofstream gltf(project / "Resources" / "Models" / "Hero" / "Hero.gltf",
                         std::ios::binary);
      gltf << "{}\n";
    }
    const eastl::string guid = generateGuidV4();
    {
      std::ofstream yaml(project / "Assets" / "Chars" / "Hero.mesh.yaml",
                         std::ios::binary);
      yaml << "type: mesh\nguid: " << guid.c_str()
           << "\nsource: resources/Models/Hero/Hero.gltf\n";
    }
    expect_true("register mesh",
                registry.registerAsset(guid, "assets/Chars/Hero.mesh.yaml"));
    browser.refresh();

    const ContentBrowserMutateResult asset_renamed =
        browser.renameEntry("assets/Chars/Hero.mesh.yaml", "Villain");
    expect_true("asset rename success", asset_renamed.success);
    expect_eq_string("asset rename path", asset_renamed.virtual_path,
                     "assets/Chars/Villain.mesh.yaml");
    expect_eq_string("registry new path", registry.resolveGuid(guid),
                     "assets/Chars/Villain.mesh.yaml");
    expect_true("old path unmapped",
                registry.findGuidForPath("assets/Chars/Hero.mesh.yaml").empty());
    expect_true(
        "intermediate unmoved",
        file_system.exists(project / "Resources" / "Models" / "Hero" /
                           "Hero.gltf"));

    fs::create_directories(project / "Assets" / "Enemies");
    browser.refresh();
    expect_true("reparent folder",
                browser.reparentEntry("assets/Chars/", "assets/Enemies/"));
    expect_true("folder moved",
                file_system.isDirectory(project / "Assets" / "Enemies" / "Chars"));
    expect_eq_string("registry after reparent", registry.resolveGuid(guid),
                     "assets/Enemies/Chars/Villain.mesh.yaml");
    expect_true(
        "intermediate still unmoved after reparent",
        file_system.exists(project / "Resources" / "Models" / "Hero" /
                           "Hero.gltf"));

    expect_true("reparent into descendant refused",
                !browser.reparentEntry("assets/Enemies/",
                                       "assets/Enemies/Chars/"));

    AssetCompilerService compiler;
    compiler.initialize(&file_system, &asset_manager, &registry);
    AssetImportService import;
    AssetImportServiceInit import_init{};
    import_init.file_system = &file_system;
    import_init.asset_registry = &registry;
    import_init.content_browser = &browser;
    import_init.asset_compiler = &compiler;
    import.initialize(import_init);

    BrowserDeleteServices del{};
    del.browser = &browser;
    del.import = &import;
    del.registry = &registry;
    del.compiler = &compiler;
    del.file_system = &file_system;

    {
      BrowserDeleteSet root_set =
          buildBrowserDeleteSet(del, {eastl::string("assets/")});
      expect_true("assets root not deletable", !root_set.ok);
    }

    {
      const ContentBrowserMutateResult empty_created =
          browser.createFolder("assets/");
      expect_true("empty folder for delete", empty_created.success);
      BrowserDeleteSet empty_set =
          buildBrowserDeleteSet(del, {empty_created.virtual_path});
      expect_true("empty folder delete ok", empty_set.ok);
      expect_true("empty folder no confirm", !empty_set.needs_confirm);
      BrowserDeleteSnapshot empty_snap;
      eastl::string empty_err;
      expect_true("apply empty folder delete",
                  snapshotAndApplyBrowserDelete(del, empty_set, empty_snap,
                                               &empty_err));
      eastl::string empty_relative = empty_created.virtual_path;
      if (empty_relative.compare(0, 7, "assets/") == 0) {
        empty_relative.erase(0, 7);
      }
      if (!empty_relative.empty() && empty_relative.back() == '/') {
        empty_relative.pop_back();
      }
      const fs::path empty_abs = project / "Assets" / empty_relative.c_str();
      expect_true("empty folder gone", !file_system.exists(empty_abs));
      DocumentHistory delete_history;
      delete_history.push(makeDeleteBrowserEntriesCommand(del, empty_set,
                                                          empty_snap));
      expect_true("undo empty folder delete", delete_history.undo());
      expect_true("empty folder restored", file_system.isDirectory(empty_abs));
    }

    {
      const char* k_tex = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
      const char* k_mesh = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
      fs::create_directories(project / "Assets" / "TexHold");
      fs::create_directories(project / "Resources" / "Textures");
      {
        std::ofstream yaml(project / "Assets" / "TexHold" / "albedo.texture.yaml",
                           std::ios::binary);
        yaml << "type: Texture2D\nguid: " << k_tex
             << "\nsource: Resources/Textures/albedo.png\nimport:\n  srgb: true\n";
      }
      {
        std::ofstream png(project / "Resources" / "Textures" / "albedo.png",
                          std::ios::binary);
        png << "png";
      }
      {
        std::ofstream yaml(project / "Assets" / "Meshes" / "hold.mesh.yaml",
                           std::ios::binary);
        yaml << "type: Mesh\nguid: " << k_mesh
             << "\nsource: Resources/Models/Hero/Hero.gltf\ntexture_guids:\n  - "
             << k_tex << "\nimport:\n  materials: true\n";
      }
      expect_true("register outside texture",
                  registry.registerAsset(k_tex, "assets/TexHold/albedo.texture.yaml"));
      expect_true("register dependent mesh",
                  registry.registerAsset(k_mesh, "assets/Meshes/hold.mesh.yaml"));
      browser.refresh();
      compiler.rebuildDependencyGraph();
      BrowserDeleteSet refuse_set =
          buildBrowserDeleteSet(del, {eastl::string("assets/TexHold/")});
      expect_true("external mesh dependent refuses folder delete", !refuse_set.ok);
    }

    {
      const char* k_mesh = "cccccccc-cccc-4ccc-8ccc-cccccccccccc";
      const char* k_scene = "dddddddd-dddd-4ddd-8ddd-dddddddddddd";
      fs::create_directories(project / "Assets" / "MeshDrop");
      {
        std::ofstream yaml(project / "Assets" / "MeshDrop" / "hero.mesh.yaml",
                           std::ios::binary);
        yaml << "type: Mesh\nguid: " << k_mesh
             << "\nsource: Resources/Models/Hero/Hero.gltf\nimport:\n  materials: true\n";
      }
      {
        std::ofstream json(project / "Assets" / "Scenes" / "detach.scene.asset",
                           std::ios::binary);
        json << "{\n  \"type\": \"Scene\",\n  \"guid\": \"" << k_scene
             << "\",\n  \"entities\": [\n    {\n      \"name\": \"Hero\",\n"
             << "      \"position\": [0, 0, 0],\n      \"rotation\": [0, 0, 0],\n"
             << "      \"rotationMode\": \"euler_degrees\",\n      \"mesh\": \""
             << k_mesh << "\"\n    }\n  ]\n}\n";
      }
      expect_true("register detach mesh",
                  registry.registerAsset(k_mesh, "assets/MeshDrop/hero.mesh.yaml"));
      expect_true("register detach scene",
                  registry.registerAsset(k_scene, "assets/Scenes/detach.scene.asset"));
      browser.refresh();
      compiler.rebuildDependencyGraph();
      BrowserDeleteSet detach_set =
          buildBrowserDeleteSet(del, {eastl::string("assets/MeshDrop/")});
      expect_true("scene dependent allows folder delete", detach_set.ok);
      expect_true("scene dependent needs confirm", detach_set.needs_confirm);
      BrowserDeleteSnapshot detach_snap;
      eastl::string detach_err;
      expect_true("apply scene detach delete",
                  snapshotAndApplyBrowserDelete(del, detach_set, detach_snap,
                                               &detach_err));
      eastl::string scene_text;
      expect_true("read detached scene",
                  file_system.readText(
                      project / "Assets" / "Scenes" / "detach.scene.asset",
                      scene_text));
      expect_true("scene mesh guid detached",
                  scene_text.find(k_mesh) == eastl::string::npos);
      DocumentHistory detach_history;
      detach_history.push(makeDeleteBrowserEntriesCommand(del, detach_set,
                                                          detach_snap));
      expect_true("undo scene detach delete", detach_history.undo());
      expect_true("mesh folder restored",
                  file_system.isDirectory(project / "Assets" / "MeshDrop"));
      expect_true("read restored scene",
                  file_system.readText(
                      project / "Assets" / "Scenes" / "detach.scene.asset",
                      scene_text));
      expect_true("scene mesh guid restored",
                  scene_text.find(k_mesh) != eastl::string::npos);
    }

    {
      const ContentBrowserMutateResult union_folder =
          browser.createFolder("assets/");
      expect_true("union folder create", union_folder.success);
      const char* k_union_tex = "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee";
      {
        std::ofstream yaml(project / "Assets" / "union.texture.yaml",
                           std::ios::binary);
        yaml << "type: Texture2D\nguid: " << k_union_tex
             << "\nsource: Resources/Textures/albedo.png\nimport:\n  srgb: true\n";
      }
      expect_true("register union texture",
                  registry.registerAsset(k_union_tex,
                                         "assets/union.texture.yaml"));
      browser.refresh();
      eastl::vector<eastl::string> union_paths;
      union_paths.push_back(union_folder.virtual_path);
      union_paths.push_back(eastl::string("assets/union.texture.yaml"));
      BrowserDeleteSet union_set = buildBrowserDeleteSet(del, union_paths);
      expect_true("union delete ok", union_set.ok);
      expect_true("union needs confirm", union_set.needs_confirm);
      BrowserDeleteSnapshot union_snap;
      eastl::string union_err;
      expect_true("apply union delete",
                  snapshotAndApplyBrowserDelete(del, union_set, union_snap,
                                               &union_err));
      eastl::string union_relative = union_folder.virtual_path;
      if (union_relative.compare(0, 7, "assets/") == 0) {
        union_relative.erase(0, 7);
      }
      if (!union_relative.empty() && union_relative.back() == '/') {
        union_relative.pop_back();
      }
      expect_true("union folder gone",
                  !file_system.exists(project / "Assets" / union_relative.c_str()));
      expect_true("union texture gone",
                  !file_system.exists(project / "Assets" / "union.texture.yaml"));
      DocumentHistory union_history;
      union_history.push(makeDeleteBrowserEntriesCommand(del, union_set,
                                                         union_snap));
      expect_true("undo union delete", union_history.undo());
      expect_true("union folder restored",
                  file_system.isDirectory(project / "Assets" /
                                          union_relative.c_str()));
      expect_true("union texture restored",
                  file_system.exists(project / "Assets" / "union.texture.yaml"));
    }

    import.shutdown();
    compiler.shutdown();

    browser.shutdown();
    thumbs.shutdown();
    asset_manager.shutdown();
    registry.shutdown();
  }

  file_system.shutdown();
  fs::remove_all(project);
  g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("content_browser_test: all passed\n");
  return 0;
}
