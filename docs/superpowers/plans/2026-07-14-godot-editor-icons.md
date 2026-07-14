# Godot Editor Icons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Vendor Godot’s `editor/icons` SVGs into Blunder and replace dock chrome close/pin plus Content Browser / Inspector emoji-Unicode affordances with colorized Slint `Image` components.

**Architecture:** Copy the full Godot editor icon pack to `engine/3rdparty/godot-icons/`. Dedicated Slint components wrap `@image-url` + `Image.colorize` while preserving existing `icon-color` / `icon-size` APIs. Only SVGs referenced from Slint embed into the binary. Minimize stays hand-drawn geometry.

**Tech Stack:** Slint UI, Godot MIT SVGs, CMake/`slint_target_sources` (transitive imports via existing `.slint` modules), MSVC Debug `engine_editor`.

**Decisions (grilled):** See `CONTEXT.md` § Editor icons; OpenSpec `openspec/changes/godot-editor-icons/`.

**OpenSpec:** `openspec/changes/godot-editor-icons/` (proposal / design / specs / tasks). Apply with `/opsx:apply`.

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/3rdparty/godot-icons/*.svg` | Vendored Godot `editor/icons` pack (~1037) |
| `engine/3rdparty/godot-icons/LICENSE` | MIT copyright notice |
| `engine/3rdparty/godot-icons/README.md` | Upstream source + embed policy |
| `engine/src/runtime/function/slint/dock_chrome_icons.slint` | Close/Pin → SVG; Minimize stays geometry |
| `engine/src/runtime/function/slint/editor_icons.slint` | Search/Reload/Folder/Tree arrows/Instance/Unlinked |
| `engine/src/runtime/function/slint/docking_panel.slint` | Already uses DockChrome*; no API change expected |
| `engine/src/runtime/function/slint/content_browser.slint` | Replace emoji/Unicode icon `Text` nodes |
| `engine/src/runtime/function/slint/inspector_panel.slint` | Scale link + Transform chevron |
| `CONTEXT.md` | Already updated (Editor Icon, Scale-link icons) |
| `openspec/changes/godot-editor-icons/` | Spec artifacts |

**Out of scope:** Transform toolbar Move/Rotate/Scale text buttons; `scene/theme/icons`; embedding all SVGs; generic `EditorIcon { name }` registry.

**Path rule:** From `engine/src/runtime/function/slint/`, Godot SVG URL is:

```text
../../../../3rdparty/godot-icons/<FileName>.svg
```

---

### Task 1: Vendor icon pack + license

**Files:**
- Create: `engine/3rdparty/godot-icons/` (all SVGs + LICENSE + README)
- Source: `E:\Dev\godot\editor\icons\*.svg` (local Godot checkout; copy once into repo)

- [ ] **Step 1: Create directory and copy SVGs**

```powershell
New-Item -ItemType Directory -Force -Path "E:\Dev\Blunder-Engine\engine\3rdparty\godot-icons" | Out-Null
Copy-Item "E:\Dev\godot\editor\icons\*.svg" "E:\Dev\Blunder-Engine\engine\3rdparty\godot-icons\" -Force
(Get-ChildItem "E:\Dev\Blunder-Engine\engine\3rdparty\godot-icons\*.svg").Count
```

Expected: count ≈ 1037 (exact Godot revision may differ slightly).

- [ ] **Step 2: Verify required filenames exist**

```powershell
@(
  'GuiClose.svg','Pin.svg','Search.svg','Reload.svg','Folder.svg',
  'GuiTreeArrowRight.svg','GuiTreeArrowDown.svg','Instance.svg','Unlinked.svg'
) | ForEach-Object {
  if (-not (Test-Path "E:\Dev\Blunder-Engine\engine\3rdparty\godot-icons\$_")) {
    throw "Missing $_"
  }
  "OK $_"
}
```

Expected: each line `OK …`.

- [ ] **Step 3: Write LICENSE**

Create `engine/3rdparty/godot-icons/LICENSE` with Godot MIT text:

```text
Copyright (c) 2014-present Godot Engine contributors.
Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

- [ ] **Step 4: Write README.md**

Create `engine/3rdparty/godot-icons/README.md`:

```markdown
# Godot editor icons (vendored)

SVG icons copied from Godot Engine `editor/icons/` (MIT).

Upstream: https://github.com/godotengine/godot (path `editor/icons/`)

Blunder keeps the full pack on disk for future UI work. Slint embeds **only**
SVGs referenced via `@image-url` in compiled `.slint` modules — do not add
bulk unused `@image-url` references.

Local refresh (optional): copy again from a Godot checkout’s `editor/icons/`.
```

- [ ] **Step 5: Commit vendored pack**

```powershell
git add engine/3rdparty/godot-icons
git commit -m "$(cat <<'EOF'
chore: vendor Godot editor/icons SVGs under 3rdparty

EOF
)"
```

---

### Task 2: Dock close/pin → Godot SVG

**Files:**
- Modify: `engine/src/runtime/function/slint/dock_chrome_icons.slint`
- Unchanged API consumers: `engine/src/runtime/function/slint/docking_panel.slint`

- [ ] **Step 1: Replace `DockChromeCloseIcon` body with Image**

Keep the export name and properties. Replace the two rotated `Rectangle` children with:

```slint
export component DockChromeCloseIcon inherits Rectangle {
    in property <color> icon-color: #808080;
    in property <length> icon-size: 10px;

    width: icon-size;
    height: icon-size;
    background: transparent;

    Image {
        width: 100%;
        height: 100%;
        source: @image-url("../../../../3rdparty/godot-icons/GuiClose.svg");
        colorize: root.icon-color;
        image-fit: contain;
    }
}
```

- [ ] **Step 2: Replace `DockChromePinIcon` body with Image**

```slint
export component DockChromePinIcon inherits Rectangle {
    in property <color> icon-color: #808080;
    in property <length> icon-size: 10px;

    width: icon-size;
    height: icon-size;
    background: transparent;

    Image {
        width: 100%;
        height: 100%;
        source: @image-url("../../../../3rdparty/godot-icons/Pin.svg");
        colorize: root.icon-color;
        image-fit: contain;
    }
}
```

- [ ] **Step 3: Leave `DockChromeMinimizeIcon` unchanged** (hand-drawn horizontal bar).

- [ ] **Step 4: Build to verify Slint compiles**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_editor
```

Expected: success. If `@image-url` path fails, confirm four `../` segments from `function/slint/` to `engine/`.

- [ ] **Step 5: Commit**

```powershell
git add engine/src/runtime/function/slint/dock_chrome_icons.slint
git commit -m "$(cat <<'EOF'
feat: use Godot GuiClose/Pin SVGs for dock chrome icons

EOF
)"
```

---

### Task 3: Add `editor_icons.slint` panel components

**Files:**
- Create: `engine/src/runtime/function/slint/editor_icons.slint`

- [ ] **Step 1: Create the module with seven components**

```slint
// Godot editor/icons wrappers for Content Browser + Inspector.
// Paths are relative to this file under function/slint/.

export component EditorIconSearch inherits Rectangle {
    in property <color> icon-color: #808080;
    in property <length> icon-size: 12px;
    width: icon-size;
    height: icon-size;
    background: transparent;
    Image {
        width: 100%; height: 100%;
        source: @image-url("../../../../3rdparty/godot-icons/Search.svg");
        colorize: root.icon-color;
        image-fit: contain;
    }
}

export component EditorIconReload inherits Rectangle {
    in property <color> icon-color: #b0b0b0;
    in property <length> icon-size: 14px;
    width: icon-size;
    height: icon-size;
    background: transparent;
    Image {
        width: 100%; height: 100%;
        source: @image-url("../../../../3rdparty/godot-icons/Reload.svg");
        colorize: root.icon-color;
        image-fit: contain;
    }
}

export component EditorIconFolder inherits Rectangle {
    in property <color> icon-color: #d0d0d0;
    in property <length> icon-size: 14px;
    width: icon-size;
    height: icon-size;
    background: transparent;
    Image {
        width: 100%; height: 100%;
        source: @image-url("../../../../3rdparty/godot-icons/Folder.svg");
        colorize: root.icon-color;
        image-fit: contain;
    }
}

export component EditorIconTreeArrowRight inherits Rectangle {
    in property <color> icon-color: #707070;
    in property <length> icon-size: 10px;
    width: icon-size;
    height: icon-size;
    background: transparent;
    Image {
        width: 100%; height: 100%;
        source: @image-url("../../../../3rdparty/godot-icons/GuiTreeArrowRight.svg");
        colorize: root.icon-color;
        image-fit: contain;
    }
}

export component EditorIconTreeArrowDown inherits Rectangle {
    in property <color> icon-color: #707070;
    in property <length> icon-size: 10px;
    width: icon-size;
    height: icon-size;
    background: transparent;
    Image {
        width: 100%; height: 100%;
        source: @image-url("../../../../3rdparty/godot-icons/GuiTreeArrowDown.svg");
        colorize: root.icon-color;
        image-fit: contain;
    }
}

export component EditorIconInstance inherits Rectangle {
    in property <color> icon-color: #e0e0e0;
    in property <length> icon-size: 14px;
    width: icon-size;
    height: icon-size;
    background: transparent;
    Image {
        width: 100%; height: 100%;
        source: @image-url("../../../../3rdparty/godot-icons/Instance.svg");
        colorize: root.icon-color;
        image-fit: contain;
    }
}

export component EditorIconUnlinked inherits Rectangle {
    in property <color> icon-color: #808080;
    in property <length> icon-size: 14px;
    width: icon-size;
    height: icon-size;
    background: transparent;
    Image {
        width: 100%; height: 100%;
        source: @image-url("../../../../3rdparty/godot-icons/Unlinked.svg");
        colorize: root.icon-color;
        image-fit: contain;
    }
}
```

No CMake list edit required if this file is only `import`ed from already-listed Slint sources (`content_browser.slint`, `inspector_panel.slint`).

- [ ] **Step 2: Commit**

```powershell
git add engine/src/runtime/function/slint/editor_icons.slint
git commit -m "$(cat <<'EOF'
feat: add Slint Editor Icon wrappers for Godot SVGs

EOF
)"
```

---

### Task 4: Wire Content Browser

**Files:**
- Modify: `engine/src/runtime/function/slint/content_browser.slint`

- [ ] **Step 1: Add import at top of file**

```slint
import {
    EditorIconSearch,
    EditorIconReload,
    EditorIconFolder,
    EditorIconTreeArrowRight,
    EditorIconTreeArrowDown,
} from "editor_icons.slint";
```

- [ ] **Step 2: Replace search-box glyph (~line 104)**

Replace the `Text { text: "🔍"; … }` with:

```slint
EditorIconSearch {
    icon-size: 12px;
    icon-color: #808080;
    width: 18px;
    vertical-alignment: center; // if layout requires, wrap in VerticalLayout / use y centering via parent
}
```

If `vertical-alignment` is not valid on `Rectangle`, center with:

```slint
HorizontalLayout {
    width: 18px;
    alignment: center;
    EditorIconSearch { icon-size: 12px; icon-color: #808080; }
}
```

Match existing layout: keep the 18px width slot.

- [ ] **Step 3: Replace refresh glyph (~line 137)**

Inside `refresh-btn`, replace `Text { text: "⟳"; … }` with centered:

```slint
EditorIconReload {
    icon-size: 14px;
    icon-color: #b0b0b0;
    x: (parent.width - self.width) / 2;
    y: (parent.height - self.height) / 2;
}
```

- [ ] **Step 4: Replace tree expand/collapse Text (~lines 284–291)**

```slint
if row.has-children && row.expanded: EditorIconTreeArrowDown {
    icon-size: 10px;
    icon-color: #707070;
    x: (parent.width - self.width) / 2;
    y: (parent.height - self.height) / 2;
}
if row.has-children && !row.expanded: EditorIconTreeArrowRight {
    icon-size: 10px;
    icon-color: #707070;
    x: (parent.width - self.width) / 2;
    y: (parent.height - self.height) / 2;
}
```

Keep the surrounding `TouchArea` and `folder-toggle` callback unchanged.

- [ ] **Step 5: Replace tree folder emoji (~line 303)**

```slint
EditorIconFolder {
    icon-size: 12px;
    icon-color: #d0d0d0;
    width: 18px;
}
```

- [ ] **Step 6: Replace grid directory emoji (~line 403)**

```slint
if tile.is-dir: EditorIconFolder {
    icon-size: root.thumb-size * 0.4;
    icon-color: #d0d0d0;
    x: (parent.width - self.width) / 2;
    y: (parent.height - self.height) / 2;
}
```

- [ ] **Step 7: Replace breadcrumb `›` (~line 200)**

```slint
if seg-index > 0: EditorIconTreeArrowRight {
    icon-size: 10px;
    icon-color: #505050;
    vertical-alignment: center; // or center via layout padding
}
```

If `ArrowRight.svg` looks clearer at 10px during visual QA, switch only this call site’s `@image-url` / component to `ArrowRight` (add `EditorIconArrowRight` if needed). Default remains `GuiTreeArrowRight`.

- [ ] **Step 8: Replace thumb-size search emoji (~line 485)**

```slint
EditorIconSearch {
    icon-size: 10px;
    icon-color: #606060;
    width: 16px;
}
```

- [ ] **Step 9: Build**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_editor
```

Expected: success.

- [ ] **Step 10: Commit**

```powershell
git add engine/src/runtime/function/slint/content_browser.slint
git commit -m "$(cat <<'EOF'
feat: replace Content Browser emoji icons with Godot SVGs

EOF
)"
```

---

### Task 5: Wire Inspector

**Files:**
- Modify: `engine/src/runtime/function/slint/inspector_panel.slint`

- [ ] **Step 1: Add import**

```slint
import {
    EditorIconInstance,
    EditorIconUnlinked,
    EditorIconTreeArrowRight,
    EditorIconTreeArrowDown,
} from "editor_icons.slint";
```

- [ ] **Step 2: Replace Transform header chevrons (~lines 135–140)**

```slint
if root.transform-expanded: EditorIconTreeArrowDown {
    icon-size: 12px;
    icon-color: #f0f0f0;
}
if !root.transform-expanded: EditorIconTreeArrowRight {
    icon-size: 12px;
    icon-color: #f0f0f0;
}
```

Keep the header `TouchArea` `clicked` toggle unchanged.

- [ ] **Step 3: Replace Scale link chain emoji (~line 329)**

Inside the 28×22 scale-link button `Rectangle`, replace `Text { text: "⛓"; … }` with:

```slint
if root.scale-link-enabled: EditorIconInstance {
    icon-size: 14px;
    icon-color: #e0e0e0;
    x: (parent.width - self.width) / 2;
    y: (parent.height - self.height) / 2;
}
if !root.scale-link-enabled: EditorIconUnlinked {
    icon-size: 14px;
    icon-color: #808080;
    x: (parent.width - self.width) / 2;
    y: (parent.height - self.height) / 2;
}
```

Do not change the `TouchArea` that toggles `scale-link-enabled` or any C++ callbacks.

- [ ] **Step 4: Build**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_editor
```

- [ ] **Step 5: Commit**

```powershell
git add engine/src/runtime/function/slint/inspector_panel.slint
git commit -m "$(cat <<'EOF'
feat: use Godot Instance/Unlinked and tree arrows in Inspector

EOF
)"
```

---

### Task 6: Visual verification checklist

**Files:** none (manual QA)

- [ ] **Step 1: Run editor**

```powershell
./build/vs2026-debug/engine/src/editor/Debug/engine_editor.exe
```

- [ ] **Step 2: Check each surface**

| Surface | Expect |
|---------|--------|
| Dock tab close | Godot × (`GuiClose`), brightens on hover, still closes |
| Dock tab pin | Godot pin, still pins |
| Auto-hide strip unpin | Close SVG |
| Auto-hide overlay | Minimize = dash geometry; close = `GuiClose` |
| Content Browser search/refresh | SVG icons, refresh still refreshes |
| Tree expand + folder | Arrows + Folder SVG |
| Breadcrumb separators | Arrow SVG between segments |
| Grid directory tiles | Folder SVG |
| Inspector Scale link | `Instance` when on, `Unlinked` when off; link behavior unchanged |
| Transform section header | Tree arrows; expand/collapse still works |

- [ ] **Step 3: If colorize fails on a glyph**

Swap to another Godot SVG in the same family or temporarily keep geometry for that one control only; do not revert the whole change. Document the swap in the PR / commit message.

- [ ] **Step 4: Final docs commit (if OpenSpec tasks checkboxes updated)**

```powershell
git add openspec/changes/godot-editor-icons/tasks.md
git commit -m "$(cat <<'EOF'
docs: mark godot-editor-icons implementation tasks done

EOF
)"
```

When implementation is complete, archive with `/opsx:archive`.

---

## Self-review (plan vs grilled consensus)

| Consensus item | Task coverage |
|----------------|---------------|
| Scope B (dock + emoji/Unicode, not toolbar text) | Tasks 2–5; out-of-scope called out |
| Vendor full `editor/icons` only | Task 1 |
| Path `engine/3rdparty/godot-icons/` + short LICENSE/README | Task 1 |
| Compile only referenced SVGs | README + no bulk `@image-url` |
| Image + colorize | Tasks 2–3 |
| Dedicated components | Tasks 2–3 |
| GuiClose / Pin / minimize geometry | Task 2 |
| Search / Reload / Folder | Task 4 |
| Tree + breadcrumb arrows | Task 4 |
| Instance / Unlinked | Task 5 |
| Inspector Transform chevrons | Task 5 |

No TBD placeholders. TDD: UI-only visual change — verification is build + visual checklist (no unit test harness for Slint glyphs in-repo).
