#include "runtime/function/editor/content_browser_commands.h"

#include "runtime/resource/content_browser/content_browser_system.h"

namespace Blunder {

namespace {

class CreateFolderCommand final : public IEditorCommand {
 public:
  ContentBrowserSystem* browser{nullptr};
  eastl::string path;

  void undo() override {
    if (browser != nullptr) {
      browser->removeEmptyFolder(path);
    }
  }

  void redo() override {
    if (browser != nullptr) {
      browser->recreateFolder(path);
    }
  }

  eastl::string label() const override { return eastl::string("New Folder"); }
};

class RenameEntryCommand final : public IEditorCommand {
 public:
  ContentBrowserSystem* browser{nullptr};
  eastl::string from_path;
  eastl::string to_path;
  eastl::string from_name;
  eastl::string to_name;
  bool is_directory{false};

  void undo() override {
    if (browser == nullptr) {
      return;
    }
    browser->renameEntry(to_path, from_name);
  }

  void redo() override {
    if (browser == nullptr) {
      return;
    }
    browser->renameEntry(from_path, to_name);
  }

  eastl::string label() const override {
    return eastl::string(is_directory ? "Rename Folder" : "Rename Asset");
  }
};

class ReparentEntryCommand final : public IEditorCommand {
 public:
  ContentBrowserSystem* browser{nullptr};
  eastl::string from_path;
  eastl::string dest_parent;
  eastl::string original_parent;
  bool is_directory{false};

  void undo() override {
    if (browser == nullptr) {
      return;
    }
    browser->reparentEntry(currentDestPath(), original_parent);
  }

  void redo() override {
    if (browser == nullptr) {
      return;
    }
    browser->reparentEntry(from_path, dest_parent);
  }

  eastl::string label() const override {
    return eastl::string(is_directory ? "Move Folder" : "Move Asset");
  }

 private:
  eastl::string currentDestPath() const {
    eastl::string dest = dest_parent;
    if (!dest.empty() && dest.back() != '/') {
      dest.push_back('/');
    }
    eastl::string name = from_path;
    if (!name.empty() && name.back() == '/') {
      name.pop_back();
    }
    const size_t slash = name.find_last_of('/');
    if (slash != eastl::string::npos) {
      name = name.substr(slash + 1);
    }
    dest.append(name);
    if (is_directory && (dest.empty() || dest.back() != '/')) {
      dest.push_back('/');
    }
    return dest;
  }
};

}  // namespace

eastl::unique_ptr<IEditorCommand> makeCreateFolderCommand(
    ContentBrowserSystem* browser, eastl::string virtual_path) {
  auto command = eastl::make_unique<CreateFolderCommand>();
  command->browser = browser;
  command->path = eastl::move(virtual_path);
  return command;
}

eastl::unique_ptr<IEditorCommand> makeRenameEntryCommand(
    ContentBrowserSystem* browser, eastl::string from_path,
    eastl::string to_path, eastl::string from_name, eastl::string to_name,
    bool is_directory) {
  auto command = eastl::make_unique<RenameEntryCommand>();
  command->browser = browser;
  command->from_path = eastl::move(from_path);
  command->to_path = eastl::move(to_path);
  command->from_name = eastl::move(from_name);
  command->to_name = eastl::move(to_name);
  command->is_directory = is_directory;
  return command;
}

eastl::unique_ptr<IEditorCommand> makeReparentEntryCommand(
    ContentBrowserSystem* browser, eastl::string from_path,
    eastl::string dest_parent, eastl::string original_parent,
    bool is_directory) {
  auto command = eastl::make_unique<ReparentEntryCommand>();
  command->browser = browser;
  command->from_path = eastl::move(from_path);
  command->dest_parent = eastl::move(dest_parent);
  command->original_parent = eastl::move(original_parent);
  command->is_directory = is_directory;
  return command;
}

}  // namespace Blunder
