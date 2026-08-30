## 1. Site source

- [x] 1.1 Add `docs/index.md` short map (site-root-shaped links; copied to `/` at build).
- [x] 1.2 Add `.github/pages/_config.yml` (primer, `baseurl: /Blunder-Engine`, relative-links, optional-front-matter).

## 2. Deploy

- [x] 2.1 Add `.github/workflows/docs-pages.yml`: assemble repo-shaped tree, Jekyll build, deploy Pages. Trigger `main` path filters + `workflow_dispatch`. Do not touch Merge CI.
- [x] 2.2 Write [ADR 0051](../../docs/adr/0051-docs-github-pages.md). Point CONTEXT **Docs site**, `docs/agents/workflow.md`, and `docs/MAINTENANCE.md` at the live URL and the workflow.

## 3. Validation

- [x] 3.1 Run `openspec validate docs-github-pages --strict`.
- [ ] 3.2 Human acceptance: after merge + Pages source = GitHub Actions, walk `manual-checklist.md` on the live URL. (Not Agent QC.)
