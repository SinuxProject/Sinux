# Sinux Governance

How the Sinux project is led, how decisions are made, and how releases ship.
Effective as of v0.6.0 (2026-09-09); this document evolves with the project.

---

## 1. Roles

### Lead Maintainer — `hesam-oxe`

- Final responsibility for technical direction, roadmap priorities, and
  feature selection.
- Final reviewer for merges to `main`; resolves technical disagreements
  with recorded rationale.
- Oversees the RFC process (§3) and appoints additional maintainers and
  reviewers as the project grows.

### Release Manager — `hesam-oxe`

- Owns the release policy (§4): versioning, milestones, tagging, and
  release cuts.
- Only the Release Manager (or an explicit delegate) tags releases and
  publishes release notes.
- Guards the green-`main` rule: no release is ever cut from a red tree.

> One person currently holds both roles; the responsibilities are defined
> separately so leadership can scale to a team without restructuring.

### Maintainers

Experienced contributors with merge rights, appointed by the Lead
Maintainer. Maintainers review PRs, shepherd their subsystems, and uphold
the Definition of Done (`CONTRIBUTING.md`).

### Contributors

Everyone who improves Sinux — code, reviews, docs, testing, bug reports.
Listed in `CONTRIBUTORS.md`.

---

## 2. Meritocracy

Roles are earned through sustained, high-quality contribution. The usual
path is Contributor → Reviewer → Maintainer, judged on the work, not the
title. Leadership serves the project: direction is set in the open, on
the roadmap, RFCs, and PRs.

---

## 3. Decision-making

- **Small changes** (fixes, minor features, docs): pull request + at least
  one maintainer review + green CI (`make iso`, `make test`).
- **Large changes** (new subsystems, ABI changes, roadmap shifts): require
  an **RFC** — a short design proposal (problem, design, alternatives,
  test plan) posted for discussion and **approved by the Lead Maintainer
  before implementation starts**.
- **Deadlocks**: the Lead Maintainer decides, with the rationale recorded
  on the RFC or PR.

---

## 4. Release policy

- **Versioning**: SemVer (`MAJOR.MINOR.PATCH`) with `alpha`/`beta`/`rc`
  pre-releases for milestones.
- **`main` is always releasable**: CI-gated, self-tested, documented.
- **Every release ships**: an annotated tag, a `CHANGELOG.md` entry, and
  release notes.
- **Atomic, conventional commits** (`feat:`, `fix:`, `docs:`, `test:`,
  `ci:`, …), curated per release by the Release Manager.

---

## 5. Licensing of contributions

- **Inbound = outbound**: all contributions are accepted under the
  project's license, **Apache-2.0** (see `LICENSE`).
- **Contributors retain copyright in their own work**; no copyright
  assignment is required or requested.
- By submitting a pull request you confirm that you have the right to
  license your contribution under these terms.

---

## 6. Conduct

Be professional and constructive: critique ideas, not people; assume good
faith; keep discussions in English on project channels. Maintainers may
moderate disruptive behavior. A full code of conduct may be adopted as the
community grows.
