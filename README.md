# Blunder-Engine
玩具游戏引擎

## Build Prerequisites

- CMake 4.0 or newer (Windows: use the CMake bundled with Visual Studio 2026)
- Visual Studio 2026 with the **v145** platform toolset (`Visual Studio 18 2026` generator)
- Vulkan SDK exposed via `VULKAN_SDK`
- Rust 1.88 or newer, with `rustc` and `cargo` available in `PATH`

Configure and build from a **Developer PowerShell for VS 2026** (or open the repo folder in
Visual Studio) so the bundled CMake 4.x is on `PATH` ahead of any older install.

```bash
cmake --preset vs2026-debug
cmake --build build/vs2026-debug --config Debug

cmake --preset vs2026-release
cmake --build build/vs2026-release --config Release
```

The editor's Slint integration is source-built during CMake configure so the
Skia Vulkan renderer can be enabled. The current viewport path still uses the
engine's off-screen Vulkan render + CPU readback flow; this change only affects
Slint's composition/present path.

引擎分为 5 层：

平台层（Platfrom Layer）：硬件与操作系统

核心层（Core Layer）：引擎内核与基础服务

资源层（Resource Layer）：模型、纹理、声音等资源管理

功能层（Function Layer）：渲染、物理、音频、网络等具体功能模块

工具层（Tool Layer）：编辑器、调试工具等面向开发者的接口