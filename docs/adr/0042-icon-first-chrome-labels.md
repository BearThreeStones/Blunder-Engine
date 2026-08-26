# Icon-first chrome labels

Editor **action** chrome that has a dedicated Godot `editor/icons` glyph is **icon-only**: the control shows an **Editor Icon**, and the English name lives in the tooltip (and accessible name), not on the face of the button. That covers Application Bar **Save / Undo / Redo** and the Play / Pause / Stop cluster; viewport transform **Move / Rotate / Scale** and the global/local space toggle; Animation Window transport **Play / Pause / Stop / Loop** plus **Fire / Enter CINE / End CINE**; dock close/pin; Content Browser search/refresh/folder and tree/breadcrumb arrows; Hierarchy kind icons and Inspector scale-link. The Animation Window TimeScale slider may use the Time glyph as its chrome label; the Inspector **TimeScale** property row keeps the word.

**Text stays** wherever the words *are* the identity: dock tab titles (Hierarchy, Inspector, Animation, Output — a leading icon is allowed, it does not replace the title); entity and Asset display names; Inspector property labels and Foldout section titles; dialog and menu copy (**View**, **Save As…**, **Add…**, primary/cancel); status word badges (**CINE**, **Inp**); Clip Binding logical names in dropdowns and the ruler.

Rejected: replacing property labels or object names with glyphs; inventing one-off geometry once a Godot SVG exists for that control; making dock tab titles icon-only; switching the source set to Unity’s Editor Icon Library (already rejected in [ADR 0039](0039-modern-dark-editor-theme.md)).

## Considered Options

- **Keep word buttons on Application Bar and transport (Save, Play, Loop, Fire as text)** — rejected; Godot glyphs already exist, and mixing word Play in the dock with icon Play on the Application Bar splits one affordance.
- **Icon-only dock tabs** — rejected; tab titles name the panel. A leading Editor Icon is optional decoration.
- **Icon-only Inspector labels (TimeScale, Clip Bindings)** — rejected; those words are the property identity, not toolbar actions.
- **Hand-drawn or emoji glyphs for new actions** — rejected; vendor Godot SVGs and `colorize` ([CONTEXT.md — Editor Icon](../../CONTEXT.md)).

## Consequences

New toolbar-class controls ask “is there a Godot SVG?” first. If yes, ship icon-only with tooltip. If no unique glyph (Save As vs Save, View menu, Add…), keep the words. Theme tokens and radius stay [ADR 0039](0039-modern-dark-editor-theme.md). Glossary: `CONTEXT.md` (Icon-first chrome, Editor Icon, Application Bar).
