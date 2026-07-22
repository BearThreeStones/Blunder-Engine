#include "runtime/project/project_manager_controller.h"

#include "runtime/project/project_file.h"

namespace Blunder {

ProjectManagerController::ProjectManagerController(
    std::filesystem::path store_path)
    : m_store_path(std::move(store_path)) {}

bool ProjectManagerController::load() { return m_list.load(m_store_path); }

bool ProjectManagerController::save() const {
  return m_list.save(m_store_path);
}

void ProjectManagerController::refreshMissing() { m_list.refreshMissing(); }

void ProjectManagerController::setPendingOpen(
    const std::filesystem::path& root) {
  m_pending_open_root = root;
  m_list.markOpened(root);
  save();
}

bool ProjectManagerController::createProject(const CreateProjectRequest& request,
                                             eastl::string& out_error) {
  ProjectInfo info;
  if (!::Blunder::createProject(request, info, out_error)) {
    return false;
  }
  m_list.addOrUpdate(info.root);
  setPendingOpen(info.root);
  return true;
}

bool ProjectManagerController::importProject(
    const std::filesystem::path& path_or_file, eastl::string& out_error) {
  out_error.clear();
  ProjectInfo info;
  if (!readProjectFile(path_or_file, info)) {
    out_error = "not a Blunder project (missing or invalid project.blunder)";
    return false;
  }
  m_list.addOrUpdate(info.root);
  setPendingOpen(info.root);
  return true;
}

eastl::string ProjectManagerController::openEntry(size_t index) {
  if (index >= m_list.entries().size()) {
    return "invalid project index";
  }
  const ProjectListEntry& entry = m_list.entries()[index];
  if (entry.missing || !isProjectDirectory(entry.path)) {
    return "project is missing or invalid";
  }
  setPendingOpen(entry.path);
  return {};
}

eastl::string ProjectManagerController::openEntryByName(
    const eastl::string& name) {
  for (size_t i = 0; i < m_list.entries().size(); ++i) {
    if (m_list.entries()[i].name == name) {
      return openEntry(i);
    }
  }
  return "project not found";
}

bool ProjectManagerController::removeEntry(size_t index) {
  if (index >= m_list.entries().size()) {
    return false;
  }
  const std::filesystem::path path = m_list.entries()[index].path;
  if (!m_list.remove(path)) {
    return false;
  }
  return save();
}

}  // namespace Blunder
