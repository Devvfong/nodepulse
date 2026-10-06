# NodePulse — Contributor Guidelines

Thank you for contributing to NodePulse. Whether you are a human software engineer or an autonomous AI coding agent, this document outlines the required contribution standards and workflow.

---

## 1. Golden Rules of Contribution

1. **Strict Phase Adherence**:
   Implementation work MUST follow the active approved phase in [CURRENT_STATE.md](file:///home/devqii/workspace/nodepulse/CURRENT_STATE.md) and [IMPLEMENTATION_PLAN.md](file:///home/devqii/workspace/nodepulse/IMPLEMENTATION_PLAN.md). Contributions introducing code for future unapproved phases will be rejected.
2. **Review Execution Directives**:
   All autonomous agents and automated tooling must strictly obey [AGENTS.md](file:///home/devqii/workspace/nodepulse/AGENTS.md).
3. **No Unjustified Dependencies**:
   Do not introduce third-party libraries or submodules without prior ADR approval.
4. **Zero Compiler Warnings**:
   All code must compile cleanly under `-Wall -Wextra -Wpedantic -Werror`.
5. **Zero Secret Leakage**:
   Never commit plain API keys, test passwords, or credentials.

---

## 2. Step-by-Step Development Workflow

### Step 1: Verify Active Phase
Before writing code, check `CURRENT_STATE.md` to identify the current phase and deliverables:
```bash
grep -E "Current implementation phase|Next approved phase" CURRENT_STATE.md
```

### Step 2: Create a Feature Branch
Create a descriptive branch following the project conventions:
```bash
git checkout -b phase/1-repository-foundation
```

### Step 3: Test-Driven Development (TDD)
- Author unit tests and `/proc` parser fixtures in `tests/` before or alongside implementations.
- Ensure edge cases (empty files, truncated lines, permission errors) are covered.

### Step 4: Implement Code
- Author headers in `include/nodepulse/` and implementations in `src/`.
- Adhere strictly to [CODING_STANDARDS.md](file:///home/devqii/workspace/nodepulse/docs/engineering/CODING_STANDARDS.md).

### Step 5: Local Verification
Run the verification suite locally:
```bash
# 1. Compile with all warnings enabled
cmake --build build -- -j$(nproc)

# 2. Run all tests
ctest --test-dir build --output-on-failure

# 3. Format check
clang-format --dry-run --Werror $(find src include tests -name '*.cpp' -o -name '*.hpp')

# 4. Static analysis
clang-tidy -p build src/**/*.cpp
```

### Step 6: Update Documentation & State
- When phase criteria are met, update [CURRENT_STATE.md](file:///home/devqii/workspace/nodepulse/CURRENT_STATE.md) to record deliverables and unlock the next phase.

---

## 3. Pull Request Submission Checklist

Before submitting a Pull Request, verify that all items are checked:

- [ ] Code is strictly confined to the currently approved phase.
- [ ] No code or architectural changes for unapproved future phases.
- [ ] 100% of unit and integration tests pass via `ctest`.
- [ ] Project compiles with zero compiler warnings under `-Werror`.
- [ ] Code passes `clang-format` and `clang-tidy` without suppressions.
- [ ] No secrets, tokens, or credentials committed.
- [ ] No privileged Docker containers or insecure socket descriptions.
- [ ] Relevant documentation and `CURRENT_STATE.md` updated.

