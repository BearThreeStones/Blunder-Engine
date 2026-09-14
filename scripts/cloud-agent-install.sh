#!/usr/bin/env bash
# Cloud Agent install for a Linux (GCC + Ninja) build of Blunder Engine.
#
# Mirrors the Merge CI recipe in .github/workflows/merge-ci.yml and
# docs/agents/cursor-cloud.md, and adds the .NET 10 SDK required by the managed
# scripting targets (engine_player POST_BUILD stages Blunder.ScriptHost /
# Blunder.Api, which only build when a net10.0 SDK is on PATH).
#
# Wired into the Cloud Agent environment as: bash scripts/cloud-agent-install.sh
# Idempotent: safe to run repeatedly.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_DIR}"

DOTNET_ROOT="${DOTNET_ROOT:-/usr/local/dotnet}"
export DOTNET_ROOT

log() { printf '\n[cloud-install] %s\n' "$*"; }

# 1. System packages (same set as merge-ci.yml, plus ninja-build).
if ! command -v ninja >/dev/null 2>&1 || ! dpkg -s libvulkan-dev >/dev/null 2>&1; then
  log "Installing system build packages via apt"
  sudo apt-get update
  sudo apt-get install -y --no-install-recommends \
    ninja-build g++ gcc pkg-config python3 clang libclang-dev ca-certificates curl \
    libvulkan-dev mesa-vulkan-drivers libssl-dev \
    libfontconfig1-dev libfreetype6-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
    libxss-dev libxtst-dev libxkbcommon-dev libdrm-dev libgbm-dev \
    libgl1-mesa-dev libegl1-mesa-dev libdbus-1-dev libudev-dev libinput-dev \
    libseat-dev libasound2-dev libpulse-dev libibus-1.0-dev libusb-1.0-0-dev
else
  log "System build packages already present; skipping apt"
fi

# 2. .NET 10 SDK (managed scripting host/API target net10.0). Skip if present.
if [ ! -x "${DOTNET_ROOT}/dotnet" ]; then
  log "Installing .NET 10 SDK into ${DOTNET_ROOT}"
  curl -fsSL https://dot.net/v1/dotnet-install.sh -o /tmp/dotnet-install.sh
  chmod +x /tmp/dotnet-install.sh
  sudo mkdir -p "${DOTNET_ROOT}"
  sudo /tmp/dotnet-install.sh --channel 10.0 --install-dir "${DOTNET_ROOT}"
else
  log ".NET SDK already present at ${DOTNET_ROOT}; skipping"
fi
# Make dotnet discoverable in future shells and this one.
sudo ln -sfn "${DOTNET_ROOT}/dotnet" /usr/local/bin/dotnet
printf 'export DOTNET_ROOT=%s\n' "${DOTNET_ROOT}" | sudo tee /etc/profile.d/blunder-dotnet.sh >/dev/null
export PATH="${DOTNET_ROOT}:${PATH}"
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1

# 3. First-level submodules. A plain init leaves some worktrees empty when a
#    partial checkout recorded the gitlink but not the files (EASTL/glm/cgltf/
#    recastnavigation); repair only those with --force so repeat runs do not
#    rewrite file mtimes needlessly. Nested trees are not needed to build.
log "Syncing git submodules (first level)"
git submodule sync
git submodule update --init
while IFS= read -r _sm_path; do
  if [ -d "${_sm_path}" ] && [ -z "$(ls -A "${_sm_path}" 2>/dev/null)" ]; then
    log "Repairing empty submodule worktree: ${_sm_path}"
    git submodule update --init --force -- "${_sm_path}"
  fi
done < <(git config --file .gitmodules --get-regexp '\.path$' | awk '{print $2}')

# 4. Linux root list. The tracked CMakeLists.txt is the Windows/MSVC root; the
#    Linux build expects a CMakeLists.txt symlink to CMakeLists_linux.cmake.
#    Guard it: recreating the symlink bumps mtime and forces a full CMake re-run.
if [ "$(readlink CMakeLists.txt 2>/dev/null || true)" != "CMakeLists_linux.cmake" ]; then
  log "Linking CMakeLists.txt -> CMakeLists_linux.cmake"
  ln -sfn CMakeLists_linux.cmake CMakeLists.txt
fi

# 5. Configure (Ninja, GCC, Debug). EASTL_USER_DEFINED_ALLOCATOR must be global.
log "Configuring CMake (build/)"
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_C_COMPILER=gcc \
  -DSDL_WAYLAND=OFF \
  -DCMAKE_CXX_FLAGS="-DEASTL_USER_DEFINED_ALLOCATOR"

# 6. Build the full tree (editor, player, managed DLLs, tests).
log "Building (cmake --build build)"
cmake --build build

log "Install complete. 'source scripts/cloud-agent-run-env.sh' to set runtime paths before running binaries or ctest."
