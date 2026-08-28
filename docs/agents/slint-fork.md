# Slint fork (`engine/3rdparty/slint`)

Submodule URL: `https://github.com/BearThreeStones/slint.git`, branch **`blunder/v1.16.1`**
(pinned by commit in the parent repo).

| Remote | URL |
|--------|-----|
| `origin` | BearThreeStones/slint (fork, push patches here) |
| `upstream` | slint-ui/slint (read-only, version bumps) |

## First-time setup (maintainer)

```bash
# On GitHub: fork slint-ui/slint -> BearThreeStones/slint (if not done yet)
cd engine/3rdparty/slint
git push -u origin blunder/v1.16.1
cd ../../..
git submodule update --init engine/3rdparty/slint
```

Linux uses `cmake/slint_linux.cmake` (official C++ SDK tarball) and skips the submodule source build when `Slint::Slint` already exists. Windows still builds the fork from `engine/3rdparty/slint` so Skia Vulkan stays enabled.

## Clone Blunder Engine

```bash
git clone --recurse-submodules https://github.com/BearThreeStones/Blunder-Engine.git
# or after clone: git submodule update --init --recursive engine/3rdparty/slint
```

## Bump Slint version

Rebase `blunder/vX.Y.Z` onto upstream tag, update `SLINT_GIT_TAG` /
`slint.cmake` branch name check, cherry-pick from previous `blunder/*` branch, rebuild `slint_cpp`.

## See also

- [render-pipeline.md](render-pipeline.md)
- [build.md](build.md)
- [cursor-cloud.md](cursor-cloud.md)
- [golden-principles.md](../golden-principles.md)
