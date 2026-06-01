#pragma once

#include "runtime/function/render/blinn_phong_editor_settings.h"

namespace Blunder {

/// C++ single source of truth for live viewport shading preview.
class EditorPreviewSettings final {
 public:
  const BlinnPhongEditorSettings& get() const { return m_settings; }
  BlinnPhongEditorSettings& mutate() { return m_settings; }
  void set(const BlinnPhongEditorSettings& settings) { m_settings = settings; }

 private:
  BlinnPhongEditorSettings m_settings{};
};

}  // namespace Blunder
