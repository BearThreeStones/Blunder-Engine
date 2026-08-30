## Context

See `proposal.md` for why. Grill + ADR 0050 already require a same-repo Pages site and forbid OINK/Hugo/`llms.txt`. First-party CI is only Merge CI. Agent guides use repo-relative links such as `../../AGENTS.md`, so the publish tree must match the repo layout rather than flattening `docs/` to `/`.

## Goals / Non-Goals

**Goals:**

- Assemble a repo-shaped tree in GitHub Actions and deploy it with GitHub's Jekyll Pages path.
- Land a short map at `/` from `docs/index.md`.
- Record generator and publish-set in ADR 0051.

**Non-Goals:**

- MkDocs, Hugo, OINK, custom CSS, custom domain, `llms.txt`.
- Local assemble or Jekyll scripts.
- PR preview environments.
- Changing Merge CI.

## Decisions

### D1 — Official Jekyll, primer, same-shape tree
**Choice:** `actions/jekyll-build-pages` + `actions/deploy-pages`. Theme `jekyll-theme-primer`. `baseurl: /Blunder-Engine`. Plugins: `jekyll-relative-links`, `jekyll-optional-front-matter`. Config lives in `.github/pages/_config.yml` and is copied onto the assembled root (not stored under `docs/` as a document).
**Why:** GitHub-native; preserves `.md` links; no new doc framework.
**Rejected:** MkDocs Material (nav/search vs new framework and broken repo-relative paths); flattening `docs/` to site root; classic “deploy from /docs folder” without copying root maps.

### D2 — Assemble in the workflow only
**Choice:** Checkout, make a publish directory, copy README / AGENTS / CONTEXT / CONTENT_LAYOUT, copy `docs/` recursively, copy `_config.yml`, copy `docs/index.md` to publish-root `index.md`, then Jekyll build + deploy.
**Why:** No extra script to maintain; Agent QC is the workflow succeeding on `main`.
**Rejected:** `scripts/assemble-docs-pages.*`; running Jekyll in Merge CI.

### D3 — Triggers
**Choice:** `push` to `main` with path filters on `docs/**`, the four root maps, `.github/workflows/docs-pages.yml`, and `.github/pages/**`, plus `workflow_dispatch`. Permissions: `contents: read`, `pages: write`, `id-token: write`. Concurrency group cancels in-progress deploys.
**Why:** Matches grilled deploy rule; avoids rebuilding Pages on every engine commit.
**Rejected:** PR preview Pages; deploying on every `main` push.

### D4 — Human acceptance is the live URL
**Choice:** After the first successful deploy, walk `https://bearthreestones.github.io/Blunder-Engine/`. Repo Settings → Pages → Source = GitHub Actions is a one-time human step listed on the checklist.
**Why:** No PR preview by design.
**Rejected:** Local Jekyll as a substitute for walking the site.

## Risks / Trade-offs

- [Jekyll skips or mis-renders some Markdown without front matter] → `jekyll-optional-front-matter`; do not require front matter on existing docs.
- [Relative `.md` links 404 after HTML conversion] → `jekyll-relative-links`.
- [HTML mockups under `docs/mockups` / `docs/previews`] → Copy as static files with the rest of `docs/`; if Jekyll mangles one, treat as a follow-up, not a blocker for the map.
- [Pages source not set to GitHub Actions] → Checklist row; workflow cannot flip the setting.
- [CONTEXT.md is large] → Accept; it is a published map, not a reason to split the glossary.

## Migration Plan

1. Land workflow, config, `docs/index.md`, ADR 0051, map links.
2. Merge to `main`; set Pages source to GitHub Actions if unset.
3. Walk the live URL (Human acceptance).

Rollback: delete the workflow (site stops updating). Markdown in git is unchanged.

## Open Questions

None.
