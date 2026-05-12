# Blunder-Engine
玩具游戏引擎

## Build Prerequisites

- CMake 3.21 or newer
- Visual Studio 2022 generator toolchain
- Vulkan SDK exposed via `VULKAN_SDK`
- Rust 1.88 or newer, with `rustc` and `cargo` available in `PATH`

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