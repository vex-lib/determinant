# determinant

R3 native terminal UI and CLI toolkit for Vexgraph: cell-grid rendering, panel layout, diff-based frame presentation, mouse decoding, and reusable widgets.

```bash
./tools/b build determinant
python3 tests/determinant/run.py
```

## About

`determinant` provides bare terminal geometry, layout, and widgets with no application content, no OS terminal I/O, and no direct stdout writes. The host supplies the backing cell grid and an injected `DeterminantOutputFn` sink; `determinant` computes layout, paints the panel tree, and emits only changed same-row cell runs inside an atomic frame bracket.

Public headers live under `cli/*.h` and `terminal/*.h`. Runtime builds link against `vexspoke` and `re` directly without copying allocators.

## Features

- **Canvas and color (`terminal_canvas`, `terminal_style`, `terminal_color`, `terminal_image`)**: Clipped 4-plane cell grid (glyph, style, fg, bg) with packed `0xRRGGBBAA` colors, host-neutral style flags, and RGBA8-to-cell rasterization.
- **Layout and tree (`terminal_anchor`, `terminal_panel`, `terminal_text`, `terminal_frame`, `terminal_appearance`)**: `FILL`, four reserving `BAND`s, and nine `PIN`s; `FLEX` and `LAYERED` panel containers; addressable `TerminalText` leaves with inherited colors; named box-drawing presets (`ascii-solid`, `ascii-striped`, `ascii-dashed`, `unicode-light`, `unicode-emdash`, `unicode-d64`). Resize is free because rectangles recompute from canvas dimensions on every paint.
- **Diff presentation (`terminal_surface`)**: Compares the newly painted grid against the last-presented frame and emits only changed contiguous same-row runs inside a lazy open/close frame transaction.
- **Widgets and motion (`terminal_tree_view`, `terminal_scroll_glide`, `terminal_markdown_view`, `terminal_code_view`, `terminal_list_view`, `terminal_text_view`, `terminal_input_line`, `terminal_status_bar`)**: Collapsible pre-order tree view with stable IDs, eased per-axis scroll motion, read-only GFM-subset Markdown preview, and a caller-fed UTF-8 code line painter with virtual inlay labels.
- **Input and host state (`terminal_input`, `terminal`, `terminal_button`, `cli/*`)**: Pure SGR/X10 mouse byte decoder producing normalized cell `TerminalPointerEvent`s, resident terminal host with `Terminal_swap`, whitespace command parser, caller-backed command registry, and caller-fed scanner.

## Build and test

From the `worktree/` root:

```bash
./tools/b build determinant
python3 tests/determinant/run.py
```

## Future

- Connect `TerminalPanel_childAt` and `terminal_input` into built-in pointer routing and hover/press/grab state.
- Add a clipping `SCROLL` panel flavor, bracketed chip/segment bars, tab strips, and an editor view with a line-number gutter.
- Migrate `TerminalRule` and appearance presets off `terminal_frame` onto panels and anchors to remove duplicate band math.
- Complete the Primeagen terminal migration and live view/command swap proof.

## Scope and Limitations

- **No OS I/O or ANSI emission**: `determinant` never emits escape sequences, never blocks on `getchar`, and never prints directly to stdout.
- **ASCII-centric cells**: No Unicode grapheme shaping, ANSI decoder, or cross-platform terminal backend proof.
- **No built-in pointer routing**: `TerminalPanel_childAt` and `terminal_input` exist separately; hover/click delivery is currently application-side.
- **Acyclic tree contract**: Direct self-containment is rejected on add and indirect cycles are refused at paint time, but teardown assumes an acyclic tree.
- **Caller-serialized**: Buffers, cells, and input are borrowed exclusive spans with no internal thread safety.
