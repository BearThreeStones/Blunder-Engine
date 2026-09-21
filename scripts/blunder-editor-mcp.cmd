@echo off
setlocal EnableExtensions
rem Cursor may ignore mcp.json cwd. Always start from this Debug tree so
rem engine/shaders can be found by walking exe parents, then run the editor.
set "DEBUG_DIR=%~dp0..\build\vs2026-debug\bin\Debug"
cd /d "%DEBUG_DIR%" || (
  echo blunder-editor-mcp: failed to cd to "%DEBUG_DIR%" 1>&2
  exit /b 1
)
set "PATH=%DEBUG_DIR%;C:\VulkanSDK\1.4.350.0\Bin;%PATH%"
"engine_editor.exe" %*
