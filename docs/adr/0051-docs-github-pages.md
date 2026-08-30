# Docs site is GitHub Pages with Jekyll over a same-shape tree

The Docs site is `https://bearthreestones.github.io/Blunder-Engine/`. GitHub Actions assembles README, AGENTS, CONTEXT, CONTENT_LAYOUT, and first-party `docs/` into a repo-shaped tree, builds it with GitHub’s Jekyll (`jekyll-theme-primer`), and deploys from `main` (path-filtered) plus `workflow_dispatch`. Home is `docs/index.md` copied to `/`. OpenSpec, engine, and `.cursor/` are not published. Merge CI does not deploy Pages. Rejected: MkDocs Material, OINK/Hugo, flattening `docs/` to the site root, PR preview sites, and a local assemble/Jekyll script.

## Considered Options

- **MkDocs Material** — rejected; better nav/search, but a new framework and it fights repo-relative links such as `../../AGENTS.md`.
- **OINK / Hugo / `llms.txt` / a second docs repo** — rejected in [ADR 0050](0050-default-change-path.md).
- **Classic Pages from the `/docs` folder only** — rejected; root maps would be missing and agent-guide links to `AGENTS.md` would break.
- **PR preview site** — rejected; extra environment for v1.
- **Local assemble or Jekyll script** — rejected; Agent QC is the Pages workflow succeeding on `main`.
