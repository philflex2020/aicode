### Git-based code review flow (practical, scalable setup)

Here’s a clean, low-friction workflow you can adopt across repos. It works with GitHub, GitLab, or Bitbucket, and supports gated CI, reviewers, and quality checks.

#### 1) Branching model
- Main branches:
  - `main`: always releasable
  - `develop` (optional): integration branch for active work
- Feature branches:
  - `feature/<short-scope>` for new features
  - `fix/<ticket-or-bug-id>` for bug fixes
  - `chore/<infra-or-maintenance>`
  - `hotfix/<prod-urgent>` for emergency patches
- Naming: keep concise and consistent. Example: `feature/ws-mtls-handshake-logging`.

#### 2) Commit conventions
- Use Conventional Commits for automation and clarity:
  - `feat(ws): add mTLS client cert validation log`
  - `fix(cuda): avoid deprecated sm_72 arch flags`
  - `perf(llama): stream KV cache to NVLink`
  - `chore(ci): add clang-tidy and pre-commit`
  - `docs(rag): add architecture diagram`
- Include an issue ID when applicable: `feat(ws): … (#123)`

#### 3) PR/MR creation checklist
- Template your PRs with a `.github/PULL_REQUEST_TEMPLATE.md` (or GitLab equivalent):
  - Summary, Context/linked issue, Changes, Test plan, Risks, Rollback, Screenshots/logs
- Require:
  - Passing CI (build + test + lint + security scans)
  - Minimum 1–2 code owners or domain reviewers
  - No direct pushes to `main`

Example PR template:
```markdown
### Summary
Brief description.

### Context
- Issue: #123
- Related: #122

### Changes
- ...

### Test Plan
- Commands/runbooks
- Screens/logs

### Risks and Rollback
- Risk areas:
- Rollback plan:

### Notes for Reviewers
- Areas to focus:
```

#### 4) Pre-commit and local quality gates
- Pre-commit hooks: `.pre-commit-config.yaml`
  - Formatters: `black`, `isort`, `prettier`
  - Linters: `ruff`/`flake8`, `eslint`, `clang-format`, `clang-tidy`
  - Secrets: `detect-secrets`, `trufflehog`
  - Large files/LFS enforcement
- Example quick install:
  ```
  pip install pre-commit
  pre-commit install
  pre-commit run -a
  ```

#### 5) CI gates (fast feedback)
- Stages:
  - Lint + type check (fast)
  - Build (CPU + CUDA variants if applicable)
  - Unit tests (sharded if large)
  - Integration tests (selective by label/path)
  - Security: `bandit` (Python), `semgrep`, `npm audit`, `cargo audit`
  - License scan: `pip-licenses` or `license-checker`
- PR-only jobs: run reduced, fast suite; nightly runs full matrix (Jetson, CUDA arches, etc.)
- Prevent merge unless green.

Example GitHub Actions snippet:
```yaml
name: pr
on:
  pull_request:
    branches: [ main, develop ]
jobs:
  lint-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: '3.11' }
      - run: pip install -r requirements-dev.txt
      - run: pre-commit run -a
      - run: pytest -q --maxfail=1
```

#### 6) Review norms (keep reviews productive)
- Scope: Aim for PRs < 400 lines (excluding generated/lock files). Split large PRs.
- Reviewer focus:
  - Correctness, security, performance critical paths
  - APIs/interfaces and tests
  - Readability, docs, and failure modes
- Author responsibilities:
  - Self-review checklist before assigning reviewers
  - Provide benchmarks when touching perf-sensitive pieces (e.g., CUDA kernels)
  - Add/adjust tests with code changes

Reviewer checklist (lightweight):
- Does code do what PR says?
- Adequate tests? Edge cases covered?
- Interfaces stable or flagged as breaking?
- Logging/metrics adequate for debugging?
- Security: inputs validated, secrets handled, mTLS/certs validated?

#### 7) Code owners and auto-assign
- Add `CODEOWNERS` to auto-assign reviewers by path:
```
# WebSocket server
server/ws/** @net-team @alice

# CUDA kernels
cuda/** @ml-systems @bob

# Docs
docs/** @tech-writers
```

#### 8) Labeling and automation
- Labels for triage: `area:ws`, `area:cuda`, `type:perf`, `type:bug`, `risk:high`
- Auto-label by path using actions or probot settings
- Use “draft” PRs for WIP to enable CI without signaling review readiness

#### 9) Policy for merges
- Merge methods:
  - Squash merge for features (clean history)
  - Rebase + merge for long-lived branches
  - Disable merge commits on `main` if you prefer linear history
- Require:
  - All checks passing
  - Required reviewers approved
  - “No unresolved comments” enforced by policy or social norm

#### 10) Post-merge
- Auto-tag builds based on commit messages or PR labels
- Auto-release notes generation using Conventional Commits and `git-chglog` or `release-please`
- Deploy to staging on merge to `main`; production on tag with `v*`

#### 11) Optional advanced pieces
- Monorepo? Use path-based CI, Bazel/CMake build graphs, and partial test runs.
- Protected files: require senior approval for `infra/`, `security/`, `schemas/` paths.
- Signed commits and branch protections:
  - Require signed commits, disallow force pushes on protected branches
- Dependency PR bots: Renovate/Dependabot tuned with schedules and grouping.

#### 12) Example repository scaffolding
```
.github/
  CODEOWNERS
  PULL_REQUEST_TEMPLATE.md
  workflows/
    pr.yml
    release.yml
.pre-commit-config.yaml
docs/CONTRIBUTING.md
```

If you want, I can:
- Generate starter files (CODEOWNERS, PR template, pre-commit config, Actions YAML).
- Tailor CI to Jetson AGX Orin builds (CUDA arch matrix, uWebSockets build, llama.cpp targets).
- Add reviewers/labels aligned to your current component map (WebSocket, CUDA, RAG).
