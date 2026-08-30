# Default change path grafts document-driven QC onto OpenSpec

Blunder keeps **OpenSpec** as the change contract and the Completion gate as mechanical QC. Superpowers and gstack are tools on that path, not a second planning spine. The sequence is Grill → OpenSpec change → apply → automated QC → Human acceptance → Adversarial review (`/review`) → archive. Glossary: [CONTEXT.md — Agent environment](../../CONTEXT.md#agent-environment-repository). Map: [docs/agents/workflow.md](../agents/workflow.md).

## Considered Options

- **Replace OpenSpec + Superpowers + gstack with four pillars as the spine** — rejected. OpenSpec already is the contract; the Completion gate already is Test-run evidence. Replacing them would throw away Agent environment work.
- **gstack `/office-hours` (or Superpowers brainstorming) as the wish step** — rejected. Grill is the wish. Planning-review commands are not Default change path phases.
- **A second PRD, or WHEN/THEN specs as the human sign-off** — rejected. The human confirms 3–7 User stories in `proposal.md`.
- **Check chat / session logs into the repo; index them in `AGENTS.md`** — rejected. OpenSpec change folders are Working memory; `openspec/changes/archive/` is the index; `AGENTS.md` stays a map. Existing `docs/exec-plans/` and `docs/superpowers/plans/` stay historical — do not add files.
- **Companion docs repo, OINK/Hugo, or `llms.txt`** — rejected as this decision. Doc deliverable is this repo’s Markdown plus GitHub Pages over that same source. Wiring the generator is a later Default change path, not Agent-doc maintenance.
- **Hooks that prove Grill or Human acceptance** — rejected. Change-path stop lives in skills and the map: the agent asks instead of inventing completion.
