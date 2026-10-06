# NodePulse — Git Workflow & Commit Guidelines

This document specifies the branching model, commit message formatting, and pull request policies for the NodePulse repository.

---

## 1. Branching Strategy

```mermaid
gitGraph
    commit id: "Phase 0 Docs"
    branch phase/1-foundation
    checkout phase/1-foundation
    commit id: "feat(cmake): setup"
    commit id: "test(smoke): gtest"
    checkout master
    merge phase/1-foundation tag: "v0.1.0"
    branch phase/2-http
    checkout phase/2-http
    commit id: "feat(http): drogon"
    checkout master
    merge phase/2-http tag: "v0.2.0"
```

### 1.1 Branch Taxonomy
- **`master`**: The primary authoritative branch. Always builds cleanly and passes CI. Direct pushes are disabled; changes merge via verified pull requests.
- **`phase/<N>-<topic>`**: Dedicated branches for executing a specific phase from [IMPLEMENTATION_PLAN.md](file:///home/devqii/workspace/nodepulse/IMPLEMENTATION_PLAN.md) (e.g. `phase/1-repository-foundation`, `phase/2-http-foundation`).
- **`fix/<issue-name>`**: Targeted bugfix branches addressing a specific defect.
- **`docs/<topic>`**: Documentation updates.

---

## 2. Conventional Commit Standards

All commit messages must adhere to the **Conventional Commits v1.0.0** specification:

```
<type>(<scope>): <short imperative summary>

[optional body explaining context and rationale]

[optional footer, e.g. Closes #123]
```

### 2.1 Commit Types
- **`feat`**: A new user-facing or architectural capability.
- **`fix`**: A bug fix.
- **`docs`**: Documentation changes only.
- **`test`**: Adding missing tests or correcting existing tests.
- **`refactor`**: Code change that neither fixes a bug nor adds a feature.
- **`perf`**: A code change that improves performance.
- **`build`**: Changes that affect the build system or external dependencies (CMake).
- **`ci`**: Changes to CI configuration files and scripts.
- **`chore`**: Maintenance tasks not modifying source or test files.

### 2.2 Scopes
- `system`, `cpu`, `memory`, `disk`, `network`, `process`, `docker`, `auth`, `rate-limit`, `sse`, `prometheus`, `postgres`, `config`, `systemd`.

### 2.3 Examples
- `feat(cpu): parse /proc/stat and compute delta utilization`
- `fix(auth): enforce constant-time string comparison for X-API-Key`
- `docs(api): document HTTP 429 rate limit response envelope`
- `test(mem): add mock /proc/meminfo fixture for zero-swap systems`

---

## 3. Merge Policy

- **Linear History**: PRs are merged via **Squash and Merge** or **Rebase and Merge** to maintain a clean linear commit graph on `master`.
- **Pre-Merge Validation**: Every PR must pass all automated CI checks:
  1. Build succeeds on GCC and Clang with `-Werror`.
  2. 100% CTest pass rate.
  3. `clang-format` and `clang-tidy` clean.
- **Tagging Releases**: Releases are tagged using Semantic Versioning (`v1.0.0`, `v1.1.0`).

