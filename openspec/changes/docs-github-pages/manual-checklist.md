# Manual checklist — docs-github-pages

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Live URL: `https://bearthreestones.github.io/Blunder-Engine/`

One-time: GitHub repo **Settings → Pages → Source = GitHub Actions**.

| # | User story | Pass |
|---|------------|------|
| 1 | Open the Pages URL; home is the short map, not README CMake steps | |
| 2 | From the map, open AGENTS, CONTEXT, CONTENT_LAYOUT, golden-principles, overview, workflow, design-docs; each renders | |
| 3 | From a `docs/agents/` page, follow a repo-relative link to AGENTS.md; it still works | |
| 4 | A docs change on `main` updates the site; a PR does not publish a preview | |
| 5 | Site has no `openspec/`, `engine/`, or `.cursor/` | |
| 6 | Pages source is GitHub Actions; Merge CI did not deploy the site | |
