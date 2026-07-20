# Task 5.4 — Reject .blend automatic export

Reject or clearly non-support `.blend` automatic export in v1 (copy-to-Source-only or error).

## Scope
Audit: 5.2 already rejects .blend Source Export. Confirm product behavior is clear error (not silent success). If only error path exists, document and mark 5.4. Optionally allow copy-to-Source-only with explicit non-Asset message — prefer keep current reject unless brief demands copy.

TDD: ensure blend import fails with success=false (may already exist).

Mark 5.4. Report: .superpowers/sdd/task-20-report.md
Do not push.
