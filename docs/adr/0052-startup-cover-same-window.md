# Startup cover lives on the Editor Session window

A windowed Editor Session shows a **Startup cover** on the same OS window that will host the Editor Shell: the window appears as soon as that session can show one, and the cover stays until the Shell is on screen. Rejected: a second splash window; hiding the window until the Shell can Present; covering Project Manager, Player, or Headless in this cut.

## Considered Options

- **Separate splash window** — rejected; two taskbar entries and a size jump when the editor appears.
- **Hide the window until Shell Present** — rejected; that removes the black HWND but is not a cover, and the long cook wait stays invisible.
