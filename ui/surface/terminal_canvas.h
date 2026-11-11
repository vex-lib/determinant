#ifndef DETERMINANT_TERMINAL_CANVAS_H
#define DETERMINANT_TERMINAL_CANVAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "format/terminal_color.h"
#include "format/terminal_style.h"
#include "ui/surface/terminal_projection.h"
#include "util/constructor.h"

// ui/surface/terminal_canvas.h — the compositor primitive.
//
// A TerminalCanvas is a RECTANGULAR VIEW over a caller-owned cell GRID: four
// parallel planes that describe one screen cell each —
//   cells   (u32)  the GLYPH as a Unicode codepoint (not a byte!)
//   styles  (u16)  host-neutral attribute flags (DIM/BOLD/ITALIC/…)
//   fg      (u32)  packed 0xRRGGBBAA foreground (TERMINAL_COLOR_DEFAULT = unset)
//   bg      (u32)  packed 0xRRGGBBAA background (TERMINAL_COLOR_DEFAULT = unset)
// Every plane is optional except cells; a null plane draws nothing for that
// dimension. Widgets draw through a region and are clipped to it, so a left rail
// can never scribble into the chat column. Regions compose via TerminalCanvas_sub.
//
// Glyphs are codepoints so the toolkit is UTF-8 capable: `text` decodes UTF-8 and
// stores scalars, a d64-width glyph (wcwidth == 2) occupies its cell plus a
// continuation marker (0) in the next, and the host blits with wide cells. The
// canvas owns NO memory and NEVER names a terminal.
//
// Drawing takes a TerminalPen (style + fg + bg), so colour is one argument, not
// four. All draw operations clip and never fault on out-of-range coordinates; a
// draw that lands fully outside the region is a silent no-op (composition, not
// resource exhaustion).

// A brush: attribute flags plus foreground/background colours. Build one with
// TerminalPen_make(...) or start from TerminalPen_0() and set fields.
typedef struct TerminalPen {
    u16 style;  // TERMINAL_STYLE_* flags (16-bit plane)
    u32 fg;     // packed 0xRRGGBBAA or TERMINAL_COLOR_DEFAULT
    u32 bg;     // packed 0xRRGGBBAA or TERMINAL_COLOR_DEFAULT
} TerminalPen;

TerminalPen TerminalPen_0(void);                                  // no flags, default colours
TerminalPen TerminalPen_1(u16 style);
TerminalPen TerminalPen_2(u16 style, u32 fg);
TerminalPen TerminalPen_3(u16 style, u32 fg, u32 bg);
#define TerminalPen(...) CONSTRUCTOR_DISPATCH(TerminalPen, __VA_ARGS__)
#define TerminalPen_zero() TerminalPen_0()

typedef struct TerminalCanvas {
    u32 *cells;      // borrowed row-major glyph CODEPOINTS (gridWidth * gridHeight)
    u16 *styles;     // borrowed parallel style plane (nullable)
    u32 *fg;         // borrowed parallel foreground plane (nullable)
    u32 *bg;         // borrowed parallel background plane (nullable)
    bytes gridWidth;     // full grid columns (indexing stride)
    bytes gridHeight;    // full grid rows
    bytes x;             // this region's absolute left column
    bytes y;             // this region's absolute top row
    bytes width;         // this region's columns (already clipped to the grid)
    bytes height;        // this region's rows (already clipped to the grid)
} TerminalCanvas;

// Bind a whole grid as a canvas covering it.
TerminalCanvas TerminalCanvas_0(void);
TerminalCanvas TerminalCanvas_4(u32 *cells, u16 *styles, bytes width, bytes height);
TerminalCanvas TerminalCanvas_6(u32 *cells, u16 *styles, u32 *fg, u32 *bg,
                                bytes width, bytes height);
#define TerminalCanvas(...) CONSTRUCTOR_DISPATCH(TerminalCanvas, __VA_ARGS__)
#define TerminalCanvas_zero() TerminalCanvas_0()

bool TerminalCanvas_isValid(const TerminalCanvas *self);

// Sub-region in LOCAL coordinates, intersected with the parent region and grid.
TerminalCanvas TerminalCanvas_sub(const TerminalCanvas *self, bytes lx, bytes ly,
                                  bytes w, bytes h);

// --- Primitives (all clip; coordinates are local to this region) ---
// `fill` writes one glyph (default ' '); `text` decodes UTF-8 to codepoints.
void TerminalCanvas_fill(TerminalCanvas *self, u32 glyph, TerminalPen pen);
void TerminalCanvas_text(TerminalCanvas *self, bytes lx, bytes ly,
                         const char *text, bytes length, TerminalPen pen);
void TerminalCanvas_hline(TerminalCanvas *self, bytes ly, u32 glyph, TerminalPen pen);
void TerminalCanvas_vline(TerminalCanvas *self, bytes lx, u32 glyph, TerminalPen pen);
void TerminalCanvas_box(TerminalCanvas *self, TerminalPen pen);

typedef struct TerminalCanvasDecodedScalar {
    u32 codepoint;
    bytes bytes;
} TerminalCanvasDecodedScalar;

// Decode one UTF-8 scalar; bytes is 0 on malformed input.
TerminalCanvasDecodedScalar TerminalCanvas_utf8Decode(const char *text, bytes length);

typedef struct TerminalCanvasTextRow {
    char data[1024];
    bytes bytes;
    bool truncated;
} TerminalCanvasTextRow;

// Write ONE codepoint at a local cell (width-aware: a d64-width glyph marks
// its continuation cell). Use this for glyph tables; `text` decodes UTF-8.
void TerminalCanvas_glyph(TerminalCanvas *self, bytes lx, bytes ly, u32 codepoint, TerminalPen pen);
// Cell width for one codepoint (0 combining, 1, 2 wide) — locale-free.
bytes TerminalCanvas_widthOf(u32 codepoint);

// --- Inspection (tests / agents) ---
u32 TerminalCanvas_glyphAt(const TerminalCanvas *self, bytes lx, bytes ly);
u16 TerminalCanvas_styleAt(const TerminalCanvas *self, bytes lx, bytes ly);
u32 TerminalCanvas_fgAt(const TerminalCanvas *self, bytes lx, bytes ly);
u32 TerminalCanvas_bgAt(const TerminalCanvas *self, bytes lx, bytes ly);
bytes   TerminalCanvas_getWidth(const TerminalCanvas *self);
bytes   TerminalCanvas_getHeight(const TerminalCanvas *self);
bool     TerminalCanvas_hasStylePlane(const TerminalCanvas *self);
bool     TerminalCanvas_hasColorPlane(const TerminalCanvas *self);
// Return one region row as bounded UTF-8 text.
TerminalCanvasTextRow TerminalCanvas_copyRow(const TerminalCanvas *self, bytes ly);

TerminalProjection TerminalCanvas_toString(const TerminalCanvas *self);
TerminalProjection TerminalCanvas_toStringStruct(const TerminalCanvas *self);

#endif
