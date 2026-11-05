# Determinant — R3 Terminal Toolkit Preferences

## Constitution Link

The complete [constitution](https://gist.github.com/vex-graph/4132a6c45cb6d3797c3e8eff2e94035a)
governs this repository. The binding law list is `gist.md`
(`../../metadata/gists/preferences/gist.md`); `../../../metadata/gists/preferences/PREFERENCES.md` is its full
rationale and is never ingested whole. Read the law first, then this document, the proof
lawbook `../../../tests/test-preferences.md` (slice one law with
`./tools/prefs test "<title>"`), the evidence ledger
`../../../metadata/gists/test-checklist.md`, and the actual implementation.

## Law Index (Binding Matrix)

| Law Title | Scope | Enforcement |
| :--- | :--- | :--- |
| Terminal Ownership Boundary Law | R3 terminal rendering, compositing, widgets, parsing and input | Owner and dependency tests |
| Resident Terminal and Swappable Module Law | Terminal lifetime and reload | Real module swap, rejection preservation and teardown tests |
| Bare Output Separation Law | R2 print vs terminal application output | Quiet rendering and routed-output tests |
| Uniform Terminal Composition Law | Canonical application frame, compositor, and host lifecycle | Contract composition and no-shadow-owner tests |

### Terminal Ownership Boundary Law

Determinant is the R3 toolkit for reusable terminal-cell rendering, widgets,
compositing, command parsing and caller-fed input. It borrows Vexspoke CPU and Relational
Engine storage contracts, never R1/R4/R5/R6 implementation headers. Determinant owns
the uniform presentation pipeline: cell geometry, region layouts, ANSI/VT escape
generation, synchronized surface flushes, and self-rendering widgets. An
R1/standalone host supervises terminal acquisition, bounded event driving and
teardown through opaque handles/callbacks. Determinant owns no competing OS
process, storage or provider implementation. Coordinates are cells, not
graphical pixels; app-specific conversation, consent, tools, channels and
policies remain consumer-owned.

### Resident Terminal and Swappable Module Law

Terminal ownership and admitted state outlive reloadable view/command code.
Before swap, the host pauses admission, excludes active callers/callbacks and
validates the complete candidate ABI/state; it then publishes or rejects
atomically. Failure preserves old callable code/state; unresolved rollback blocks
resumption, and retirement leaves no callback into old code. R3 supplies
module-neutral contracts; R1 owns trusted loading and code residency. Toolkit
construction is not live Primeagen reload proof: migrate reusable terminal
pieces, retain app/session/consent state in the host, reproduce the
restart-required defect, then prove a visible view/command change in the same
production process with rollback.

Derived geometry, owned backing, initialized counts, feed positions and callback
generations change only through admission/feed/reserve/swap/reset/destruction;
safe queries exist, but no setters can forge capacity, ownership or residency.
This scoped managed accessor exception under the Conflict Triage Law and Single
Class Per File Law (Java Law) preserves lifetime truth. The host serializes
externally and retains callable code until users are excluded; atomics alone do
not make backing relocation or unloading safe. Cold growth is allowed, never
during admitted hot rendering. Input is caller-fed, with observable capacity
refusal/truncation, never blocking stdin reads.

### Bare Output Separation Law

Vexspoke retains minimal bare one-line printing for CLI diagnostics; R2 never
calls Determinant, which may consume R2 primitives. Terminal applications route
output through terminal presentation, never bare stdout over a managed screen.
Diagnostic stderr is a separate cold channel, not widget rendering. Hotcwap's
Console is the R1 lifecycle object, not the R2 printer or a Determinant widget.

### Uniform Terminal Composition Law

Determinant provides the canonical application frame (`TerminalFrame`), terminal
compositor (`TerminalCompositor`), and host execution harness (`TerminalApp`)
so consumers compose lower-tier contracts without re-implementing TTY raw mode,
signal traps, device escape flushes, or shadow painters beside R3 (the Vertical
Integration Law: Compose down, never rebuild). Terminal applications wire
reusable views and run loops through Determinant contracts, never through ad-hoc
host rendering scripts or unshared terminal drivers.

## Repo-Local Extensions (managed, per the Conflict Triage Law)

### Placement and Component Tree Extension

The terminal hosts **one content panel**, a tree of **two component kinds**:
panel (container) and text (leaf).

- `TerminalAnchor` is a pure placement function returning the region *and* its
  resulting cursor. Bands (`BAND_TOP/BOTTOM/LEFT/RIGHT`) reserve slices, shrinking
  the cursor for later children; `FILL` and nine pins reserve nothing, letting
  backgrounds/overlays cover without disturbing flow. Layout is one rule:
  reserve or not.
- `TerminalPanel` owns tree, z/flow order, not content. One flavour field (not a
  class per kind): `FLEX` advances past each reserving child; `LAYERED` does not,
  resolving every child against the whole area. `TerminalPanel_paint` is **total**:
  repaint the whole tree on every call, without dirty bits or skipping.
- `TerminalText` is an addressable, one-row borrowed string with its own pen,
  optional brackets and alignment. It is an object, not an anonymous draw, so
  hover, recolouring and hit-testing can name it. Unset colours inherit from its
  panel; styles combine.
- Either kind may contain either. Adding transfers ownership to the parent;
  `remove` frees, `detach` returns ownership for reparenting, and freeing a panel
  frees its subtree. Direct self-containment is forbidden; indirect cycles are
  refused at paint time.
- **Resize is free and is the reason the tree exists.** Recompute every rectangle
  from canvas size on each paint: no cached sizes, just paint at new dimensions.
- Presentation delta is **not** the panel's job. `TerminalSurface` diffs the
  fresh grid against the last paint, emitting changed same-row runs only. Thus
  total repaint costs the host nothing on an unchanged frame. A refused run leaves
  the record unchanged, so next present redelivers that delta.
- `TerminalCompositor` owns the output translation seam: it consumes `TerminalSurface`
  delta runs, formats optimized ANSI/VT escape sequences into a bounded frame buffer,
  and synchronizes atomic device flushes without consumer-side escape generation.
- Child identity is slot + generation; retired slots report stale IDs, never
  address recycled children. Derived geometry/growth changes only via
  add/remove/detach/anchor, never a lifetime-forging setter. Cold growth doubles;
  refusals are counted and reported once per epoch.

`TerminalFrame` is the canonical top-level responsive layout object. It divides
the root canvas into header, left rail, centre, right rail, and footer regions.
It provides the standard five-region workbench for interactables and superapps,
coordinating with panels and anchors rather than duplicating geometry.

`TerminalButton` is interactive text (hover, click, retained handler) and the
only widget retaining a handler; every other widget returns an event and keeps
nothing.

`TerminalTreeView` is a scrollable, collapsible tree of addressable rows. A
node's subtree is a contiguous pre-order range, so folding is a jump and unfolding
is a step; visible rows are cached and recomputed only on a structural change;
ids are stable and never recycled. In addition to headless node/row queries, it
ships a built-in canonical row painter with configurable indentation
(`TerminalTreeView_setIndent`, default 2 spaces), folder marker styling, and
icon provider integration (e.g. Nerd Font or plain mode). The caller configures
these uniform presentation defaults instead of duplicating file-tree rendering
loops in consumer entry points.

`TerminalScrollGlide` is the shared scroll motion for any viewport: per-axis
shown/target positions plus the easing between them, with a snap threshold. Input
moves the target; `step` returns the whole-cell position to paint. It owns no
cells and no clipping, so it is the motion half of a scroll panel, not the
`SCROLL` panel flavour. `step` is the frame path: one null guard, no logging and
no allocation.

`TerminalCodeView` is a borrowed one-line painter, not a lexer/language adapter.
Consumers supply ordered UTF-8 byte spans with terminal pens and optional inlay
labels anchored at source byte offsets. Determinant validates admission bounds
and paints clipped cells; virtual hints never change source bytes or span
coordinates. Consumers own parsing, semantic classification, theme policy and
backing lifetimes. This R3 toolkit borrows no other R3 language implementation.

`TerminalApp` is the standard host execution harness. It manages the terminal
lifecycle: raw mode acquisition/restoration, `SIGWINCH` resize handling, input
polling, event dispatch, and frame pump over the root frame. Consumers wire
objects and run `TerminalApp` without implementing ad-hoc terminal drivers.

### Source Taxonomy Extension

`src` is organised by the capability a unit owns, not by a flat widget prefix:

- `ui/surface/` — `Terminal`, `TerminalCanvas`, `TerminalProjection`, `TerminalSurface`: the cell surface and last-paint pipeline.
- `ui/compositor/` — `TerminalCompositor`: delta-to-ANSI translation and synchronized device flush buffer.
- `ui/app/` — `TerminalApp`: lifecycle harness, terminal raw mode, resize and event loop.
- `ui/panel/` — `TerminalPanel`, `TerminalAnchor`, `TerminalFrame`: container, placement, region layout.
- `ui/text/` — `TerminalText`, `TerminalTextView`, `TerminalStatusBar`.
- `ui/list/`, `ui/tree/` — `TerminalListView`, `TerminalTreeView`.
- `ui/field/` — `TerminalInputLine` (single-line field); a multiline textarea is not implemented.
- `ui/code/` — `TerminalCodeView`; a diff panel and diff-file model are not implemented.
- `ui/markdown/` — `TerminalMarkdownView` plus its cold owners: `TerminalMarkdownTheme` (roles, per-level pen ladder, glyphs), `TerminalMarkdownInline` (bold/italic/underline/code span parse), `TerminalMarkdownText` (document line model), `TerminalMarkdownTable` (alignment, escapes, column fit), `TerminalMarkdownCallout` (both spellings, severity and indent), `TerminalMarkdownCodeBlock` (fence pool and copy projection) and `TerminalMarkdownAction` (click value and icon geometry). The view owns layout and paint; the owners own parsing and records; the caller owns the clipboard and performs the action the view reports.
- `ui/button/` — `TerminalButton`.
- `ui/image/` — `TerminalImage`.
- `ui/scroll/` — `TerminalScrollGlide`.
- `ui/theme/` — `TerminalAppearance` presets.
- `events/` — caller-fed input decoding (currently mouse); a normalised key event is not implemented.
- `format/` — `TerminalStyle` flags and `TerminalColor` packing.
- `cli/` — command parsing and log projection, outside `ui/`.

Every include names the canonical path (`ui/panel/terminal_panel.h`,
`events/terminal_input.h`, `format/terminal_color.h`); `src/terminal` no longer
exists. A new reusable unit lands in the directory that owns its capability.
Class names keep the `Terminal` prefix — the directory is placement, not identity.
Mirrored owner tests live under `tests/determinant/<same relative directory>/`.

### Canonical Frame Integration and Progressing Flavours

`TerminalFrame` computes five canonical regions (header, left rail, centre,
right rail, footer), providing a standard top-level application layout that
coordinates with `TerminalPanel` and `TerminalAnchor`. Frame geometry is
permanent and integrated, not retired: superapps and interactables target
these five standard regions, while finer-grained layout within each region uses
anchors and panels.

`SCROLL` and `CODE` panel flavours are progressing: scrolling needs a real clip
and offset; code needs a gutter. `TerminalScrollGlide` provides the reusable
motion model, and `TerminalCodeView` provides line-level rendering. Standardized
gutter presentation, `cursor/` stepping, and caret movement will be unified
in Determinant so consumers like Semicolon can retire their duplicated editor
loops under the Vertical Integration Law (Compose down, never rebuild).

Not implemented or implied by an executed owner: panel-tree pointer/hover routing
(cold `TerminalPanel_childAt` exists, unused); bracketed chip/segment bar; tab
strip; and child-array allocation-failure injection. `TerminalTreeView` proves
folding, selection, reveal and viewport clamping, and its built-in row painter
renders canonical indented rows with folder and icon styles. The format plane is
16-bit and carries italic as `TERMINAL_STYLE_ITALIC`, so a span can be bold, italic
and underlined without a class of its own.

Markdown presentation is still a bounded subset, not a browser: headings ladders,
both callout spellings, aligned tables, fenced code with a copy affordance and
labelled links are owned; emphasis nesting, HTML, reference links, images and a
full CommonMark parser are not. A link press reports its target; it never hands
an untrusted URI to a desktop opener.

;;INTENTION("R3 terminal toolkit: uniform terminal presentation including canonical TerminalFrame layout, self-painting TerminalTreeView with configurable indents and icons, anchor/panel component tree, and TerminalCompositor/TerminalApp lifecycle, ensuring consumers compose R3 directly rather than re-implementing terminal drivers.")

## Current Proof Boundary

This lawbook states the accepted contract, not readiness. Owner tests belong in
`../../tests/determinant`. Primeagen integration, live reload, real terminal/
backend behavior and other platforms need their own executed evidence. No push,
interactive launch or live provider call is implied.
