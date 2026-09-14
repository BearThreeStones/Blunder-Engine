# Cursor skill upstreams

GitHub skill packs live here as **git submodules**. Cursor still loads
`.cursor/skills/<name>/SKILL.md` (a short stub that points at the submodule
file). Windows cannot create git symlinks without Administrator, so stubs
are used instead of directory links.

| Submodule | Remote | Pin |
|-----------|--------|-----|
| `gstack` | https://github.com/garrytan/gstack.git | `c8f0c4e` (local install that produced the original copies) |
| `caveman` | https://github.com/JuliusBrussee/caveman.git | `15581d1` (`main`) |
| `ponytail` | https://github.com/DietrichGebert/ponytail.git | `356918e` (`main`) |
| `anthropic-skills` | https://github.com/anthropics/skills.git | `34040c9` (`main`) |
| `mattpocock-skills` | https://github.com/mattpocock/skills.git | `3cca18b` (`main`) |
| `vercel-labs-skills` | https://github.com/vercel-labs/skills.git | `d667282` (`main`) |

Update one pack:

```powershell
git submodule update --remote .cursor/vendor/gstack
git add .cursor/vendor/gstack
```

Clone with skills:

```powershell
git clone --recurse-submodules https://github.com/BearThreeStones/Blunder-Engine.git
# or after a normal clone:
git submodule update --init -- .cursor/vendor
```

`gstack/bin` dist builds, `browse/dist`, and `design/dist` stay gitignored
upstream and are not part of the pin (same binaries skipped when we copied
markdown only).
