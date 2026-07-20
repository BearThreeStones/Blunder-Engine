# Design docs

> Why and boundaries — not step-by-step how-to. For procedures, see [docs/agents/](../agents/).

| Document | Purpose |
|----------|---------|
| [architecture.md](architecture.md) | README five-layer model mapped to repo layout |
| [../golden-principles.md](../golden-principles.md) | Agent must-follow rules |
| [../../CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md) | Assets / Resources virtual paths and cook pipeline |
| [../adr/0012-pull-asset-pipeline.md](../adr/0012-pull-asset-pipeline.md) | Pull Asset Pipeline decision (three-tier + GUID refs) |
| [../../CONTEXT.md](../../CONTEXT.md#asset-pipeline) | Asset pipeline domain vocabulary |

When changing boundaries (new top-level module, moving editor code, new content roots), update `architecture.md` and any affected agent guides in the same change.
