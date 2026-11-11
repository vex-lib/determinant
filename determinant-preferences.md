# Determinant — R3 Terminal Toolkit Preferences

## Constitution Link

gist.md (`../../metadata/gists/preferences/gist.md`) is the compressed law list; all universal laws bind this repository. Read one on demand: `./tools/prefs law "<title>"`. Universal laws are inherited, never restated.

Tier: core (R3 Terminal Toolkit). Owns: terminal-cell rendering, compositing, widgets, ANSI/VT generation, synchronized surface flushes, self-rendering widgets. Borrows: Vexspoke CPU, Relational Engine storage.

## Exclusive Laws

| Law Title | Scope | Enforcement |
| **Terminal Ownership Boundary Law** | R3 terminal rendering | Mandatory for `determinant` |
| **Resident Terminal and Swappable Module Law** | Terminal lifetime and reload | Mandatory for `determinant` |
| **Bare Output Separation Law** | R2 print vs terminal output | Mandatory for `determinant` |
| **Uniform Terminal Composition Law** | Canonical application frame | Mandatory for `determinant` |

### Terminal Ownership Boundary Law

* Determinant is the R3 toolkit for reusable terminal-cell rendering, widgets, compositing, command parsing and caller-fed input.
* Borrows Vexspoke CPU and Relational Engine storage contracts; never R1/R4/R5/R6 implementation headers.
* Owns the uniform presentation pipeline: cell geometry, region layouts, ANSI/VT escape generation, synchronized surface flushes, self-rendering widgets.
* Owns no competing OS process, storage or provider implementation.
* Coordinates are cells, not graphical pixels; app-specific conversation, consent, tools, channels and policies remain consumer-owned.

### Resident Terminal and Swappable Module Law

* Terminal ownership and admitted state outlive reloadable view/command code.
* Before swap, the host pauses admission, excludes active callers/callbacks and validates the complete candidate ABI/state; then publishes or rejects atomically.
* Failure preserves old callable code/state; unresolved rollback blocks resumption; retirement leaves no callback into old code.
* R3 supplies module-neutral contracts; R1 owns trusted loading and code residency.
* Input is caller-fed, with observable capacity refusal/truncation, never blocking stdin reads.
* Cold growth is allowed, never during admitted hot rendering.

### Bare Output Separation Law

* Vexspoke retains minimal bare one-line printing for CLI diagnostics; R2 never calls Determinant.
* Terminal applications route output through terminal presentation, never bare stdout over a managed screen.
* Diagnostic stderr is a separate cold channel, not widget rendering.

### Uniform Terminal Composition Law

* Determinant provides the canonical application frame (`TerminalFrame`), terminal compositor (`TerminalCompositor`), and host execution harness (`TerminalApp`).
* Consumers compose lower-tier contracts without re-implementing TTY raw mode, signal traps, device escape flushes, or shadow painters beside R3.
* Terminal applications wire reusable views and run loops through Determinant contracts, never through ad-hoc host rendering scripts or unshared terminal drivers.

## Repo-Local Extensions

* None — no managed exceptions to universal laws.

## Readiness Cross-Reference

* Feature readiness: determinant readiness gist.
* Open blockers: ecosystem blockers gist.
* Proof lawbook: `tests/test-preferences.md` — `./tools/prefs test "<title>"`.
* Evidence ledger: `metadata/gists/test-checklist.md` — `python3 tools/test_checklist.py check`.
