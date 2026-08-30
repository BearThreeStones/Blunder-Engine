## Why

Repo Markdown is the Doc deliverable, but it is unpublished. ADR 0050 already requires a same-repo GitHub Pages Docs site; this change wires that site so humans and agents share one rendered map.

## User stories

1. I open `https://bearthreestones.github.io/Blunder-Engine/` and see a short map home page, not the README CMake steps.
2. From that map I can open `AGENTS.md`, `CONTEXT.md`, `CONTENT_LAYOUT.md`, golden-principles, overview, workflow, and design-docs, and each page renders.
3. From a `docs/agents/` page I follow a repo-relative link back to `AGENTS.md` and it still works on the site.
4. After a docs change lands on `main`, the site updates. Opening a pull request does not publish a preview site.
5. The site does not include `openspec/`, `engine/`, or `.cursor/`.
6. The GitHub repo Pages source is GitHub Actions (one-time repo setting). Merge CI does not build or deploy the docs site.

## What Changes

- Add a GitHub Actions workflow that assembles a **repo-shaped** publish tree and deploys GitHub Pages with official Jekyll (`jekyll-theme-primer`).
- Publish README, AGENTS, CONTEXT, CONTENT_LAYOUT, and first-party `docs/`. Copy `docs/index.md` to the site root as the home map.
- Deploy only on `main` when those paths change, plus `workflow_dispatch`. No PR preview. No Merge CI coupling.
- Record generator and publish-set decisions in ADR 0051. Point MAINTENANCE / workflow docs at the live site URL.
- **Out of scope:** OINK, Hugo, MkDocs Material, `llms.txt`, a second docs repo, local Jekyll/assemble scripts, custom domain.

## Capabilities

### New Capabilities

- `docs-site`: GitHub Pages publishes this repo's Markdown from a same-shape tree so repo-relative links work.

### Modified Capabilities

- (none)

## Impact

- New: `.github/workflows/docs-pages.yml`, `.github/pages/_config.yml`, `docs/index.md`, `docs/adr/0051-docs-github-pages.md`
- Update: `docs/MAINTENANCE.md`, `docs/agents/workflow.md`, `CONTEXT.md` (ADR link), maybe `AGENTS.md` one row
- No engine/C++/Slint. No Merge CI job changes.
- Human acceptance is walking the live URL after the first `main` deploy. Agent QC is the Pages workflow succeeding on `main`.
