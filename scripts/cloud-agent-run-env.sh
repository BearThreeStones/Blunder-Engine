# Source this before running Blunder binaries or ctest on Linux:
#   source scripts/cloud-agent-run-env.sh
# Sets DOTNET_ROOT/PATH, DISPLAY, and LD_LIBRARY_PATH for the Slang + Slint SDKs
# that ship as prebuilt shared libraries under .cmake_deps (see cursor-cloud.md).
_blunder_repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)"

export DOTNET_ROOT="${DOTNET_ROOT:-/usr/local/dotnet}"
case ":${PATH}:" in
  *":${DOTNET_ROOT}:"*) ;;
  *) export PATH="${DOTNET_ROOT}:${PATH}" ;;
esac
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1

# The VM provides a virtual X display at :1 for GUI (editor) runs.
export DISPLAY="${DISPLAY:-:1}"

_blunder_slang_dirs="$(find "${_blunder_repo_dir}/.cmake_deps" -name 'libslang.so' -printf '%h:' 2>/dev/null || true)"
_blunder_slint_dirs="$(find "${_blunder_repo_dir}/.cmake_deps" "${_blunder_repo_dir}/build" -name 'libslint_cpp.so' -printf '%h:' 2>/dev/null || true)"
export LD_LIBRARY_PATH="${_blunder_slang_dirs}${_blunder_slint_dirs}${_blunder_repo_dir}/.cmake_deps/slint-sdk-1.16.1/install/lib:${LD_LIBRARY_PATH:-}"

unset _blunder_repo_dir _blunder_slang_dirs _blunder_slint_dirs
