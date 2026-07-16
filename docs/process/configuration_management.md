# Configuration Management Guide
# GitHub Issue → Branch → Pull Request → Rebase → Merge

> Project: **Development of a Real Automotive Diagnostic ECU on STM32 using CAN and UDS**

---

## 1. Purpose

This document defines the Configuration Management workflow for the student project.

The workflow helps the team:

- Control changes.
- Avoid direct commits to protected branches.
- Keep code review mandatory.
- Link requirements to issues, branches, PRs, tests and releases.
- Practice an engineering process similar to real embedded/automotive projects.

---

## 2. Configuration Items

The following items are configuration items and must be managed in Git:

| Configuration Item | Path |
|---|---|
| Source code | `Core/`, `Debug/`, `Driver/` |
| Testing | `testing/` |
| CAN database | `dbc/` |
| Requirements | `docs/requirements/` |
| Design documents | `docs/design/` |
| Test specifications | `docs/tests/` |
| Process documents | `docs/process/` |
| Final report | `docs/report/` |

---

## 3. Branch Strategy

### Protected Branches

```text
main
└── stable release branch
```

### main

The `main` branch contains only stable releases.

Rules:

- No direct commit.
- Merge only approved Pull Requests.
- Must pass system and acceptance tests before release.

### Working Branches

```text
feature/<ticket_name>
bugfix/<ticket_name>
<ticket_name>
```

Examples:

```text
feature/can-driver
feature/uds-0x22
feature/dtc-manager
bugfix/can-filter
pytest/system-test
```

---

## 4. End-to-End Workflow

```text
Requirement or Defect
      ↓
GitHub Issue
      ↓
Branch from main
      ↓
Coding and local test
      ↓
Commit
      ↓
Pull Request
      ↓
Review
      ↓
Rebase with main
      ↓
Merge into main
      ↓
Integration/System Test
      ↓
Final Report
      ↓
Release tag
```

---

## 5. GitHub Issue Rule

### CM-001 - No Issue, No Code

A student shall not start coding before a GitHub Issue exists.

Every Issue should include:

- Requirement ID or defect ID.
- Problem statement.
- Scope.
- Acceptance Criteria.
- Owner.
- Expected evidence.

### Example Issue

```markdown
# Implement UDS Service 0x22 ReadDataByIdentifier

## Requirement
SYS-003, SYS-004, SWR-UDS-001

## Scope
Implement ReadDataByIdentifier for:
- DID 0xF190 VIN
- DID 0xF187 Software Version
- DID 0x0101 Vehicle Speed

## Acceptance Criteria
- 22 F1 90 returns 62 F1 90 <VIN>
- 22 F1 87 returns 62 F1 87 <SW version>
- 22 FF FF returns 7F 22 31

## Evidence
- Unit test log
- Tester log
- PR review result
```

---

## 6. Branch Creation Rule

### CM-002 - Create Branch from main

Always branch from the latest `main`:

```bash
git checkout main
git pull origin main
git checkout -b feature/uds-0x22
```

### Branch Naming

| Branch Type | Pattern | Example |
|---|---|---|
| Feature | `feature/<short-name>` | `feature/uds-0x22` |
| Bugfix | `bugfix/<short-name>` | `bugfix/can-filter` |
| Docs | `docs/<short-name>` | `docs/can-matrix` |
| Test | `test/<short-name>` | `test/system-test` |
| Release | `release/<version>` | `release/v1.0.0` |

---

## 7. Commit Rule

### CM-003 - Use Clear Commit Message

Use a simple conventional commit style.

Examples:

```text
feat: add UDS service 0x22
fix: correct CAN filter configuration
docs: update CAN matrix
test: add ST-002 VIN read test
refactor: simplify DID lookup
```

Avoid:

```text
update
fix code
final version
abc
```

### Commit Size

Good commits are small and meaningful.

Good:

```text
feat: add DID table
feat: add VIN read handler
feat: add unknown DID NRC handling
```

Bad:

```text
feat: update everything
```

---

## 8. Pull Request Rule

### CM-004 - No PR, No Merge

Every change must go through Pull Request.

### PR Template

```markdown
## Linked Issue
Closes #<issue-id>

## Requirement
SYS-xxx / SWR-xxx

## Summary
Explain what was changed.

## Test Evidence
- Unit test:
- Integration test:
- System test:
- CAN trace / UART log:

## Risk
State potential impact.

## Checklist
- [ ] Build passed
- [ ] Tests passed
- [ ] Evidence attached
- [ ] Reviewer assigned
- [ ] Traceability updated
```

---

## 9. Review Rule

### CM-005 - At Least One Reviewer

A Pull Request must have at least one reviewer before merge.

Reviewer checks:

- Requirement coverage.
- Code readability.
- Module responsibility.
- Test evidence.
- No unrelated changes.
- No direct hardware-specific dependency in upper layers.

### Review Comment Examples

```text
Please move HAL CAN call out of uds.c into can_if or adapter layer.
```

```text
Please add negative response test for unknown DID.
```

---

## 10. Rebase Rule

### CM-006 - Rebase Before Merge

Before merging, update the feature branch with latest `main`.

```bash
git checkout feature/uds-0x22
git fetch origin
git rebase origin/main
```

If there is conflict:

```bash
git status
# fix conflicted files
git add .
git rebase --continue
```

After successful rebase:

```bash
git push --force-with-lease
```

Important:

- Use `--force-with-lease`, not plain `--force`.
- Do not rebase shared branches without informing teammates.

---

## 11. Merge Rule

### Merge to main

Only release-ready code is merged to `main`.

Conditions:

- Build passed.
- Integration test passed.
- System test passed.
- Acceptance test passed or limitations documented.
- Release note prepared.

---

## 12. Release Management

### Version Naming

```text
v0.1.0
v0.2.0
v0.3.0
v1.0.0
```

### Suggested Releases

| Version | Content |
|---|---|
| `v0.1.0` | CAN driver and EngineStatus |
| `v0.2.0` | UDS 0x22 ReadDataByIdentifier |
| `v0.3.0` | UDS MVP and DTC |
| `v1.0.0` | Final demo release |

### Release Note Template

```markdown
# Release v0.2.0

## Added
- UDS ReadDataByIdentifier 0x22
- VIN DID 0xF190
- SW Version DID 0xF187

## Fixed
- CAN filter issue

## Test Evidence
- ST-002 passed
- ST-003 passed

## Known Issues
- ISO-TP flow control simplified
```

---

## 13. Traceability Rule

Every change should be traceable:

```text
Requirement
  ↓
GitHub Issue
  ↓
Branch
  ↓
Pull Request
  ↓
Test Evidence
  ↓
Release Tag
```

Example:

```text
SYS-003 Read VIN
  ↓
Issue #23
  ↓
feature/uds-0x22
  ↓
PR #15
  ↓
ST-002 log
  ↓
v0.2.0
```

---

## 14. Definition of Done

An issue is done only when:

- Code is implemented.
- Code builds successfully.
- Test case is created.
- Test has passed.
- Evidence is attached.
- PR is reviewed.
- Branch is rebased.
- PR is merged to `main`.
- Traceability matrix is updated.

---

## 15. Golden Rules

1. No Issue → No Code.
2. No Branch → No Development.
3. No PR → No Merge.
4. No Review → No Approval.
5. No Test Evidence → No Done.
6. No Traceability → No Release.
7. No Direct Commit to main.
8. No Hardware Feature Merge Without Evidence.

---

## 16. Student Task Example

Feature: Implement EngineStatus cyclic CAN message.

```text
Requirement: SYS-001
Issue: #12 EngineStatus cyclic CAN
Branch: feature/engine-status
PR: #08
Tests: UT-CAN-001, IT-001, ST-001
Evidence: CAN trace showing ID 0x100 every 1000ms
Release: v0.1.0
```

This is the expected working style for the project.
