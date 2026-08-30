## Purpose

Publishes this repository's versioned Markdown as a GitHub Pages site that keeps repo-relative links working, so humans read the same source agents already follow.

## ADDED Requirements

### Requirement: Same-shape published tree
The Docs site SHALL publish the repository maps README, AGENTS, CONTEXT, and CONTENT_LAYOUT at the site root, and SHALL publish first-party `docs/` under the same `docs/` prefix. The site SHALL NOT publish `openspec/`, `engine/`, or `.cursor/`.

#### Scenario: Home map and agent guide share one tree
- **WHEN** a reader opens the Docs site home page and follows a link to AGENTS.md, then from an agent guide follows a repo-relative link back to AGENTS.md
- **THEN** both AGENTS.md destinations resolve on the site
- **AND** the home page is a short documentation map, not the README build-prerequisite steps

#### Scenario: Working memory and engine stay off the site
- **WHEN** a reader browses the Docs site
- **THEN** no page is served from `openspec/`, `engine/`, or `.cursor/`

### Requirement: Deploy from the default branch only
The Docs site SHALL deploy when the default branch (`main`) receives a change under the published Markdown paths, or when a maintainer runs a manual workflow dispatch. Pull requests SHALL NOT publish a preview site. Merge CI SHALL NOT build or deploy the Docs site.

#### Scenario: Main docs change updates the site
- **WHEN** a change to `docs/` or a published root map is merged to `main`
- **THEN** the Docs site is rebuilt and deployed

#### Scenario: Pull request does not preview
- **WHEN** a pull request changes published Markdown
- **THEN** the Docs site URL is not replaced by a pull-request preview deployment

#### Scenario: Merge CI stays a build gate
- **WHEN** Merge CI runs
- **THEN** that job does not assemble or deploy the Docs site
