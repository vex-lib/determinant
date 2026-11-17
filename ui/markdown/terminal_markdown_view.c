#include "vex.h"
#include "ui/markdown/terminal_markdown_view.h"
#include "ui/markdown/markdown_text.h"
#include "ui/markdown/markdown_inline.h"
#include "ui/markdown/markdown_table.h"
#include "format/terminal_style.h"
#include "nio/mem.h"
#include "oop/oop.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

;;DEFINITION
/** A rendered Markdown document in terminal cells. It exists because a README is
 * not source code: a preview has to spend the cell budget on hierarchy, not on
 * the punctuation that describes it, and a terminal has no proportional type to
 * fall back on. So headings become weight and accent, emphasis becomes bold and
 * underlined without printing its asterisks, inline code becomes green text without
 * a reverse-video box, lists become hanging indents, callouts become colored
 * sections marked by a bold left wall repeated on every wrapped row, and fences
 * become full-width translucent grey bands with no box glyphs. Tables remain
 * bordered. A code block is surrounded by one blank row above and below, and the
 * same band is used when the fence sits inside a callout or blockquote.
 * A callout has a separate, highlighted `! TYPE !` label row and an unbolded body.
 * The syntax markers themselves are
 * consumed by the parser and never printed.
 *
 * The split is deliberate. Parsing and wrapping are COLD: setViewport(width)
 * walks the borrowed source once, produces flat rows and segments, and caches
 * them. render() is HOT: it walks the visible slice and writes cells, with no
 * allocation, no logging and no parsing. A resize asks for a new layout; a
 * scroll asks for nothing but a different row index. That is what lets the shell
 * keep its glide and present-on-demand path intact while previewing.
 *
 * This is a GFM-ish subset, not a CommonMark engine. It understands ATX and
 * setext headings, paragraphs, strong/emphasis, inline code, links, images,
 * unordered/ordered/task lists, block quotes, fenced code, thematic breaks and
 * simple pipe tables. It does not lay out HTML, render images, resolve reference
 * links or footnotes, do math, nest tables, or shape grapheme clusters. Table
 * columns prefer their measured natural widths and wrap at word boundaries only
 * when the whole table cannot fit the viewport; then available cells are divided
 * in proportion to each column's content need. When columns cannot share at least
 * 30 characters wide or overflow the viewport, tables allocate unwarped widths
 * and scroll horizontally rather than warping text or clipping source cells.
 * The horizontal pan belongs to the TABLE ALONE: prose, headings, lists and code
 * never move sideways, and each table row is clamped to its own overflow.
 * Ordered-list marker width comes from the full source span, with no fixed digit
 * count; siblings align to the widest marker. Emoji and other wide scalars consume
 * two terminal cells. Those are
 * stated limits, not accidents: the renderer must never pretend to a conformance
 * it does not have.
 *
 * Borrowing is explicit. The source lines belong to the caller; the view stores
 * only the pointer array and every span points into it. The caller clears the
 * source before freeing it. The style record is borrowed by value and resolved at
 * render time, so recolouring does not reflow. */

;;OVERVIEW
/** ============================================================================
 * CLASS: TerminalMarkdownView (ui/markdown/terminal_markdown_view.c)
 * STRUCT FIELDS (Mirroring ui/markdown/terminal_markdown_view.h)
 *   const char *const *lines   borrowed source lines; caller owns their bytes
 *   bytes lineCount           number of borrowed lines
 *   bool dirty                 source changed since the last layout
 *   bool ready                 a layout exists and may be rendered
 *   bool truncated             the wrapped-row limit was reached
 *   bool failed                a cold pool growth was refused
 *   bytes width               content width the current layout used
 *   bytes contentWidth        unwarped extent of the widest row/table
 *   bytes visibleRows         rows the caller can show (for the scroll clamp)
 *   bytes scroll              first wrapped row drawn
 *   bytes scrollX             shared TABLE pan offset; prose never uses it
 *   MarkdownRow *rows          owned flat wrapped-row table
 *   bytes rowCount            rows in the document (blank rows included)
 *   bytes rowCapacity         admitted row slots
 *   MarkdownSegment *segments  owned flat segment pool
 *   bytes segmentCount        live segments
 *   bytes segmentCapacity     admitted segment slots
 *   MarkdownCodeBlockRecord *codeBlocks  owned laid-out fence bookkeeping
 *   bytes codeBlockCount      fences in the current layout
 *   bytes codeBlockCapacity   admitted fence slots
 *   TerminalMarkdownStyle style  role pens resolved at render time
 *   bytes refusals            cold allocation refusals in this epoch
 * SLOT RECORD: MarkdownSegment
 *   const char *text           borrowed source span (or a static glyph)
 *   bytes length              bytes in `text`
 *   bytes repeat              how many times `text` is drawn (1 for normal text)
 *   TerminalMarkdownRole role  which style pen supplies the base colour
 *   u8 style              extra BOLD/UNDERLINE flags
 * SLOT RECORD: MarkdownRow
 *   bytes firstSegment        offset of this row's first segment
 *   bytes segmentCount        number of segments in this row
 *   bool codeBlock             this row is part of a shaded fenced code block
 * SLOT RECORD: MarkdownCodeBlockRecord
 *   bytes firstRow            first shaded document row
 *   bytes rowCount            shaded rows in this block
 *   bytes firstSourceLine     first content line in the borrowed source
 *   bytes sourceLineCount     content lines (fences excluded)
 *   bytes quoteDepth          >0 when the fence sits inside a quote/callout
 * PRIVATE HELPERS
 *   MarkdownPrefixItem  const char *text (borrowed glyph span); bytes length
 *     (span bytes); bytes cells (display width); TerminalMarkdownRole role
 *     (pen role); u8 style (extra style flags).
 *   MarkdownPrefix  MarkdownPrefixItem items[16] (row prefix); bytes count
 *     (live prefix entries); bytes markerStart (continuation boundary);
 *     bytes markerCount (marked entries); bytes cells (prefix width);
 *     bytes hang (continuation indent); bool repeatOnWrap (repeat quote gutter).
 *   MarkdownListMarker  bool valid/ordered/task/taskDone; bytes indent,
 *     contentStart, markerLength (admitted marker and content offset).
 *   MarkdownCellLine  const char *text (borrowed cell slice); bytes length
 *     (slice bytes); bytes next (next source byte offset); bytes cells
 *     (display width of the slice).
 *   cellsOf/bytesForCells(text, length, ...)  cell-aware UTF-8 measurement
 *   refuse(self, reason)      count a cold refusal, reporting once per epoch
 *   growRows/growSegments/growCodeBlocks(self)  cold doubling growth preserving live entries
 *   appendRow/appendRowForced(self)  begin a wrapped row (the notice may bypass
 *                             the document limit by one)
 *   appendSegment(self, ...)  append to the open row
 *   appendCodeBlock/markCodeRows(self, ...)  record a fence and shade its rows
 *   fenceContentText(line, depth, ...)  strip quote markers from a copy line
 *   prefixSpaces/prefixQuote/prefixBeginMarker/prefixEndMarker
 *   wrapStart/wrapSpace/wrapWord/wrapBreak
 *   findBytes/findBacktickRun/emitInline
 *   emitGap/emitHeading/emitParagraphLine/emitFence/emitQuoteFence/emitRule
 *   emitList/emitQuote
 *   tableCellLine/tableCellLineCount/tableColumnMinimum/tableAllocateWidths
 *   emitTableBorder/appendTableSpaces/emitTableRow/emitTable
 *   /splitTableRow/trimSpan;
 *   tables prefer each column's measured content width and proportionally
 *   allocate viewport cells when the natural table does not fit; ordered list siblings share
 *   the widest marker column so digit-count changes do not shift item text;
 *   callouts use the > [!TYPE] form, with a bold yellow `! TYPE !` label row,
 *   a blank gutter row, and body-role content that does not inherit quote bold;
 *   a fenced block is a borderless full-width band painted under its text, with
 *   blank rows around it; a fence inside a quote/callout keeps the quote gutter
 *   layout(self)              the cold parse + wrap pass
 * FUNCTION REGISTRY (exports)
 * Constructor: TerminalMarkdownView *TerminalMarkdownView_0() [arity chooser]
 * Lifecycle:   fn TerminalMarkdownView_free(self)
 * Source:      bool setSource(self, lines, count)
 * Layout:      bool setViewport(self, width)
 * Viewport:    bool setVisibleRows(self, rows); bytes visibleRows(self)
 *              bytes maxScroll(self)
 * Scroll:      bool setScroll(self, row); bytes getScroll(self)
 *              bool scrollBy(self, delta)
 *              bytes contentWidth/maxScrollX(self)
 *              bool setScrollX(self, column); bytes getScrollX(self)
 *              bytes rowScrollX(self, row); bool scrollXBy(self, delta)
 * Queries:     bytes rowCount/getWidth(self)
 *              bytes codeBlockCount(self)
 *              TerminalMarkdownCodeBlock codeBlockAt(self, row)
 *              TerminalMarkdownCodeLine codeBlockLine(self, block, line)
 *              bool isReady/isTruncated(self)
 *              bytes exhaustionCount(self); fn clearExhaustion(self)
 * Style:       bool setStyle(self, style); TerminalMarkdownStyle getStyle(self)
 * Render:      fn TerminalMarkdownView_render(self, canvas)
 * Projections: TerminalProjection TerminalMarkdownView_toString/toStringStruct(self)
 * CONTRACT:
 *   - setSource borrows; the caller keeps the line bytes alive and clears the
 *     source before freeing them. A null list or line with a nonzero count is refused.
 *   - setViewport(0) is refused; a null view is refused on every setter.
 *   - render is quiet and allocation-free; a null view, invalid canvas or an
 *     unready view is a no-op.
 *   - scroll is clamped in the view; a caller cannot store an offset past the
 *     document and wonder why nothing moved.
 *   - row growth refusal is counted and reported once per epoch; the view then
 *     reports not-ready rather than rendering half a document.
 *   - the row limit is an honest truncation, not a hidden cap: isTruncated()
 *     tells the caller, and a final marker row says so when there is room.
 * ============================================================================ */


;;GETTER
static inline bytes MarkdownSpan_length(const MarkdownSpan *s) { return (*s).length; }
;;GETTER
static inline bytes *MarkdownSpan_lengthMut(MarkdownSpan *s) { return &(*s).length; }
;;SETTER
static inline fn MarkdownSpan_setLength(MarkdownSpan *s, bytes v) { (*s).length = v; }
;;GETTER
static inline const char * MarkdownSpan_link(const MarkdownSpan *s) { return (*s).link; }
;;GETTER
static inline const char * *MarkdownSpan_linkMut(MarkdownSpan *s) { return &(*s).link; }
;;SETTER
static inline fn MarkdownSpan_setLink(MarkdownSpan *s, const char * v) { (*s).link = v; }
;;GETTER
static inline bytes MarkdownSpan_linkLength(const MarkdownSpan *s) { return (*s).linkLength; }
;;GETTER
static inline bytes *MarkdownSpan_linkLengthMut(MarkdownSpan *s) { return &(*s).linkLength; }
;;SETTER
static inline fn MarkdownSpan_setLinkLength(MarkdownSpan *s, bytes v) { (*s).linkLength = v; }
;;GETTER
static inline TerminalMarkdownRole MarkdownSpan_role(const MarkdownSpan *s) { return (*s).role; }
;;GETTER
static inline TerminalMarkdownRole *MarkdownSpan_roleMut(MarkdownSpan *s) { return &(*s).role; }
;;SETTER
static inline fn MarkdownSpan_setRole(MarkdownSpan *s, TerminalMarkdownRole v) { (*s).role = v; }
;;GETTER
static inline u16 MarkdownSpan_style(const MarkdownSpan *s) { return (*s).style; }
;;GETTER
static inline u16 *MarkdownSpan_styleMut(MarkdownSpan *s) { return &(*s).style; }
;;SETTER
static inline fn MarkdownSpan_setStyle(MarkdownSpan *s, u16 v) { (*s).style = v; }
;;GETTER
static inline const char * MarkdownSpan_text(const MarkdownSpan *s) { return (*s).text; }
;;GETTER
static inline const char * *MarkdownSpan_textMut(MarkdownSpan *s) { return &(*s).text; }
;;SETTER
static inline fn MarkdownSpan_setText(MarkdownSpan *s, const char * v) { (*s).text = v; }

;;GETTER
static inline TerminalPen TerminalMarkdownTheme_copied(const TerminalMarkdownTheme *s) { return (*s).copied; }
;;GETTER
static inline TerminalPen *TerminalMarkdownTheme_copiedMut(TerminalMarkdownTheme *s) { return &(*s).copied; }
;;SETTER
static inline fn TerminalMarkdownTheme_setCopied(TerminalMarkdownTheme *s, TerminalPen v) { (*s).copied = v; }
;;GETTER
static inline const char * TerminalMarkdownTheme_copiedGlyph(const TerminalMarkdownTheme *s) { return (*s).copiedGlyph; }
;;GETTER
static inline const char * *TerminalMarkdownTheme_copiedGlyphMut(TerminalMarkdownTheme *s) { return &(*s).copiedGlyph; }
;;SETTER
static inline fn TerminalMarkdownTheme_setCopiedGlyph(TerminalMarkdownTheme *s, const char * v) { (*s).copiedGlyph = v; }
;;GETTER
static inline TerminalPen TerminalMarkdownTheme_copy(const TerminalMarkdownTheme *s) { return (*s).copy; }
;;GETTER
static inline TerminalPen *TerminalMarkdownTheme_copyMut(TerminalMarkdownTheme *s) { return &(*s).copy; }
;;SETTER
static inline fn TerminalMarkdownTheme_setCopy(TerminalMarkdownTheme *s, TerminalPen v) { (*s).copy = v; }
;;GETTER
static inline const char * TerminalMarkdownTheme_copyGlyph(const TerminalMarkdownTheme *s) { return (*s).copyGlyph; }
;;GETTER
static inline const char * *TerminalMarkdownTheme_copyGlyphMut(TerminalMarkdownTheme *s) { return &(*s).copyGlyph; }
;;SETTER
static inline fn TerminalMarkdownTheme_setCopyGlyph(TerminalMarkdownTheme *s, const char * v) { (*s).copyGlyph = v; }

;;GETTER
static inline bytes TerminalMarkdownTableCellLine_cells(const TerminalMarkdownTableCellLine *s) { return (*s).cells; }
;;GETTER
static inline bytes *TerminalMarkdownTableCellLine_cellsMut(TerminalMarkdownTableCellLine *s) { return &(*s).cells; }
;;SETTER
static inline fn TerminalMarkdownTableCellLine_setCells(TerminalMarkdownTableCellLine *s, bytes v) { (*s).cells = v; }
;;GETTER
static inline bool TerminalMarkdownTableCellLine_leadingHyphen(const TerminalMarkdownTableCellLine *s) { return (*s).leadingHyphen; }
;;GETTER
static inline bool *TerminalMarkdownTableCellLine_leadingHyphenMut(TerminalMarkdownTableCellLine *s) { return &(*s).leadingHyphen; }
;;SETTER
static inline fn TerminalMarkdownTableCellLine_setLeadingHyphen(TerminalMarkdownTableCellLine *s, bool v) { (*s).leadingHyphen = v; }
;;GETTER
static inline bytes TerminalMarkdownTableCellLine_length(const TerminalMarkdownTableCellLine *s) { return (*s).length; }
;;GETTER
static inline bytes *TerminalMarkdownTableCellLine_lengthMut(TerminalMarkdownTableCellLine *s) { return &(*s).length; }
;;SETTER
static inline fn TerminalMarkdownTableCellLine_setLength(TerminalMarkdownTableCellLine *s, bytes v) { (*s).length = v; }
;;GETTER
static inline const char * TerminalMarkdownTableCellLine_text(const TerminalMarkdownTableCellLine *s) { return (*s).text; }
;;GETTER
static inline const char * *TerminalMarkdownTableCellLine_textMut(TerminalMarkdownTableCellLine *s) { return &(*s).text; }
;;SETTER
static inline fn TerminalMarkdownTableCellLine_setText(TerminalMarkdownTableCellLine *s, const char * v) { (*s).text = v; }
;;GETTER
static inline bool TerminalMarkdownTableCellLine_trailingHyphen(const TerminalMarkdownTableCellLine *s) { return (*s).trailingHyphen; }
;;GETTER
static inline bool *TerminalMarkdownTableCellLine_trailingHyphenMut(TerminalMarkdownTableCellLine *s) { return &(*s).trailingHyphen; }
;;SETTER
static inline fn TerminalMarkdownTableCellLine_setTrailingHyphen(TerminalMarkdownTableCellLine *s, bool v) { (*s).trailingHyphen = v; }
;;GETTER
static inline bool TerminalMarkdownTableCellLine_truncated(const TerminalMarkdownTableCellLine *s) { return (*s).truncated; }
;;GETTER
static inline bool *TerminalMarkdownTableCellLine_truncatedMut(TerminalMarkdownTableCellLine *s) { return &(*s).truncated; }
;;SETTER
static inline fn TerminalMarkdownTableCellLine_setTruncated(TerminalMarkdownTableCellLine *s, bool v) { (*s).truncated = v; }

#define MARKDOWN_SPACES_CAP 32u
#define MARKDOWN_PREFIX_ITEMS 16u
#define MARKDOWN_MAX_COLUMNS 16u
#define MARKDOWN_MAX_INLINE_DEPTH 8u
#define MARKDOWN_NO_INDEX ((bytes) -1)
// The code-block wash: a neutral grey at 40% alpha. It is composited over the
// host's own backdrop by the device backend, so one value reads on a dark and on a
// light theme. This is PRESENTATION, not content — nothing in the syntax depends
// on it, and a caller can replace the fence pen's background through setStyle.
#define MARKDOWN_CODE_BLOCK_BG TERMINAL_RGBA(0xA0, 0xA0, 0xA0, 0x44)
#define MARKDOWN_CODE_BLOCK_PAD 2u

static const char *const MARKDOWN_SPACES =
    "                                ";
static const char *const MARKDOWN_RULE_GLYPH = "─";
static const char *const MARKDOWN_QUOTE_GLYPH = "| ";
static const char *const MARKDOWN_BORDER_TOP = "┌";
static const char *const MARKDOWN_BORDER_MIDDLE = "├";
static const char *const MARKDOWN_BORDER_BOTTOM = "└";
static const char *const MARKDOWN_BORDER_TOP_JOIN = "┬";
static const char *const MARKDOWN_BORDER_MIDDLE_JOIN = "┼";
static const char *const MARKDOWN_BORDER_BOTTOM_JOIN = "┴";
static const char *const MARKDOWN_BORDER_RIGHT = "┐";
static const char *const MARKDOWN_BORDER_RIGHT_MIDDLE = "┤";
static const char *const MARKDOWN_BORDER_RIGHT_BOTTOM = "┘";
static const char *const MARKDOWN_BORDER_HORIZONTAL = "─";
static const char *const MARKDOWN_BORDER_VERTICAL = "│";

typedef struct MarkdownSegment {
    const char *text;
    bytes length;
    bytes repeat;
    TerminalMarkdownRole role;
    u16 style;
    u8 variant;        // heading level (1..6) or callout kind; else 0
    const char *link;       // borrowed target, or nullptr for plain text
    bytes linkLength;
} MarkdownSegment;

;;GETTER
static inline bytes MarkdownSegment_length(const MarkdownSegment *s) { return (*s).length; }
;;GETTER
static inline bytes *MarkdownSegment_lengthMut(MarkdownSegment *s) { return &(*s).length; }
;;SETTER
static inline fn MarkdownSegment_setLength(MarkdownSegment *s, bytes v) { (*s).length = v; }
;;GETTER
static inline const char * MarkdownSegment_link(const MarkdownSegment *s) { return (*s).link; }
;;GETTER
static inline const char * *MarkdownSegment_linkMut(MarkdownSegment *s) { return &(*s).link; }
;;SETTER
static inline fn MarkdownSegment_setLink(MarkdownSegment *s, const char * v) { (*s).link = v; }
;;GETTER
static inline bytes MarkdownSegment_linkLength(const MarkdownSegment *s) { return (*s).linkLength; }
;;GETTER
static inline bytes *MarkdownSegment_linkLengthMut(MarkdownSegment *s) { return &(*s).linkLength; }
;;SETTER
static inline fn MarkdownSegment_setLinkLength(MarkdownSegment *s, bytes v) { (*s).linkLength = v; }
;;GETTER
static inline bytes MarkdownSegment_repeat(const MarkdownSegment *s) { return (*s).repeat; }
;;GETTER
static inline bytes *MarkdownSegment_repeatMut(MarkdownSegment *s) { return &(*s).repeat; }
;;SETTER
static inline fn MarkdownSegment_setRepeat(MarkdownSegment *s, bytes v) { (*s).repeat = v; }
;;GETTER
static inline TerminalMarkdownRole MarkdownSegment_role(const MarkdownSegment *s) { return (*s).role; }
;;GETTER
static inline TerminalMarkdownRole *MarkdownSegment_roleMut(MarkdownSegment *s) { return &(*s).role; }
;;SETTER
static inline fn MarkdownSegment_setRole(MarkdownSegment *s, TerminalMarkdownRole v) { (*s).role = v; }
;;GETTER
static inline u16 MarkdownSegment_style(const MarkdownSegment *s) { return (*s).style; }
;;GETTER
static inline u16 *MarkdownSegment_styleMut(MarkdownSegment *s) { return &(*s).style; }
;;SETTER
static inline fn MarkdownSegment_setStyle(MarkdownSegment *s, u16 v) { (*s).style = v; }
;;GETTER
static inline const char * MarkdownSegment_text(const MarkdownSegment *s) { return (*s).text; }
;;GETTER
static inline const char * *MarkdownSegment_textMut(MarkdownSegment *s) { return &(*s).text; }
;;SETTER
static inline fn MarkdownSegment_setText(MarkdownSegment *s, const char * v) { (*s).text = v; }
;;GETTER
static inline u8 MarkdownSegment_variant(const MarkdownSegment *s) { return (*s).variant; }
;;GETTER
static inline u8 *MarkdownSegment_variantMut(MarkdownSegment *s) { return &(*s).variant; }
;;SETTER
static inline fn MarkdownSegment_setVariant(MarkdownSegment *s, u8 v) { (*s).variant = v; }


typedef struct MarkdownRow {
    bytes firstSegment;
    bytes segmentCount;
    bool codeBlock;   // this row is part of a shaded fenced code block
    bool table;       // this row belongs to a table (the only rows that pan)
    bytes extent;    // unwarped cell extent of the row, for its own pan clamp
} MarkdownRow;

;;GETTER
static inline bool MarkdownRow_codeBlock(const MarkdownRow *s) { return (*s).codeBlock; }
;;GETTER
static inline bool *MarkdownRow_codeBlockMut(MarkdownRow *s) { return &(*s).codeBlock; }
;;SETTER
static inline fn MarkdownRow_setCodeBlock(MarkdownRow *s, bool v) { (*s).codeBlock = v; }
;;GETTER
static inline bytes MarkdownRow_extent(const MarkdownRow *s) { return (*s).extent; }
;;GETTER
static inline bytes *MarkdownRow_extentMut(MarkdownRow *s) { return &(*s).extent; }
;;SETTER
static inline fn MarkdownRow_setExtent(MarkdownRow *s, bytes v) { (*s).extent = v; }
;;GETTER
static inline bytes MarkdownRow_firstSegment(const MarkdownRow *s) { return (*s).firstSegment; }
;;GETTER
static inline bytes *MarkdownRow_firstSegmentMut(MarkdownRow *s) { return &(*s).firstSegment; }
;;SETTER
static inline fn MarkdownRow_setFirstSegment(MarkdownRow *s, bytes v) { (*s).firstSegment = v; }
;;GETTER
static inline bytes MarkdownRow_segmentCount(const MarkdownRow *s) { return (*s).segmentCount; }
;;GETTER
static inline bytes *MarkdownRow_segmentCountMut(MarkdownRow *s) { return &(*s).segmentCount; }
;;SETTER
static inline fn MarkdownRow_setSegmentCount(MarkdownRow *s, bytes v) { (*s).segmentCount = v; }
;;GETTER
static inline bool MarkdownRow_table(const MarkdownRow *s) { return (*s).table; }
;;GETTER
static inline bool *MarkdownRow_tableMut(MarkdownRow *s) { return &(*s).table; }
;;SETTER
static inline fn MarkdownRow_setTable(MarkdownRow *s, bool v) { (*s).table = v; }


typedef struct MarkdownPrefixItem {
    const char *text;
    bytes length;
    bytes cells;
    TerminalMarkdownRole role;
    u16 style;
    u8 variant;        // heading level or callout kind; else 0
} MarkdownPrefixItem;

;;GETTER
static inline bytes MarkdownPrefixItem_cells(const MarkdownPrefixItem *s) { return (*s).cells; }
;;GETTER
static inline bytes *MarkdownPrefixItem_cellsMut(MarkdownPrefixItem *s) { return &(*s).cells; }
;;SETTER
static inline fn MarkdownPrefixItem_setCells(MarkdownPrefixItem *s, bytes v) { (*s).cells = v; }
;;GETTER
static inline bytes MarkdownPrefixItem_length(const MarkdownPrefixItem *s) { return (*s).length; }
;;GETTER
static inline bytes *MarkdownPrefixItem_lengthMut(MarkdownPrefixItem *s) { return &(*s).length; }
;;SETTER
static inline fn MarkdownPrefixItem_setLength(MarkdownPrefixItem *s, bytes v) { (*s).length = v; }
;;GETTER
static inline TerminalMarkdownRole MarkdownPrefixItem_role(const MarkdownPrefixItem *s) { return (*s).role; }
;;GETTER
static inline TerminalMarkdownRole *MarkdownPrefixItem_roleMut(MarkdownPrefixItem *s) { return &(*s).role; }
;;SETTER
static inline fn MarkdownPrefixItem_setRole(MarkdownPrefixItem *s, TerminalMarkdownRole v) { (*s).role = v; }
;;GETTER
static inline u16 MarkdownPrefixItem_style(const MarkdownPrefixItem *s) { return (*s).style; }
;;GETTER
static inline u16 *MarkdownPrefixItem_styleMut(MarkdownPrefixItem *s) { return &(*s).style; }
;;SETTER
static inline fn MarkdownPrefixItem_setStyle(MarkdownPrefixItem *s, u16 v) { (*s).style = v; }
;;GETTER
static inline const char * MarkdownPrefixItem_text(const MarkdownPrefixItem *s) { return (*s).text; }
;;GETTER
static inline const char * *MarkdownPrefixItem_textMut(MarkdownPrefixItem *s) { return &(*s).text; }
;;SETTER
static inline fn MarkdownPrefixItem_setText(MarkdownPrefixItem *s, const char * v) { (*s).text = v; }
;;GETTER
static inline u8 MarkdownPrefixItem_variant(const MarkdownPrefixItem *s) { return (*s).variant; }
;;GETTER
static inline u8 *MarkdownPrefixItem_variantMut(MarkdownPrefixItem *s) { return &(*s).variant; }
;;SETTER
static inline fn MarkdownPrefixItem_setVariant(MarkdownPrefixItem *s, u8 v) { (*s).variant = v; }


typedef struct MarkdownPrefix {
    MarkdownPrefixItem items[MARKDOWN_PREFIX_ITEMS];
    bytes count;
    bytes markerStart;
    bytes markerCount;
    bytes cells;
    bytes hang;
    bool repeatOnWrap;
} MarkdownPrefix;

;;GETTER
static inline bytes MarkdownPrefix_cells(const MarkdownPrefix *s) { return (*s).cells; }
;;GETTER
static inline bytes *MarkdownPrefix_cellsMut(MarkdownPrefix *s) { return &(*s).cells; }
;;SETTER
static inline fn MarkdownPrefix_setCells(MarkdownPrefix *s, bytes v) { (*s).cells = v; }
;;GETTER
static inline bytes MarkdownPrefix_count(const MarkdownPrefix *s) { return (*s).count; }
;;GETTER
static inline bytes *MarkdownPrefix_countMut(MarkdownPrefix *s) { return &(*s).count; }
;;SETTER
static inline fn MarkdownPrefix_setCount(MarkdownPrefix *s, bytes v) { (*s).count = v; }
;;GETTER
static inline bytes MarkdownPrefix_hang(const MarkdownPrefix *s) { return (*s).hang; }
;;GETTER
static inline bytes *MarkdownPrefix_hangMut(MarkdownPrefix *s) { return &(*s).hang; }
;;SETTER
static inline fn MarkdownPrefix_setHang(MarkdownPrefix *s, bytes v) { (*s).hang = v; }
;;GETTER
static inline MarkdownPrefixItem *MarkdownPrefix_itemsMut(MarkdownPrefix *s) { return (*s).items; }
;;GETTER
static inline const MarkdownPrefixItem *MarkdownPrefix_items(const MarkdownPrefix *s) { return (*s).items; }
;;GETTER
static inline bytes MarkdownPrefix_markerCount(const MarkdownPrefix *s) { return (*s).markerCount; }
;;GETTER
static inline bytes *MarkdownPrefix_markerCountMut(MarkdownPrefix *s) { return &(*s).markerCount; }
;;SETTER
static inline fn MarkdownPrefix_setMarkerCount(MarkdownPrefix *s, bytes v) { (*s).markerCount = v; }
;;GETTER
static inline bytes MarkdownPrefix_markerStart(const MarkdownPrefix *s) { return (*s).markerStart; }
;;GETTER
static inline bytes *MarkdownPrefix_markerStartMut(MarkdownPrefix *s) { return &(*s).markerStart; }
;;SETTER
static inline fn MarkdownPrefix_setMarkerStart(MarkdownPrefix *s, bytes v) { (*s).markerStart = v; }
;;GETTER
static inline bool MarkdownPrefix_repeatOnWrap(const MarkdownPrefix *s) { return (*s).repeatOnWrap; }
;;GETTER
static inline bool *MarkdownPrefix_repeatOnWrapMut(MarkdownPrefix *s) { return &(*s).repeatOnWrap; }
;;SETTER
static inline fn MarkdownPrefix_setRepeatOnWrap(MarkdownPrefix *s, bool v) { (*s).repeatOnWrap = v; }


typedef struct MarkdownWrap {
    const MarkdownPrefix *prefix;
    bytes width;
    bytes x;
    bytes prefixCells;
    bool open;
    bool started;
    bool pendingSpace;
    bool content;
} MarkdownWrap;

;;GETTER
static inline bool MarkdownWrap_content(const MarkdownWrap *s) { return (*s).content; }
;;GETTER
static inline bool *MarkdownWrap_contentMut(MarkdownWrap *s) { return &(*s).content; }
;;SETTER
static inline fn MarkdownWrap_setContent(MarkdownWrap *s, bool v) { (*s).content = v; }
;;GETTER
static inline bool MarkdownWrap_open(const MarkdownWrap *s) { return (*s).open; }
;;GETTER
static inline bool *MarkdownWrap_openMut(MarkdownWrap *s) { return &(*s).open; }
;;SETTER
static inline fn MarkdownWrap_setOpen(MarkdownWrap *s, bool v) { (*s).open = v; }
;;GETTER
static inline bool MarkdownWrap_pendingSpace(const MarkdownWrap *s) { return (*s).pendingSpace; }
;;GETTER
static inline bool *MarkdownWrap_pendingSpaceMut(MarkdownWrap *s) { return &(*s).pendingSpace; }
;;SETTER
static inline fn MarkdownWrap_setPendingSpace(MarkdownWrap *s, bool v) { (*s).pendingSpace = v; }
;;GETTER
static inline const MarkdownPrefix * MarkdownWrap_prefix(const MarkdownWrap *s) { return (*s).prefix; }
;;GETTER
static inline const MarkdownPrefix * *MarkdownWrap_prefixMut(MarkdownWrap *s) { return &(*s).prefix; }
;;SETTER
static inline fn MarkdownWrap_setPrefix(MarkdownWrap *s, const MarkdownPrefix * v) { (*s).prefix = v; }
;;GETTER
static inline bytes MarkdownWrap_prefixCells(const MarkdownWrap *s) { return (*s).prefixCells; }
;;GETTER
static inline bytes *MarkdownWrap_prefixCellsMut(MarkdownWrap *s) { return &(*s).prefixCells; }
;;SETTER
static inline fn MarkdownWrap_setPrefixCells(MarkdownWrap *s, bytes v) { (*s).prefixCells = v; }
;;GETTER
static inline bool MarkdownWrap_started(const MarkdownWrap *s) { return (*s).started; }
;;GETTER
static inline bool *MarkdownWrap_startedMut(MarkdownWrap *s) { return &(*s).started; }
;;SETTER
static inline fn MarkdownWrap_setStarted(MarkdownWrap *s, bool v) { (*s).started = v; }
;;GETTER
static inline bytes MarkdownWrap_width(const MarkdownWrap *s) { return (*s).width; }
;;GETTER
static inline bytes *MarkdownWrap_widthMut(MarkdownWrap *s) { return &(*s).width; }
;;SETTER
static inline fn MarkdownWrap_setWidth(MarkdownWrap *s, bytes v) { (*s).width = v; }
;;GETTER
static inline bytes MarkdownWrap_x(const MarkdownWrap *s) { return (*s).x; }
;;GETTER
static inline bytes *MarkdownWrap_xMut(MarkdownWrap *s) { return &(*s).x; }
;;SETTER
static inline fn MarkdownWrap_setX(MarkdownWrap *s, bytes v) { (*s).x = v; }


typedef struct MarkdownListMarker {
    bool valid;
    bool ordered;
    bool task;
    bool taskDone;
    bytes indent;
    bytes contentStart;
    bytes markerLength;
} MarkdownListMarker;

typedef TerminalMarkdownTableCellLine MarkdownCellLine;

struct TerminalMarkdownView {
    const char *const *lines;
    bytes lineCount;
    bool dirty;
    bool ready;
    bool truncated;
    bool failed;
    bytes width;
    bytes contentWidth;
    bytes visibleRows;
    bytes scroll;
    bytes scrollX;
    MarkdownRow *rows;
    bytes rowCount;
    bytes rowCapacity;
    MarkdownSegment *segments;
    bytes segmentCount;
    bytes segmentCapacity;
    TerminalMarkdownCodeBlockPool *codeBlocks;
    TerminalMarkdownTheme theme;
    TerminalMarkdownStyle style;
    bytes hoverRow;
    bytes hoverColumn;
    bytes copiedBlock;
    bytes refusals;
};

;;GETTER
static inline TerminalMarkdownCodeBlockPool * TerminalMarkdownView_codeBlocks(const TerminalMarkdownView *s) { return (*s).codeBlocks; }
;;GETTER
static inline TerminalMarkdownCodeBlockPool * *TerminalMarkdownView_codeBlocksMut(TerminalMarkdownView *s) { return &(*s).codeBlocks; }
;;SETTER
static inline fn TerminalMarkdownView_setCodeBlocks(TerminalMarkdownView *s, TerminalMarkdownCodeBlockPool * v) { (*s).codeBlocks = v; }
;;GETTER
static inline bytes TerminalMarkdownView_contentWidth(const TerminalMarkdownView *s) { return (*s).contentWidth; }
;;GETTER
static inline bytes *TerminalMarkdownView_contentWidthMut(TerminalMarkdownView *s) { return &(*s).contentWidth; }
;;SETTER
static inline fn TerminalMarkdownView_setContentWidth(TerminalMarkdownView *s, bytes v) { (*s).contentWidth = v; }
;;GETTER
static inline bytes TerminalMarkdownView_copiedBlock(const TerminalMarkdownView *s) { return (*s).copiedBlock; }
;;GETTER
static inline bytes *TerminalMarkdownView_copiedBlockMut(TerminalMarkdownView *s) { return &(*s).copiedBlock; }
;;SETTER
static inline fn TerminalMarkdownView_setCopiedBlock(TerminalMarkdownView *s, bytes v) { (*s).copiedBlock = v; }
;;GETTER
static inline bool TerminalMarkdownView_dirty(const TerminalMarkdownView *s) { return (*s).dirty; }
;;GETTER
static inline bool *TerminalMarkdownView_dirtyMut(TerminalMarkdownView *s) { return &(*s).dirty; }
;;SETTER
static inline fn TerminalMarkdownView_setDirty(TerminalMarkdownView *s, bool v) { (*s).dirty = v; }
;;GETTER
static inline bool TerminalMarkdownView_failed(const TerminalMarkdownView *s) { return (*s).failed; }
;;GETTER
static inline bool *TerminalMarkdownView_failedMut(TerminalMarkdownView *s) { return &(*s).failed; }
;;SETTER
static inline fn TerminalMarkdownView_setFailed(TerminalMarkdownView *s, bool v) { (*s).failed = v; }
;;GETTER
static inline bytes TerminalMarkdownView_hoverColumn(const TerminalMarkdownView *s) { return (*s).hoverColumn; }
;;GETTER
static inline bytes *TerminalMarkdownView_hoverColumnMut(TerminalMarkdownView *s) { return &(*s).hoverColumn; }
;;SETTER
static inline fn TerminalMarkdownView_setHoverColumn(TerminalMarkdownView *s, bytes v) { (*s).hoverColumn = v; }
;;GETTER
static inline bytes TerminalMarkdownView_hoverRow(const TerminalMarkdownView *s) { return (*s).hoverRow; }
;;GETTER
static inline bytes *TerminalMarkdownView_hoverRowMut(TerminalMarkdownView *s) { return &(*s).hoverRow; }
;;SETTER
static inline fn TerminalMarkdownView_setHoverRow(TerminalMarkdownView *s, bytes v) { (*s).hoverRow = v; }
;;GETTER
static inline bytes TerminalMarkdownView_lineCount(const TerminalMarkdownView *s) { return (*s).lineCount; }
;;GETTER
static inline bytes *TerminalMarkdownView_lineCountMut(TerminalMarkdownView *s) { return &(*s).lineCount; }
;;SETTER
static inline fn TerminalMarkdownView_setLineCount(TerminalMarkdownView *s, bytes v) { (*s).lineCount = v; }
;;GETTER
static inline const char *const * TerminalMarkdownView_lines(const TerminalMarkdownView *s) { return (*s).lines; }
;;GETTER
static inline const char *const * *TerminalMarkdownView_linesMut(TerminalMarkdownView *s) { return &(*s).lines; }
;;SETTER
static inline fn TerminalMarkdownView_setLines(TerminalMarkdownView *s, const char *const * v) { (*s).lines = v; }
;;GETTER
static inline bool TerminalMarkdownView_ready(const TerminalMarkdownView *s) { return (*s).ready; }
;;GETTER
static inline bool *TerminalMarkdownView_readyMut(TerminalMarkdownView *s) { return &(*s).ready; }
;;SETTER
static inline fn TerminalMarkdownView_setReady(TerminalMarkdownView *s, bool v) { (*s).ready = v; }
;;GETTER
static inline bytes TerminalMarkdownView_refusals(const TerminalMarkdownView *s) { return (*s).refusals; }
;;GETTER
static inline bytes *TerminalMarkdownView_refusalsMut(TerminalMarkdownView *s) { return &(*s).refusals; }
;;SETTER
static inline fn TerminalMarkdownView_setRefusals(TerminalMarkdownView *s, bytes v) { (*s).refusals = v; }
;;GETTER
static inline bytes TerminalMarkdownView_rowCapacity(const TerminalMarkdownView *s) { return (*s).rowCapacity; }
;;GETTER
static inline bytes *TerminalMarkdownView_rowCapacityMut(TerminalMarkdownView *s) { return &(*s).rowCapacity; }
;;SETTER
static inline fn TerminalMarkdownView_setRowCapacity(TerminalMarkdownView *s, bytes v) { (*s).rowCapacity = v; }
;;GETTER
static inline bytes TerminalMarkdownView_rowCount(const TerminalMarkdownView *s) { return (*s).rowCount; }
;;GETTER
static inline bytes *TerminalMarkdownView_rowCountMut(TerminalMarkdownView *s) { return &(*s).rowCount; }
;;SETTER
static inline fn TerminalMarkdownView_setRowCount(TerminalMarkdownView *s, bytes v) { (*s).rowCount = v; }
;;GETTER
static inline MarkdownRow * TerminalMarkdownView_rows(const TerminalMarkdownView *s) { return (*s).rows; }
;;GETTER
static inline MarkdownRow * *TerminalMarkdownView_rowsMut(TerminalMarkdownView *s) { return &(*s).rows; }
;;SETTER
static inline fn TerminalMarkdownView_setRows(TerminalMarkdownView *s, MarkdownRow * v) { (*s).rows = v; }
;;GETTER
static inline bytes TerminalMarkdownView_scroll(const TerminalMarkdownView *s) { return (*s).scroll; }
;;GETTER
static inline bytes *TerminalMarkdownView_scrollMut(TerminalMarkdownView *s) { return &(*s).scroll; }
;;SETTER
static inline fn TerminalMarkdownView_setScroll(TerminalMarkdownView *s, bytes v) { (*s).scroll = v; }
;;GETTER
static inline bytes TerminalMarkdownView_scrollX(const TerminalMarkdownView *s) { return (*s).scrollX; }
;;GETTER
static inline bytes *TerminalMarkdownView_scrollXMut(TerminalMarkdownView *s) { return &(*s).scrollX; }
;;SETTER
static inline fn TerminalMarkdownView_setScrollX(TerminalMarkdownView *s, bytes v) { (*s).scrollX = v; }
;;GETTER
static inline bytes TerminalMarkdownView_segmentCapacity(const TerminalMarkdownView *s) { return (*s).segmentCapacity; }
;;GETTER
static inline bytes *TerminalMarkdownView_segmentCapacityMut(TerminalMarkdownView *s) { return &(*s).segmentCapacity; }
;;SETTER
static inline fn TerminalMarkdownView_setSegmentCapacity(TerminalMarkdownView *s, bytes v) { (*s).segmentCapacity = v; }
;;GETTER
static inline bytes TerminalMarkdownView_segmentCount(const TerminalMarkdownView *s) { return (*s).segmentCount; }
;;GETTER
static inline bytes *TerminalMarkdownView_segmentCountMut(TerminalMarkdownView *s) { return &(*s).segmentCount; }
;;SETTER
static inline fn TerminalMarkdownView_setSegmentCount(TerminalMarkdownView *s, bytes v) { (*s).segmentCount = v; }
;;GETTER
static inline MarkdownSegment * TerminalMarkdownView_segments(const TerminalMarkdownView *s) { return (*s).segments; }
;;GETTER
static inline MarkdownSegment * *TerminalMarkdownView_segmentsMut(TerminalMarkdownView *s) { return &(*s).segments; }
;;SETTER
static inline fn TerminalMarkdownView_setSegments(TerminalMarkdownView *s, MarkdownSegment * v) { (*s).segments = v; }
;;GETTER
static inline TerminalMarkdownStyle TerminalMarkdownView_style(const TerminalMarkdownView *s) { return (*s).style; }
;;GETTER
static inline TerminalMarkdownStyle *TerminalMarkdownView_styleMut(TerminalMarkdownView *s) { return &(*s).style; }
;;SETTER
static inline fn TerminalMarkdownView_setStyle(TerminalMarkdownView *s, TerminalMarkdownStyle v) { (*s).style = v; }
;;GETTER
static inline TerminalMarkdownTheme TerminalMarkdownView_theme(const TerminalMarkdownView *s) { return (*s).theme; }
;;GETTER
static inline TerminalMarkdownTheme *TerminalMarkdownView_themeMut(TerminalMarkdownView *s) { return &(*s).theme; }
;;SETTER
static inline fn TerminalMarkdownView_setTheme(TerminalMarkdownView *s, TerminalMarkdownTheme v) { (*s).theme = v; }
;;GETTER
static inline bool TerminalMarkdownView_truncated(const TerminalMarkdownView *s) { return (*s).truncated; }
;;GETTER
static inline bool *TerminalMarkdownView_truncatedMut(TerminalMarkdownView *s) { return &(*s).truncated; }
;;SETTER
static inline fn TerminalMarkdownView_setTruncated(TerminalMarkdownView *s, bool v) { (*s).truncated = v; }
;;GETTER
static inline bytes TerminalMarkdownView_visibleRows(const TerminalMarkdownView *s) { return (*s).visibleRows; }
;;GETTER
static inline bytes *TerminalMarkdownView_visibleRowsMut(TerminalMarkdownView *s) { return &(*s).visibleRows; }
;;SETTER
static inline fn TerminalMarkdownView_setVisibleRows(TerminalMarkdownView *s, bytes v) { (*s).visibleRows = v; }
;;GETTER
static inline bytes TerminalMarkdownView_width(const TerminalMarkdownView *s) { return (*s).width; }
;;GETTER
static inline bytes *TerminalMarkdownView_widthMut(TerminalMarkdownView *s) { return &(*s).width; }
;;SETTER
static inline fn TerminalMarkdownView_setWidth(TerminalMarkdownView *s, bytes v) { (*s).width = v; }


static bytes cellsOf(const char *text, bytes length) {
    return TerminalMarkdownText_cells(text, length);
}

// Bytes that fit in `cells` CELLS, never splitting a UTF-8 scalar. A combining
// mark has zero width and rides with the scalar before it, so a row cannot start
// with a f32ing accent.
static bytes bytesForCells(const char *text, bytes length, bytes cells, bytes *usedCells) {
    return TerminalMarkdownText_bytesForCells(text, length, cells, usedCells);
}

static bytes scalarBytes(const char *text, bytes length) {
    return TerminalMarkdownText_scalarBytes(text, length);
}

static fn refuse(TerminalMarkdownView *self, const char *reason) {
    if (TerminalMarkdownView_refusals(self) == 0)
        THROW("TerminalMarkdownView exhausted: %s (rows %zu, segments %zu)",
              reason, TerminalMarkdownView_rowCount(self), TerminalMarkdownView_segmentCount(self));
    if (TerminalMarkdownView_refusals(self) != SIZE_MAX)
        TerminalMarkdownView_setRefusals(self, TerminalMarkdownView_refusals(self) + 1u);
    TerminalMarkdownView_setFailed(self, true);
}

static bool growRows(TerminalMarkdownView *self) {
    bytes next = TerminalMarkdownView_rowCapacity(self) == 0 ? TERMINAL_MARKDOWN_ROWS_DEFAULT
                                           : TerminalMarkdownView_rowCapacity(self) * 2u;
    if (next < TerminalMarkdownView_rowCapacity(self) || next > SIZE_MAX / sizeof(MarkdownRow)) {
        refuse(self, "row capacity overflow");
        return false;
    }
    MarkdownRow *grown = Memory_alloc(TYPE_BYTE_ARRAY, next * sizeof(MarkdownRow));
    if (grown == nullptr) {
        refuse(self, "row pool allocation refused");
        return false;
    }
    if (TerminalMarkdownView_rowCapacity(self) != 0)
        memcpy(grown, TerminalMarkdownView_rows(self), TerminalMarkdownView_rowCapacity(self) * sizeof(MarkdownRow));
    Memory_free(TerminalMarkdownView_rows(self));
    TerminalMarkdownView_setRows(self, grown);
    TerminalMarkdownView_setRowCapacity(self, next);
    return true;
}

static bool growSegments(TerminalMarkdownView *self) {
    bytes next = TerminalMarkdownView_segmentCapacity(self) == 0 ? TERMINAL_MARKDOWN_SEGMENTS_DEFAULT
                                              : TerminalMarkdownView_segmentCapacity(self) * 2u;
    if (next < TerminalMarkdownView_segmentCapacity(self) || next > SIZE_MAX / sizeof(MarkdownSegment)) {
        refuse(self, "segment capacity overflow");
        return false;
    }
    MarkdownSegment *grown = Memory_alloc(TYPE_BYTE_ARRAY, next * sizeof(MarkdownSegment));
    if (grown == nullptr) {
        refuse(self, "segment pool allocation refused");
        return false;
    }
    if (TerminalMarkdownView_segmentCapacity(self) != 0)
        memcpy(grown, TerminalMarkdownView_segments(self), TerminalMarkdownView_segmentCapacity(self) * sizeof(MarkdownSegment));
    Memory_free(TerminalMarkdownView_segments(self));
    TerminalMarkdownView_setSegments(self, grown);
    TerminalMarkdownView_setSegmentCapacity(self, next);
    return true;
}

// Mark every row in [first, last) as a shaded code row. Wrapping can append rows
// inside a single logical line, so the range is marked once the fence is complete
// instead of trying to tag each row at creation.
static fn markCodeRows(TerminalMarkdownView *self, bytes first) {
    for (bytes index = first; index < TerminalMarkdownView_rowCount(self); ++index)
        TerminalMarkdownView_rows(self)[index].codeBlock = true;
}

// Mark every row a table emitted as a table row. The blank gap row before the
// top border is included; its extent is zero, so its own pan clamp is zero too.
static fn markTableRows(TerminalMarkdownView *self, bytes first) {
    for (bytes index = first; index < TerminalMarkdownView_rowCount(self); ++index)
        TerminalMarkdownView_rows(self)[index].table = true;
}

static bool appendRowInternal(TerminalMarkdownView *self, bool force) {
    if (!force && TerminalMarkdownView_rowCount(self) + 1u >= TERMINAL_MARKDOWN_ROWS_LIMIT) {
        TerminalMarkdownView_setTruncated(self, true);
        return false;
    }
    if (TerminalMarkdownView_rowCount(self) == TerminalMarkdownView_rowCapacity(self) && !growRows(self))
        return false;
    TerminalMarkdownView_rows(self)[TerminalMarkdownView_rowCount(self)] = (MarkdownRow) { TerminalMarkdownView_segmentCount(self), 0, false,
                                                     false, 0u };
    TerminalMarkdownView_setRowCount(self, TerminalMarkdownView_rowCount(self) + 1u);
    return true;
}

static bool appendRow(TerminalMarkdownView *self) {
    return appendRowInternal(self, false);
}

static bool appendSegment(TerminalMarkdownView *self, const char *text, bytes length,
                          bytes repeat, TerminalMarkdownRole role, u16 style) {
    if (self == nullptr || TerminalMarkdownView_rowCount(self) == 0 || text == nullptr)
        return false;
    if (TerminalMarkdownView_segmentCount(self) == TerminalMarkdownView_segmentCapacity(self) && !growSegments(self))
        return false;
    MarkdownRow *row = &TerminalMarkdownView_rowsMut(self)[TerminalMarkdownView_rowCount(self) - 1u];
    if (TerminalMarkdownView_segmentCount(self) != MarkdownRow_firstSegment(row) + MarkdownRow_segmentCount(row))
        return false;
    MarkdownSegment *segment = &TerminalMarkdownView_segmentsMut(self)[TerminalMarkdownView_segmentCount(self)];
    MarkdownSegment_setText(segment, text);
    MarkdownSegment_setLength(segment, length);
    MarkdownSegment_setRepeat(segment, repeat == 0 ? 1u : repeat);
    MarkdownSegment_setRole(segment, role);
    MarkdownSegment_setStyle(segment, style);
    MarkdownSegment_setVariant(segment, 0);
    MarkdownSegment_setLink(segment, nullptr);
    MarkdownSegment_setLinkLength(segment, 0);
    TerminalMarkdownView_setSegmentCount(self, TerminalMarkdownView_segmentCount(self) + 1u);
    MarkdownRow_setSegmentCount(row, MarkdownRow_segmentCount(row) + 1u);
    return true;
}

// Stamp every segment appended since `first` with the structure the row belongs
// to: the heading level or callout kind, and the borrowed link target (if any).
// Wrap can split one span across rows, so the range — not the last segment — is
// what carries the identity.
static bool stampRange(TerminalMarkdownView *self, bytes first, u8 variant,
                       const char *link, bytes linkLength) {
    if (self == nullptr || (variant == 0 && link == nullptr))
        return true;
    for (bytes index = first; index < TerminalMarkdownView_segmentCount(self); ++index) {
        MarkdownSegment *segment = &TerminalMarkdownView_segmentsMut(self)[index];
        MarkdownSegment_setVariant(segment, variant);
        MarkdownSegment_setLink(segment, link);
        MarkdownSegment_setLinkLength(segment, linkLength);
    }
    return true;
}

static fn prefixAddVariant(MarkdownPrefix *prefix, const char *text, bytes length,
                             TerminalMarkdownRole role, u16 style, u8 variant) {
    if (prefix == nullptr || MarkdownPrefix_count(prefix) >= MARKDOWN_PREFIX_ITEMS || text == nullptr)
        return;
    MarkdownPrefixItem *item = &MarkdownPrefix_itemsMut(prefix)[MarkdownPrefix_count(prefix)];
    MarkdownPrefixItem_setText(item, text);
    MarkdownPrefixItem_setLength(item, length);
    MarkdownPrefixItem_setCells(item, cellsOf(text, length));
    MarkdownPrefixItem_setRole(item, role);
    MarkdownPrefixItem_setStyle(item, style);
    MarkdownPrefixItem_setVariant(item, variant);
    MarkdownPrefix_setCells(prefix, MarkdownPrefix_cells(prefix) + (MarkdownPrefixItem_cells(item)));
    MarkdownPrefix_setCount(prefix, MarkdownPrefix_count(prefix) + 1u);
}

static fn prefixAdd(MarkdownPrefix *prefix, const char *text, bytes length,
                      TerminalMarkdownRole role, u16 style) {
    prefixAddVariant(prefix, text, length, role, style, 0);
}

static fn prefixSpaces(MarkdownPrefix *prefix, bytes cells,
                         TerminalMarkdownRole role, u16 style) {
    while (cells > 0) {
        bytes take = cells > MARKDOWN_SPACES_CAP ? MARKDOWN_SPACES_CAP : cells;
        prefixAdd(prefix, MARKDOWN_SPACES, take, role, style);
        cells -= take;
    }
}

static fn prefixQuote(MarkdownPrefix *prefix, bytes depth) {
    for (bytes level = 0; level < depth && MarkdownPrefix_count(prefix) < MARKDOWN_PREFIX_ITEMS; ++level)
        prefixAdd(prefix, MARKDOWN_QUOTE_GLYPH, strlen(MARKDOWN_QUOTE_GLYPH),
                  TERMINAL_MARKDOWN_QUOTE, TERMINAL_STYLE_BOLD);
    MarkdownPrefix_setRepeatOnWrap(prefix, true);
}

static fn prefixBeginMarker(MarkdownPrefix *prefix) {
    MarkdownPrefix_setMarkerStart(prefix, MarkdownPrefix_count(prefix));
    MarkdownPrefix_setMarkerCount(prefix, 0);
    MarkdownPrefix_setHang(prefix, 0);
}

static fn prefixEndMarker(MarkdownPrefix *prefix) {
    if (MarkdownPrefix_count(prefix) < MarkdownPrefix_markerStart(prefix))
        MarkdownPrefix_setCount(prefix, MarkdownPrefix_markerStart(prefix));
    MarkdownPrefix_setMarkerCount(prefix, MarkdownPrefix_count(prefix) - MarkdownPrefix_markerStart(prefix));
    for (bytes index = MarkdownPrefix_markerStart(prefix); index < MarkdownPrefix_count(prefix); ++index)
        MarkdownPrefix_setHang(prefix, MarkdownPrefix_hang(prefix) + (MarkdownPrefix_itemsMut(prefix)[index].cells));
}

static bool wrapStart(TerminalMarkdownView *self, MarkdownWrap *wrap, bool first) {
    if (!appendRow(self))
        return false;
    MarkdownWrap_setOpen(wrap, true);
    MarkdownWrap_setX(wrap, 0);
    MarkdownWrap_setContent(wrap, false);
    MarkdownWrap_setPendingSpace(wrap, false);
    const MarkdownPrefix *prefix = MarkdownWrap_prefix(wrap);
    if (prefix != nullptr) {
        bytes limit = first || MarkdownPrefix_repeatOnWrap(prefix)
            ? MarkdownPrefix_count(prefix) : MarkdownPrefix_markerStart(prefix);
        for (bytes index = 0; index < limit; ++index) {
            const MarkdownPrefixItem *item = &MarkdownPrefix_itemsMut(prefix)[index];
            bytes first = TerminalMarkdownView_segmentCount(self);
            if (!appendSegment(self, MarkdownPrefixItem_text(item), MarkdownPrefixItem_length(item), 1u, MarkdownPrefixItem_role(item), MarkdownPrefixItem_style(item)))
                return false;
            stampRange(self, first, MarkdownPrefixItem_variant(item), nullptr, 0);
            MarkdownWrap_setX(wrap, MarkdownWrap_x(wrap) + (MarkdownPrefixItem_cells(item)));
        }
        if (!first && !MarkdownPrefix_repeatOnWrap(prefix) && MarkdownPrefix_hang(prefix) != 0) {
            if (!appendSegment(self, MARKDOWN_SPACES, MarkdownPrefix_hang(prefix), 1u,
                               TERMINAL_MARKDOWN_BODY, 0))
                return false;
            MarkdownWrap_setX(wrap, MarkdownWrap_x(wrap) + (MarkdownPrefix_hang(prefix)));
        }
    }
    MarkdownWrap_setPrefixCells(wrap, MarkdownWrap_x(wrap));
    MarkdownWrap_setStarted(wrap, true);
    return true;
}

static fn wrapSpace(MarkdownWrap *wrap) {
    if (wrap != nullptr)
        MarkdownWrap_setPendingSpace(wrap, true);
}

static bool wrapWord(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                     bytes length, TerminalMarkdownRole role, u16 style) {
    if (wrap == nullptr || text == nullptr || length == 0)
        return true;
    if (!MarkdownWrap_open(wrap) && !wrapStart(self, wrap, !MarkdownWrap_started(wrap)))
        return false;
    bytes wordCells = cellsOf(text, length);
    bytes gap = MarkdownWrap_pendingSpace(wrap) && MarkdownWrap_content(wrap) ? 1u : 0u;
    if (MarkdownWrap_content(wrap) && MarkdownWrap_x(wrap) + gap + wordCells > MarkdownWrap_width(wrap)) {
        if (!wrapStart(self, wrap, false))
            return false;
        gap = 0;
    }
    if (gap != 0) {
        // The gap belongs to the span that follows it, so a link label is one
        // hover target and a heading's inter-word space keeps the heading pen.
        if (!appendSegment(self, " ", 1u, 1u, role, 0))
            return false;
        MarkdownWrap_setX(wrap, MarkdownWrap_x(wrap) + 1u);
    }
    MarkdownWrap_setPendingSpace(wrap, false);
    bytes available = MarkdownWrap_width(wrap) > MarkdownWrap_x(wrap) ? MarkdownWrap_width(wrap) - MarkdownWrap_x(wrap) : 0u;
    if (wordCells <= 30u && wordCells <= available) {
        if (!appendSegment(self, text, length, 1u, role, style))
            return false;
        MarkdownWrap_setX(wrap, MarkdownWrap_x(wrap) + (wordCells));
        MarkdownWrap_setContent(wrap, true);
        return true;
    }
    bytes at = 0u;
    bool continuing = false;
    while (at < length) {
        available = MarkdownWrap_width(wrap) > MarkdownWrap_x(wrap) ? MarkdownWrap_width(wrap) - MarkdownWrap_x(wrap) : 0u;
        if (available <= (continuing ? 2u : 1u)) {
            if (!wrapStart(self, wrap, false))
                return false;
            available = MarkdownWrap_width(wrap) > MarkdownWrap_x(wrap) ? MarkdownWrap_width(wrap) - MarkdownWrap_x(wrap) : 1u;
        }
        if (continuing) {
            if (!appendSegment(self, "-", 1u, 1u, role, style))
                return false;
            MarkdownWrap_setX(wrap, MarkdownWrap_x(wrap) + 1u);
            MarkdownWrap_setContent(wrap, true);
            available = MarkdownWrap_width(wrap) > MarkdownWrap_x(wrap) ? MarkdownWrap_width(wrap) - MarkdownWrap_x(wrap) : 0u;
        }
        bytes remainingLength = length - at;
        bytes remainingCells = cellsOf(text + at, remainingLength);
        bytes maxCells = available > 30u ? 30u : available;
        bytes budget = maxCells;
        if (remainingCells > maxCells) {
            budget = maxCells > 1u ? maxCells - 1u : 1u;
        }
        bytes used = 0u;
        bytes take = bytesForCells(text + at, remainingLength, budget, &used);
        if (take == 0u) {
            take = scalarBytes(text + at, remainingLength);
            used = cellsOf(text + at, take);
        }
        if (!appendSegment(self, text + at, take, 1u, role, style))
            return false;
        MarkdownWrap_setX(wrap, MarkdownWrap_x(wrap) + (used));
        MarkdownWrap_setContent(wrap, true);
        at += take;
        if (at < length) {
            if (!appendSegment(self, "-", 1u, 1u, role, style))
                return false;
            MarkdownWrap_setX(wrap, MarkdownWrap_x(wrap) + 1u);
            if (!wrapStart(self, wrap, false))
                return false;
            continuing = true;
        }
    }
    return true;
}

static bool wrapBreak(TerminalMarkdownView *self, MarkdownWrap *wrap) {
    if (wrap == nullptr || !MarkdownWrap_open(wrap) || !MarkdownWrap_content(wrap))
        return true;
    MarkdownWrap_setPendingSpace(wrap, false);
    return wrapStart(self, wrap, false);
}

static bytes findBytes(const char *text, bytes length, bytes from,
                        const char *needle, bytes needleLength) {
    return TerminalMarkdownText_find(text, length, from, needle, needleLength);
}

static bool isSpace(char character) {
    return character == ' ' || character == '\t';
}

static bool appendPlain(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                        bytes length, TerminalMarkdownRole role, u16 style) {
    bytes at = 0;
    while (at < length) {
        if (isSpace(text[at])) {
            wrapSpace(wrap);
            ++at;
            continue;
        }
        bytes start = at;
        while (at < length && !isSpace(text[at]))
            ++at;
        if (!wrapWord(self, wrap, text + start, at - start, role, style))
            return false;
    }
    return true;
}

// Place ONE parsed span: wrap it as a word (which can split it across rows), then
// give every segment it produced the span's link and the row's variant.
static bool wrapSpan(TerminalMarkdownView *self, MarkdownWrap *wrap, const MarkdownSpan *span,
                     u8 variant) {
    bytes first = TerminalMarkdownView_segmentCount(self);
    // A code span is one unbreakable unit; every other span wraps at spaces, so
    // `**two words**` does not become a single 11-cell word.
    bool placed = MarkdownSpan_role(span) == TERMINAL_MARKDOWN_CODE
                ? wrapWord(self, wrap, MarkdownSpan_text(span), MarkdownSpan_length(span), MarkdownSpan_role(span), MarkdownSpan_style(span))
                : appendPlain(self, wrap, MarkdownSpan_text(span), MarkdownSpan_length(span), MarkdownSpan_role(span),
                              MarkdownSpan_style(span));
    if (!placed)
        return false;
    return stampRange(self, first, variant, MarkdownSpan_link(span), MarkdownSpan_linkLength(span));
}

// Parse a source span COLD into flat spans, then place them. The parser owns the
// grammar (escapes, code runs, emphasis, links, images, autolinks); the view owns
// only word wrapping. A span buffer that overflowed still parsed the tail, so the
// bytes past the last emitted span are placed literally rather than dropped.
static bool emitSpans(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                      bytes length, TerminalMarkdownRole base, u16 style,
                      u8 variant) {
    MarkdownSpan spans[MARKDOWN_INLINE_SPANS];
    bool complete = true;
    bytes count = TerminalMarkdownInline_parse(text, length, base, style, spans,
                                                MARKDOWN_INLINE_SPANS, &complete);
    for (bytes index = 0; index < count; ++index)
        if (!wrapSpan(self, wrap, &spans[index], variant))
            return false;
    if (complete || count == 0)
        return true;
    const MarkdownSpan *last = &spans[count - 1u];
    bytes consumed = (bytes) (MarkdownSegment_text(last) + MarkdownSegment_length(last) - text);
    if (consumed >= length)
        return true;
    return appendPlain(self, wrap, text + consumed, length - consumed, base, style);
}

static bytes leadingSpaces(const char *text, bytes length) {
    return TerminalMarkdownText_leadingSpaces(text, length);
}

static bool lineBlank(const char *text, bytes length) {
    return TerminalMarkdownText_blank(text, length);
}

static bytes trimRight(const char *text, bytes length) {
    return TerminalMarkdownText_trimRight(text, length);
}

static bytes headingLevel(const char *text, bytes length) {
    bytes spaces = leadingSpaces(text, length);
    if (spaces > 3u)
        return 0;
    bytes hashes = 0;
    while (spaces + hashes < length && text[spaces + hashes] == '#')
        ++hashes;
    if (hashes == 0 || hashes > 6u)
        return 0;
    if (spaces + hashes < length && text[spaces + hashes] != ' ')
        return 0;
    return hashes;
}

static bytes headingTextStart(const char *text, bytes length, bytes *outLength) {
    bytes spaces = leadingSpaces(text, length);
    bytes hashes = 0;
    while (spaces + hashes < length && text[spaces + hashes] == '#')
        ++hashes;
    bytes start = spaces + hashes;
    if (start < length && text[start] == ' ')
        ++start;
    bytes end = start;
    while (end < length)
        ++end;
    while (end > start && isSpace(text[end - 1u]))
        --end;
    while (end > start && text[end - 1u] == '#') {
        bytes candidate = end - 1u;
        while (candidate > start && text[candidate - 1u] == '#')
            --candidate;
        if (candidate == start || text[candidate - 1u] == ' ')
            end = candidate;
        else
            break;
    }
    while (end > start && isSpace(text[end - 1u]))
        --end;
    *outLength = end - start;
    return start;
}

static bool isFence(const char *text, bytes length, char *marker, bytes *run,
                    bytes *contentStart) {
    bytes spaces = leadingSpaces(text, length);
    if (spaces > 3u || spaces >= length)
        return false;
    char candidate = text[spaces];
    if (candidate != '`' && candidate != '~')
        return false;
    bytes count = 0;
    while (spaces + count < length && text[spaces + count] == candidate)
        ++count;
    if (count < 3u)
        return false;
    *marker = candidate;
    *run = count;
    *contentStart = spaces + count;
    return true;
}

static bool isClosingFence(const char *text, bytes length, char marker, bytes run) {
    bytes spaces = leadingSpaces(text, length);
    if (spaces > 3u || spaces >= length || text[spaces] != marker)
        return false;
    bytes count = 0;
    while (spaces + count < length && text[spaces + count] == marker)
        ++count;
    if (count < run)
        return false;
    for (bytes at = spaces + count; at < length; ++at)
        if (!isSpace(text[at]))
            return false;
    return true;
}

static bool isThematic(const char *text, bytes length) {
    char marker = 0;
    bytes count = 0;
    for (bytes at = 0; at < length; ++at) {
        char current = text[at];
        if (isSpace(current))
            continue;
        if (marker == 0)
            marker = current;
        else if (current != marker)
            return false;
        ++count;
    }
    return count >= 3u && (marker == '-' || marker == '*' || marker == '_');
}

static bytes setextLevel(const char *text, bytes length) {
    bytes spaces = leadingSpaces(text, length);
    if (spaces > 3u || spaces >= length)
        return 0;
    char marker = text[spaces];
    if (marker != '=' && marker != '-')
        return 0;
    bytes count = 0;
    bytes at = spaces;
    while (at < length && text[at] == marker) {
        ++at;
        ++count;
    }
    if (count == 0)
        return 0;
    for (; at < length; ++at)
        if (!isSpace(text[at]))
            return 0;
    return marker == '=' ? 1u : 2u;
}

static bool quoteLine(const char *text, bytes length, bytes *depth, bytes *contentStart) {
    bytes spaces = leadingSpaces(text, length);
    if (spaces > 3u || spaces >= length || text[spaces] != '>')
        return false;
    bytes at = spaces;
    bytes count = 0;
    while (at < length && text[at] == '>') {
        ++count;
        ++at;
        if (at < length && text[at] == ' ')
            ++at;
    }
    *depth = count;
    *contentStart = at;
    return true;
}

static MarkdownListMarker listMarker(const char *text, bytes length) {
    MarkdownListMarker marker = {0};
    bytes spaces = leadingSpaces(text, length);
    if (spaces >= length)
        return marker;
    bytes at = spaces;
    bool ordered = false;
    if (text[at] == '-' || text[at] == '*' || text[at] == '+') {
        ++at;
    } else if (text[at] >= '0' && text[at] <= '9') {
        while (at < length && text[at] >= '0' && text[at] <= '9')
            ++at;
        if (at == spaces || at >= length || (text[at] != '.' && text[at] != ')'))
            return marker;
        ++at;
        ordered = true;
    } else {
        return marker;
    }
    bytes markerEnd = at;
    if (at >= length || text[at] != ' ')
        return marker;
    ++at;
    if (at < length && text[at] == ' ')
        ++at;
    marker.valid = true;
    marker.ordered = ordered;
    marker.indent = spaces;
    marker.markerLength = markerEnd - spaces + 1u;
    marker.contentStart = at;
    if (at + 3u < length && text[at] == '[' && text[at + 2u] == ']'
        && text[at + 3u] == ' ') {
        char state = text[at + 1u];
        if (state == ' ' || state == 'x' || state == 'X') {
            marker.task = true;
            marker.taskDone = state == 'x' || state == 'X';
            marker.contentStart = at + 4u;
        }
    }
    return marker;
}

static bool emitGap(TerminalMarkdownView *self) {
    if (TerminalMarkdownView_rowCount(self) == 0)
        return true;
    MarkdownRow *last = &TerminalMarkdownView_rowsMut(self)[TerminalMarkdownView_rowCount(self) - 1u];
    if (MarkdownRow_segmentCount(last) == 0)
        return true;
    return appendRow(self);
}

static bool emitRule(TerminalMarkdownView *self, bytes width) {
    if (width == 0)
        return true;
    if (!appendRow(self))
        return false;
    return appendSegment(self, MARKDOWN_RULE_GLYPH, strlen(MARKDOWN_RULE_GLYPH), width,
                         TERMINAL_MARKDOWN_RULE, 0);
}

static bool emitHeading(TerminalMarkdownView *self, const char *text, bytes length, bytes level) {
    if (!emitGap(self))
        return false;
    MarkdownPrefix prefix = {0};
    MarkdownWrap wrap = { .prefix = &prefix, .width = TerminalMarkdownView_width(self) };
    bytes bodyStart = headingTextStart(text, length, &length);
    // The level travels with every segment, so the theme can resolve a different
    // pen for `#` and `####` at render time without reflowing the document.
    if (!emitSpans(self, &wrap, text + bodyStart, length, TERMINAL_MARKDOWN_HEADING, 0,
                   (u8) level))
        return false;
    if (level == 1u && TerminalMarkdownView_width(self) != 0)
        return emitRule(self, TerminalMarkdownView_width(self));
    return true;
}

static bool emitParagraphLine(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                              bytes length) {
    bytes spaces = leadingSpaces(text, length);
    if (spaces < length && spaces <= 3u)
        return emitSpans(self, wrap, text + spaces, length - spaces, TERMINAL_MARKDOWN_BODY, 0, 0);
    return emitSpans(self, wrap, text, length, TERMINAL_MARKDOWN_BODY, 0, 0);
}

// One fenced block, painted through whatever gutter the container uses. The
// prefix is the ONLY difference between a top-level fence, one inside a quote and
// one inside a callout: the line walk, the shading and the copy range are shared.
// `quoteDepth` is the number of `>` markers each source line carries (0 at top
// level), so the copy projection can strip exactly those.
static bool emitFenceBlock(TerminalMarkdownView *self, const char *const *lines, bytes lineCount,
                           bytes *index, const MarkdownPrefix *prefix,
                           char marker, bytes run, bytes quoteDepth) {
    bytes start = *index;
    bytes at = start + 1u;
    bytes contentEnd = at;
    bytes firstRow = TerminalMarkdownView_rowCount(self);
    MarkdownWrap wrap = { .prefix = prefix, .width = TerminalMarkdownView_width(self) };
    bool first = true;
    while (at < lineCount) {
        const char *line = lines[at];
        bytes length = strlen(line);
        bytes contentOffset = 0;
        if (quoteDepth != 0) {
            bytes lineDepth = 0;
            bytes lineContent = 0;
            if (!quoteLine(line, length, &lineDepth, &lineContent) || lineDepth != quoteDepth)
                break;
            contentOffset = lineContent;
        }
        if (isClosingFence(line + contentOffset, length - contentOffset, marker, run)) {
            contentEnd = at;
            ++at;
            break;
        }
        contentEnd = at + 1u;
        if (!wrapStart(self, &wrap, first))
            return false;
        first = false;
        if (length > contentOffset) {
            bytes codeLength = length - contentOffset;
            bytes codeCells = cellsOf(line + contentOffset, codeLength);
            if (!appendSegment(self, line + contentOffset, codeLength, 1u,
                               TERMINAL_MARKDOWN_FENCE, 0))
                return false;
            wrap.x += codeCells;
            wrap.content = true;
        }
        ++at;
    }
    markCodeRows(self, firstRow);
    if (!TerminalMarkdownCodeBlockPool_add(TerminalMarkdownView_codeBlocks(self), firstRow,
                                           TerminalMarkdownView_rowCount(self) - firstRow, start + 1u,
                                           contentEnd - (start + 1u), quoteDepth))
        return false;
    *index = at;
    return true;
}

static bool emitFence(TerminalMarkdownView *self, const char *const *lines, bytes lineCount,
                      bytes *index, char marker, bytes run, bytes contentStart) {
    (discard) contentStart;
    if (!emitGap(self))
        return false;
    MarkdownPrefix prefix = {0};
    prefixSpaces(&prefix, MARKDOWN_CODE_BLOCK_PAD, TERMINAL_MARKDOWN_FENCE, 0);
    prefix.repeatOnWrap = true;
    if (!emitFenceBlock(self, lines, lineCount, index, &prefix, marker, run, 0))
        return false;
    return emitGap(self);
}

static bool emitQuoteFence(TerminalMarkdownView *self, const char *const *lines,
                           bytes lineCount, bytes *index, char marker, bytes run,
                           bytes quoteDepth) {
    MarkdownPrefix prefix = {0};
    prefixQuote(&prefix, quoteDepth);
    prefix.repeatOnWrap = true;
    return emitFenceBlock(self, lines, lineCount, index, &prefix, marker, run, quoteDepth);
}

static bool emitList(TerminalMarkdownView *self, const char *const *lines, bytes lineCount,
                     bytes *index) {
    if (!emitGap(self))
        return false;
    bytes at = *index;
    while (at < lineCount) {
        bytes length = strlen(lines[at]);
        MarkdownListMarker marker = listMarker(lines[at], length);
        if (!marker.valid)
            break;
        bytes orderedMarkerWidth = marker.ordered ? marker.markerLength : 0u;
        if (marker.ordered) {
            bytes scan = at + 1u;
            while (scan < lineCount) {
                bytes scanLength = strlen(lines[scan]);
                if (lineBlank(lines[scan], scanLength)) {
                    ++scan;
                    continue;
                }
                MarkdownListMarker candidate = listMarker(lines[scan], scanLength);
                if (candidate.valid) {
                    if (candidate.ordered && candidate.indent == marker.indent
                        && candidate.markerLength > orderedMarkerWidth)
                        orderedMarkerWidth = candidate.markerLength;
                    if (candidate.indent <= marker.indent)
                        ++scan;
                    else
                        break;
                    continue;
                }
                if (leadingSpaces(lines[scan], scanLength) < marker.contentStart)
                    break;
                ++scan;
            }
        }
        MarkdownPrefix prefix = {0};
        bytes indent = marker.indent;
        if (indent > TerminalMarkdownView_width(self) / 2u)
            indent = TerminalMarkdownView_width(self) / 2u;
        prefixSpaces(&prefix, indent, TERMINAL_MARKDOWN_BODY, 0);
        prefixBeginMarker(&prefix);
        if (marker.task) {
            prefixAdd(&prefix, marker.taskDone ? "☑ " : "☐ ", 4u,
                      TERMINAL_MARKDOWN_MARKER, 0);
        } else if (marker.ordered) {
            if (orderedMarkerWidth > marker.markerLength)
                prefixSpaces(&prefix, orderedMarkerWidth - marker.markerLength,
                             TERMINAL_MARKDOWN_BODY, 0);
            prefixAdd(&prefix, lines[at] + marker.indent, marker.markerLength,
                      TERMINAL_MARKDOWN_MARKER, 0);
        } else {
            prefixAdd(&prefix, "• ", strlen("• "), TERMINAL_MARKDOWN_MARKER, 0);
        }
        prefixEndMarker(&prefix);
        MarkdownWrap wrap = { .prefix = &prefix, .width = TerminalMarkdownView_width(self) };
        bytes contentEnd = trimRight(lines[at], length);
        if (contentEnd > marker.contentStart) {
            if (!emitSpans(self, &wrap, lines[at] + marker.contentStart,
                           contentEnd - marker.contentStart, TERMINAL_MARKDOWN_BODY, 0, 0))
                return false;
        } else if (!wrapStart(self, &wrap, true)) {
            return false;
        }
        bytes next = at + 1u;
        while (next < lineCount) {
            bytes nextLength = strlen(lines[next]);
            if (lineBlank(lines[next], nextLength))
                break;
            MarkdownListMarker nested = listMarker(lines[next], nextLength);
            if (nested.valid)
                break;
            bytes spaces = leadingSpaces(lines[next], nextLength);
            if (spaces < marker.contentStart)
                break;
            wrapSpace(&wrap);
            bytes end = trimRight(lines[next], nextLength);
            if (!emitSpans(self, &wrap, lines[next] + spaces, end - spaces,
                           TERMINAL_MARKDOWN_BODY, 0, 0))
                return false;
            ++next;
        }
        at = next;
        while (at < lineCount && lineBlank(lines[at], strlen(lines[at])))
            ++at;
        if (at < lineCount && !listMarker(lines[at], strlen(lines[at])).valid)
            break;
    }
    *index = at;
    return true;
}

static bool startsBlock(const char *text, bytes length);

// The gutter every callout shares: the bar, one space, repeated on every wrapped
// row. Severity is the pen, not the shape, so a NOTE and a CAUTION read as the
// same kind of object with different urgency.
static fn prefixCallout(MarkdownPrefix *prefix, TerminalMarkdownCalloutKind kind) {
    prefixAddVariant(prefix, TerminalMarkdownCallout_bar(),
                     strlen(TerminalMarkdownCallout_bar()),
                     TERMINAL_MARKDOWN_CALLOUT, TERMINAL_STYLE_BOLD, (u8) kind);
    prefixAddVariant(prefix, " ", 1u, TERMINAL_MARKDOWN_CALLOUT, 0, (u8) kind);
    MarkdownPrefix_setRepeatOnWrap(prefix, true);
}

// A callout: a bold `KIND` label on the bar's row, the optional title beside it,
// and an unbolded BODY-role body indented by the bar. The marker itself is never
// printed. Fences inside the aside keep the bar as their gutter.
static bool emitCallout(TerminalMarkdownView *self, const char *const *lines, bytes lineCount,
                        bytes *index, bytes depth, bytes contentStart,
                        TerminalMarkdownCallout callout) {
    if (!emitGap(self))
        return false;
    MarkdownPrefix prefix = {0};
    if (depth > 1u)
        prefixQuote(&prefix, depth - 1u);
    prefixCallout(&prefix, callout.kind);
    MarkdownWrap wrap = { .prefix = &prefix, .width = TerminalMarkdownView_width(self) };
    bytes at = *index;
    bytes length = strlen(lines[at]);
    bytes end = trimRight(lines[at], length);
    if (!wrapStart(self, &wrap, true))
        return false;
    const char *name = TerminalMarkdownCallout_name(callout.kind);
    bytes nameCells = cellsOf(name, strlen(name));
    bytes first = TerminalMarkdownView_segmentCount(self);
    if (!appendSegment(self, name, strlen(name), 1u, TERMINAL_MARKDOWN_CALLOUT,
                       TERMINAL_STYLE_BOLD))
        return false;
    stampRange(self, first, (u8) callout.kind, nullptr, 0);
    wrap.x += nameCells;
    wrap.content = true;
    if (contentStart < end) {
        wrapSpace(&wrap);
        if (!emitSpans(self, &wrap, lines[at] + contentStart, end - contentStart,
                       TERMINAL_MARKDOWN_CALLOUT, 0, (u8) callout.kind))
            return false;
    }
    bool body = false;
    bool labelOpen = true;
    ++at;
    while (at < lineCount) {
        bytes lineLength = strlen(lines[at]);
        bytes content = 0;
        if (depth != 0) {
            bytes lineDepth = 0;
            if (!quoteLine(lines[at], lineLength, &lineDepth, &content) || lineDepth != depth)
                break;
        }
        char fenceMarker = 0;
        bytes fenceRun = 0;
        bytes fenceOffset = 0;
        bool fence = isFence(lines[at] + content, lineLength - content,
                             &fenceMarker, &fenceRun, &fenceOffset);
        if (!fence && depth == 0 && startsBlock(lines[at], lineLength))
            break;
        bytes lineEnd = trimRight(lines[at], lineLength);
        if (fence) {
            if (labelOpen && !body) {
                if (!wrapBreak(self, &wrap))
                    return false;
                labelOpen = false;
            }
            wrap.open = false;
            wrap.started = false;
            wrap.content = false;
            if (!emitGap(self))
                return false;
            bytes blockStart = at;
            if (!emitFenceBlock(self, lines, lineCount, &at, &prefix,
                                fenceMarker, fenceRun, depth))
                return false;
            if (at == blockStart)
                ++at;
            if (!emitGap(self))
                return false;
            body = true;
            continue;
        }
        if (lineEnd <= content) {
            if (depth == 0)
                break;
            if (wrap.open && wrap.content && !wrapBreak(self, &wrap))
                return false;
            ++at;
            continue;
        }
        if (!body) {
            if (labelOpen && !wrapBreak(self, &wrap))
                return false;
            labelOpen = false;
            body = true;
        } else {
            wrapSpace(&wrap);
        }
        if (!emitSpans(self, &wrap, lines[at] + content, lineEnd - content,
                       TERMINAL_MARKDOWN_BODY, 0, 0))
            return false;
        ++at;
    }
    *index = at;
    return true;
}

// A blockquote, or a callout when its first content span names a severity. The
// two spellings of a callout — GFM's `> [!NOTE]` and a README's `!NOTE!` — both
// land here; only the marker is different, so only the marker is parsed away.
static bool emitQuote(TerminalMarkdownView *self, const char *const *lines, bytes lineCount,
                      bytes *index) {
    if (!emitGap(self))
        return false;
    bytes at = *index;
    bytes depth = 0;
    bytes contentStart = 0;
    if (!quoteLine(lines[at], strlen(lines[at]), &depth, &contentStart))
        return false;
    bytes length = strlen(lines[at]);
    bytes end = trimRight(lines[at], length);
    TerminalMarkdownCallout callout =
        TerminalMarkdownCallout_parse(lines[at] + contentStart, end > contentStart
                                                             ? end - contentStart : 0u);
    if (callout.valid)
        return emitCallout(self, lines, lineCount, index, depth,
                           contentStart + callout.contentStart, callout);
    MarkdownPrefix prefix = {0};
    prefixQuote(&prefix, depth);
    MarkdownWrap wrap = { .prefix = &prefix, .width = TerminalMarkdownView_width(self) };
    while (at < lineCount) {
        bytes lineLength = strlen(lines[at]);
        bytes lineDepth = 0;
        bytes lineContent = 0;
        if (!quoteLine(lines[at], lineLength, &lineDepth, &lineContent))
            break;
        if (lineDepth != depth)
            break;
        char fenceMarker = 0;
        bytes fenceRun = 0;
        bytes fenceOffset = 0;
        if (isFence(lines[at] + lineContent, lineLength - lineContent,
                    &fenceMarker, &fenceRun, &fenceOffset)) {
            if (!emitGap(self))
                return false;
            wrap.open = false;
            wrap.started = false;
            wrap.pendingSpace = false;
            wrap.content = false;
            if (!emitQuoteFence(self, lines, lineCount, &at, fenceMarker, fenceRun, depth))
                return false;
            if (!emitGap(self))
                return false;
            continue;
        }
        bytes lineEnd = trimRight(lines[at], lineLength);
        if (lineEnd > lineContent) {
            wrapSpace(&wrap);
            if (!emitSpans(self, &wrap, lines[at] + lineContent, lineEnd - lineContent,
                           TERMINAL_MARKDOWN_QUOTE, 0, 0))
                return false;
        } else if (!wrap.open) {
            if (!wrapStart(self, &wrap, !wrap.started))
                return false;
        } else if (!wrapBreak(self, &wrap)) {
            return false;
        }
        ++at;
    }
    *index = at;
    return true;
}

static bool tableCandidate(const char *text, bytes length, const char *next, bytes nextLength,
                           bytes *columns) {
    const char *cells[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    bytes lengths[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    if (TerminalMarkdownTable_split(text, length, cells, lengths,
                                    TERMINAL_MARKDOWN_TABLE_COLUMNS) < 2u)
        return false;
    bytes count = TerminalMarkdownTable_parseDelimiter(next, nextLength, nullptr, 0);
    if (count < 2u)
        return false;
    if (columns != nullptr)
        *columns = count;
    return true;
}

static bool appendTableSpaces(TerminalMarkdownView *self, bytes cells) {
    while (cells != 0u) {
        bytes chunk = cells > MARKDOWN_SPACES_CAP ? MARKDOWN_SPACES_CAP : cells;
        if (!appendSegment(self, MARKDOWN_SPACES, chunk, 1u, TERMINAL_MARKDOWN_BODY, 0))
            return false;
        cells -= chunk;
    }
    return true;
}

// Place an already-wrapped cell slice exactly where the width policy put it: no
// re-wrapping, because the table decided the break. Inline styling still applies,
// so `**bold**` and `` `code` `` in a cell lose their markers like they do in a
// paragraph.
static bool appendCellSpans(TerminalMarkdownView *self, const char *text, bytes length,
                            TerminalMarkdownRole base, u16 style) {
    MarkdownSpan spans[MARKDOWN_INLINE_SPANS];
    bool complete = true;
    bytes count = TerminalMarkdownInline_parse(text, length, base, style, spans,
                                                MARKDOWN_INLINE_SPANS, &complete);
    bytes consumed = 0;
    for (bytes index = 0; index < count; ++index) {
        const MarkdownSpan *span = &spans[index];
        bytes first = TerminalMarkdownView_segmentCount(self);
        if (!appendSegment(self, MarkdownSpan_text(span), MarkdownSpan_length(span), 1u, MarkdownSpan_role(span), MarkdownSpan_style(span)))
            return false;
        stampRange(self, first, 0, MarkdownSpan_link(span), MarkdownSpan_linkLength(span));
        consumed = (bytes) (MarkdownSpan_text(span) + MarkdownSpan_length(span) - text);
    }
    if (complete || consumed >= length)
        return true;
    return appendSegment(self, text + consumed, length - consumed, 1u, base, style);
}

// One visual line of one cell: alignment padding, the styled text, and — only
// when the width policy had to cut inside a word — an honest ellipsis.
static bool emitCellLine(TerminalMarkdownView *self, const TerminalMarkdownTableCellLine *line,
                         bytes width, TerminalMarkdownTableAlign align,
                         TerminalMarkdownRole role, u16 style) {
    bytes used = TerminalMarkdownTableCellLine_cells(line);
    if (used > width)
        used = width;
    bytes slack = width - used;
    bytes lead = align == TERMINAL_MARKDOWN_TABLE_CENTER ? slack / 2u
                : align == TERMINAL_MARKDOWN_TABLE_RIGHT ? slack : 0u;
    if (lead != 0 && !appendTableSpaces(self, lead))
        return false;
    if (TerminalMarkdownTableCellLine_leadingHyphen(line)) {
        if (!appendSegment(self, "-", 1u, 1u, role, style))
            return false;
    }
    if (!appendCellSpans(self, TerminalMarkdownTableCellLine_text(line), TerminalMarkdownTableCellLine_length(line), role, style))
        return false;
    if (TerminalMarkdownTableCellLine_trailingHyphen(line)) {
        if (!appendSegment(self, "-", 1u, 1u, role, style))
            return false;
    }
    bytes tail = slack - lead;
    if (TerminalMarkdownTableCellLine_truncated(line)) {
        if (!appendSegment(self, "…", strlen("…"), 1u, role, style))
            return false;
        if (tail != 0)
            --tail;
    }
    return tail == 0 || appendTableSpaces(self, tail);
}

static bool emitTableBorder(TerminalMarkdownView *self, const bytes *widths,
                            bytes columns, bytes kind) {
    const char *left = kind == 0u ? MARKDOWN_BORDER_TOP
                      : kind == 1u ? MARKDOWN_BORDER_MIDDLE : MARKDOWN_BORDER_BOTTOM;
    const char *join = kind == 0u ? MARKDOWN_BORDER_TOP_JOIN
                      : kind == 1u ? MARKDOWN_BORDER_MIDDLE_JOIN : MARKDOWN_BORDER_BOTTOM_JOIN;
    const char *right = kind == 0u ? MARKDOWN_BORDER_RIGHT
                       : kind == 1u ? MARKDOWN_BORDER_RIGHT_MIDDLE
                                    : MARKDOWN_BORDER_RIGHT_BOTTOM;
    if (!appendRow(self)
        || !appendSegment(self, left, strlen(left), 1u, TERMINAL_MARKDOWN_TABLE_BORDER, 0))
        return false;
    for (bytes column = 0; column < columns; ++column) {
        if (!appendSegment(self, MARKDOWN_BORDER_HORIZONTAL,
                           strlen(MARKDOWN_BORDER_HORIZONTAL), widths[column] + 2u,
                           TERMINAL_MARKDOWN_TABLE_BORDER, 0))
            return false;
        if (column + 1u < columns
            && !appendSegment(self, join, strlen(join), 1u,
                              TERMINAL_MARKDOWN_TABLE_BORDER, 0))
            return false;
    }
    return appendSegment(self, right, strlen(right), 1u, TERMINAL_MARKDOWN_TABLE_BORDER, 0);
}

static bool emitTableRow(TerminalMarkdownView *self, const char *text, bytes length,
                         bytes columns, const bytes *widths,
                         const TerminalMarkdownTableAlign *aligns, bool heading) {
    const char *cells[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    bytes lengths[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    bytes count = TerminalMarkdownTable_split(text, length, cells, lengths,
                                               TERMINAL_MARKDOWN_TABLE_COLUMNS);
    bytes offsets[TERMINAL_MARKDOWN_TABLE_COLUMNS] = {0};
    bytes heights[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    bytes rowHeight = 1u;
    for (bytes column = 0; column < columns; ++column) {
        heights[column] = column < count
                        ? TerminalMarkdownTable_lineCount(cells[column], lengths[column],
                                                          widths[column])
                        : 1u;
        if (heights[column] > rowHeight)
            rowHeight = heights[column];
    }
    TerminalMarkdownRole role = heading ? TERMINAL_MARKDOWN_TABLE_HEADER
                                        : TERMINAL_MARKDOWN_BODY;
    for (bytes visualRow = 0; visualRow < rowHeight; ++visualRow) {
        if (!appendRow(self)
            || !appendSegment(self, MARKDOWN_BORDER_VERTICAL,
                              strlen(MARKDOWN_BORDER_VERTICAL), 1u,
                              TERMINAL_MARKDOWN_TABLE_BORDER, 0)
            || !appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0))
            return false;
        for (bytes column = 0; column < columns; ++column) {
            TerminalMarkdownTableCellLine line = {0};
            if (visualRow < heights[column]) {
                const char *cell = column < count ? cells[column] : "";
                bytes cellLength = column < count ? lengths[column] : 0u;
                line = TerminalMarkdownTable_cellLine(cell, cellLength, offsets[column],
                                                      widths[column]);
                offsets[column] = line.next;
            }
            if (!emitCellLine(self, &line, widths[column], aligns[column], role, 0))
                return false;
            if (column + 1u < columns) {
                if (!appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0)
                    || !appendSegment(self, MARKDOWN_BORDER_VERTICAL,
                                      strlen(MARKDOWN_BORDER_VERTICAL), 1u,
                                      TERMINAL_MARKDOWN_TABLE_BORDER, 0)
                    || !appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0))
                    return false;
            }
        }
        if (!appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0)
            || !appendSegment(self, MARKDOWN_BORDER_VERTICAL,
                              strlen(MARKDOWN_BORDER_VERTICAL), 1u,
                              TERMINAL_MARKDOWN_TABLE_BORDER, 0))
            return false;
    }
    return true;
}

static bool emitTable(TerminalMarkdownView *self, const char *const *lines, bytes lineCount,
                      bytes *index, bytes columns) {
    if (columns > TERMINAL_MARKDOWN_TABLE_COLUMNS)
        columns = TERMINAL_MARKDOWN_TABLE_COLUMNS;
    const char *cells[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    bytes lengths[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    bytes header = *index;
    TerminalMarkdownTableAlign aligns[TERMINAL_MARKDOWN_TABLE_COLUMNS] = {0};
    bytes declared = TerminalMarkdownTable_parseDelimiter(lines[header + 1u],
                                                           strlen(lines[header + 1u]),
                                                           aligns,
                                                           TERMINAL_MARKDOWN_TABLE_COLUMNS);
    if (declared < columns)
        columns = declared;
    bytes body = header + 2u;
    bytes end = body;
    while (end < lineCount) {
        bytes length = strlen(lines[end]);
        if (lineBlank(lines[end], length)
            || findBytes(lines[end], length, 0, "|", 1u) == MARKDOWN_NO_INDEX)
            break;
        ++end;
    }
    bytes needed[TERMINAL_MARKDOWN_TABLE_COLUMNS] = {0};
    bytes minimums[TERMINAL_MARKDOWN_TABLE_COLUMNS] = {0};
    bytes widths[TERMINAL_MARKDOWN_TABLE_COLUMNS] = {0};
    bytes firstRow = TerminalMarkdownView_rowCount(self);
    for (bytes row = header; row < end; ++row) {
        bytes count = TerminalMarkdownTable_split(lines[row], strlen(lines[row]), cells, lengths,
                                                   TERMINAL_MARKDOWN_TABLE_COLUMNS);
        for (bytes column = 0; column < columns; ++column) {
            if (column >= count)
                continue;
            bytes natural = TerminalMarkdownTable_natural(cells[column], lengths[column]);
            if (natural > needed[column])
                needed[column] = natural;
            bytes word = TerminalMarkdownTable_longestWord(cells[column], lengths[column]);
            if (word > minimums[column])
                minimums[column] = word;
        }
    }
    for (bytes column = 0; column < columns; ++column) {
        if (needed[column] == 0u)
            needed[column] = 1u;
        if (minimums[column] == 0u)
            minimums[column] = 1u;
    }
    bool warped = !TerminalMarkdownTable_allocate(needed, minimums, widths, columns, TerminalMarkdownView_width(self))
               || TerminalMarkdownTable_isWarped(needed, widths, columns, TERMINAL_MARKDOWN_TABLE_WARP_THRESHOLD);
    if (warped) {
        TerminalMarkdownTable_allocateUnwarped(needed, widths, columns);
    }
    bytes tableTotalWidth = columns * 3u + 1u;
    for (bytes column = 0; column < columns; ++column)
        tableTotalWidth += widths[column];
    if (tableTotalWidth > TerminalMarkdownView_contentWidth(self))
        TerminalMarkdownView_setContentWidth(self, tableTotalWidth);
    if (!emitGap(self))
        return false;
    if (!emitTableBorder(self, widths, columns, 0u)
        || !emitTableRow(self, lines[header], strlen(lines[header]), columns, widths,
                         aligns, true))
        return false;
    if (body == end) {
        if (!emitTableBorder(self, widths, columns, 2u))
            return false;
        markTableRows(self, firstRow);
        *index = end;
        return true;
    }
    if (!emitTableBorder(self, widths, columns, 1u))
        return false;
    for (bytes row = body; row < end; ++row) {
        if (!emitTableRow(self, lines[row], strlen(lines[row]), columns, widths, aligns, false))
            return false;
        if (!emitTableBorder(self, widths, columns, row + 1u < end ? 1u : 2u))
            return false;
    }
    markTableRows(self, firstRow);
    *index = end;
    return true;
}

static bool startsBlock(const char *text, bytes length) {
    if (lineBlank(text, length))
        return true;
    if (headingLevel(text, length) != 0)
        return true;
    char marker = 0;
    bytes run = 0;
    bytes contentStart = 0;
    if (isFence(text, length, &marker, &run, &contentStart))
        return true;
    if (isThematic(text, length))
        return true;
    if (listMarker(text, length).valid)
        return true;
    bytes depth = 0;
    if (quoteLine(text, length, &depth, &contentStart))
        return true;
    return false;
}

static bool layout(TerminalMarkdownView *self) {
    TerminalMarkdownCodeBlockPool_reset(TerminalMarkdownView_codeBlocks(self));
    TerminalMarkdownView_setContentWidth(self, TerminalMarkdownView_width(self));
    bytes index = 0;
    while (index < TerminalMarkdownView_lineCount(self) && !TerminalMarkdownView_failed(self) && !TerminalMarkdownView_truncated(self)) {
        const char *line = TerminalMarkdownView_lines(self)[index];
        bytes length = strlen(line);
        if (lineBlank(line, length)) {
            ++index;
            continue;
        }
        char fenceMarker = 0;
        bytes fenceRun = 0;
        bytes fenceContent = 0;
        if (isFence(line, length, &fenceMarker, &fenceRun, &fenceContent)) {
            if (!emitFence(self, TerminalMarkdownView_lines(self), TerminalMarkdownView_lineCount(self), &index,
                           fenceMarker, fenceRun, fenceContent))
                break;
            continue;
        }
        bytes level = headingLevel(line, length);
        if (level != 0) {
            if (!emitHeading(self, line, length, level))
                break;
            ++index;
            continue;
        }
        if (index + 1u < TerminalMarkdownView_lineCount(self)) {
            bytes nextLength = strlen(TerminalMarkdownView_lines(self)[index + 1u]);
            bytes setext = setextLevel(TerminalMarkdownView_lines(self)[index + 1u], nextLength);
            if (setext != 0 && !startsBlock(line, length)) {
                if (!emitHeading(self, line, length, setext))
                    break;
                index += 2u;
                continue;
            }
        }
        if (isThematic(line, length)) {
            if (!emitGap(self))
                break;
            if (!emitRule(self, TerminalMarkdownView_width(self)))
                break;
            ++index;
            continue;
        }
        if (listMarker(line, length).valid) {
            if (!emitList(self, TerminalMarkdownView_lines(self), TerminalMarkdownView_lineCount(self), &index))
                break;
            continue;
        }
        bytes quoteDepth = 0;
        bytes quoteContent = 0;
        if (quoteLine(line, length, &quoteDepth, &quoteContent)) {
            if (!emitQuote(self, TerminalMarkdownView_lines(self), TerminalMarkdownView_lineCount(self), &index))
                break;
            continue;
        }
        bytes calloutSpaces = leadingSpaces(line, length);
        if (calloutSpaces <= 3u && calloutSpaces < length && line[calloutSpaces] == '!') {
            bytes calloutEnd = trimRight(line, length);
            TerminalMarkdownCallout callout =
                TerminalMarkdownCallout_parse(line + calloutSpaces, calloutEnd - calloutSpaces);
            if (callout.valid) {
                if (!emitCallout(self, TerminalMarkdownView_lines(self), TerminalMarkdownView_lineCount(self), &index, 0,
                                 calloutSpaces + callout.contentStart, callout))
                    break;
                continue;
            }
        }
        if (index + 1u < TerminalMarkdownView_lineCount(self)) {
            bytes nextLength = strlen(TerminalMarkdownView_lines(self)[index + 1u]);
            bytes columns = 0;
            if (tableCandidate(line, length, TerminalMarkdownView_lines(self)[index + 1u], nextLength, &columns)) {
                if (!emitTable(self, TerminalMarkdownView_lines(self), TerminalMarkdownView_lineCount(self), &index, columns))
                    break;
                continue;
            }
        }
        if (!emitGap(self))
            break;
        MarkdownPrefix prefix = {0};
        MarkdownWrap wrap = { .prefix = &prefix, .width = TerminalMarkdownView_width(self) };
        bool firstLine = true;
        while (index < TerminalMarkdownView_lineCount(self)) {
            const char *paragraph = TerminalMarkdownView_lines(self)[index];
            bytes paragraphLength = strlen(paragraph);
            if (lineBlank(paragraph, paragraphLength))
                break;
            if (!firstLine && startsBlock(paragraph, paragraphLength))
                break;
            if (firstLine && startsBlock(paragraph, paragraphLength))
                break;
            bytes rightTrim = trimRight(paragraph, paragraphLength);
            bool hardBreak = rightTrim != paragraphLength
                          || (rightTrim > 0 && paragraph[rightTrim - 1u] == '\\');
            if (hardBreak && rightTrim > 0 && paragraph[rightTrim - 1u] == '\\')
                --rightTrim;
            if (!firstLine) {
                if (hardBreak) {
                    if (!wrapBreak(self, &wrap))
                        break;
                } else {
                    wrapSpace(&wrap);
                }
            }
            if (!emitParagraphLine(self, &wrap, paragraph, rightTrim))
                break;
            if (hardBreak && !wrapBreak(self, &wrap))
                break;
            firstLine = false;
            ++index;
        }
    }
    if (TerminalMarkdownView_truncated(self) && TerminalMarkdownView_rowCount(self) + 1u < TERMINAL_MARKDOWN_ROWS_LIMIT) {
        if (!appendRowInternal(self, true))
            return false;
        if (!appendSegment(self, "… preview truncated", strlen("… preview truncated"), 1u,
                           TERMINAL_MARKDOWN_MARKER, 0))
            return false;
    }
    for (bytes r = 0; r < TerminalMarkdownView_rowCount(self); ++r) {
        const MarkdownRow *rowRec = &TerminalMarkdownView_rowsMut(self)[r];
        bytes rowWidth = 0;
        for (bytes s = 0; s < MarkdownRow_segmentCount(rowRec); ++s) {
            const MarkdownSegment *seg = &TerminalMarkdownView_segmentsMut(self)[MarkdownRow_firstSegment(rowRec) + s];
            rowWidth += cellsOf(MarkdownSegment_text(seg), MarkdownSegment_length(seg)) * MarkdownSegment_repeat(seg);
        }
        if (rowWidth > TerminalMarkdownView_contentWidth(self))
            TerminalMarkdownView_setContentWidth(self, rowWidth);
        TerminalMarkdownView_rows(self)[r].extent = rowWidth;
    }
    return !TerminalMarkdownView_failed(self);
}

TerminalMarkdownView *TerminalMarkdownView_0() {
    TerminalMarkdownView *self = Memory_alloc(TYPE_BYTE_ARRAY, sizeof(TerminalMarkdownView));
    if (self == nullptr)
        return nullptr;
    *self = (TerminalMarkdownView) {0};
    TerminalMarkdownView_setCodeBlocks(self, TerminalMarkdownCodeBlockPool_0());
    if (TerminalMarkdownView_codeBlocks(self) == nullptr) {
        Memory_free(self);
        return nullptr;
    }
    TerminalMarkdownView_setTheme(self, TerminalMarkdownTheme_0());
    TerminalMarkdownView_setStyle(self, TerminalMarkdownTheme_style(TerminalMarkdownView_themeMut(self)));
    TerminalMarkdownView_setHoverRow(self, MARKDOWN_NO_INDEX);
    TerminalMarkdownView_setHoverColumn(self, MARKDOWN_NO_INDEX);
    TerminalMarkdownView_setCopiedBlock(self, MARKDOWN_NO_INDEX);
    return self;
}

fn TerminalMarkdownView_free(TerminalMarkdownView *self) {
    if (self == nullptr)
        return;
    Memory_free(TerminalMarkdownView_rows(self));
    Memory_free(TerminalMarkdownView_segments(self));
    TerminalMarkdownCodeBlockPool_free(TerminalMarkdownView_codeBlocks(self));
    Memory_free(self);
}

bool TerminalMarkdownView_setSource(TerminalMarkdownView *self, const char *const *lines,
                                    bytes count) {
    if (self == nullptr) {
        THROW("TerminalMarkdownView source: null view");
        return false;
    }
    if (count != 0 && lines == nullptr) {
        THROW("TerminalMarkdownView source: null list");
        return false;
    }
    for (bytes index = 0; index < count; ++index)
        if (lines[index] == nullptr) {
            THROW("TerminalMarkdownView source: null line at %zu", index);
            return false;
        }
    TerminalMarkdownView_setLines(self, lines);
    TerminalMarkdownView_setLineCount(self, count);
    TerminalMarkdownView_setDirty(self, true);
    TerminalMarkdownView_setReady(self, false);
    TerminalMarkdownView_setTruncated(self, false);
    TerminalMarkdownView_setFailed(self, false);
    TerminalMarkdownView_setRowCount(self, 0);
    TerminalMarkdownView_setSegmentCount(self, 0);
    TerminalMarkdownCodeBlockPool_reset(TerminalMarkdownView_codeBlocks(self));
    TerminalMarkdownView_setHoverRow(self, MARKDOWN_NO_INDEX);
    TerminalMarkdownView_setHoverColumn(self, MARKDOWN_NO_INDEX);
    TerminalMarkdownView_setCopiedBlock(self, MARKDOWN_NO_INDEX);
    TerminalMarkdownView_setScroll(self, 0);
    TerminalMarkdownView_setScrollX(self, 0);
    TerminalMarkdownView_setContentWidth(self, 0);
    return true;
}

bool TerminalMarkdownView_setViewport(TerminalMarkdownView *self, bytes width) {
    if (self == nullptr) {
        THROW("TerminalMarkdownView viewport: null view");
        return false;
    }
    if (width == 0) {
        THROW("TerminalMarkdownView viewport: zero width");
        return false;
    }
    if (TerminalMarkdownView_ready(self) && !TerminalMarkdownView_dirty(self) && TerminalMarkdownView_width(self) == width)
        return true;
    TerminalMarkdownView_setWidth(self, width);
    TerminalMarkdownView_setDirty(self, false);
    TerminalMarkdownView_setReady(self, false);
    TerminalMarkdownView_setTruncated(self, false);
    TerminalMarkdownView_setFailed(self, false);
    TerminalMarkdownView_setRowCount(self, 0);
    TerminalMarkdownView_setSegmentCount(self, 0);
    TerminalMarkdownCodeBlockPool_reset(TerminalMarkdownView_codeBlocks(self));
    TerminalMarkdownView_setCopiedBlock(self, MARKDOWN_NO_INDEX);
    if (TerminalMarkdownView_lines(self) == nullptr || TerminalMarkdownView_lineCount(self) == 0) {
        TerminalMarkdownView_setReady(self, true);
        TerminalMarkdownView_setScroll(self, 0);
        TerminalMarkdownView_setScrollX(self, 0);
        TerminalMarkdownView_setContentWidth(self, width);
        return true;
    }
    if (!layout(self)) {
        TerminalMarkdownView_setReady(self, false);
        return false;
    }
    TerminalMarkdownView_setReady(self, true);
    if (TerminalMarkdownView_scroll(self) > TerminalMarkdownView_maxScroll(self))
        TerminalMarkdownView_setScroll(self, TerminalMarkdownView_maxScroll(self));
    if (TerminalMarkdownView_scrollX(self) > TerminalMarkdownView_maxScrollX(self))
        TerminalMarkdownView_setScrollX(self, TerminalMarkdownView_maxScrollX(self));
    return true;
}

bytes TerminalMarkdownView_rowCount(const TerminalMarkdownView *self) {
    return self != nullptr ? TerminalMarkdownView_rowCount(self) : 0u;
}

bytes TerminalMarkdownView_getWidth(const TerminalMarkdownView *self) {
    return self != nullptr ? TerminalMarkdownView_width(self) : 0u;
}

bool TerminalMarkdownView_setVisibleRows(TerminalMarkdownView *self, bytes rows) {
    if (self == nullptr) {
        THROW("TerminalMarkdownView visible rows: null view");
        return false;
    }
    TerminalMarkdownView_setVisibleRows(self, rows);
    if (TerminalMarkdownView_scroll(self) > TerminalMarkdownView_maxScroll(self))
        TerminalMarkdownView_setScroll(self, TerminalMarkdownView_maxScroll(self));
    return true;
}

bytes TerminalMarkdownView_visibleRows(const TerminalMarkdownView *self) {
    return self != nullptr ? TerminalMarkdownView_visibleRows(self) : 0u;
}

bytes TerminalMarkdownView_maxScroll(const TerminalMarkdownView *self) {
    if (self == nullptr)
        return 0u;
    if (TerminalMarkdownView_rowCount(self) <= TerminalMarkdownView_visibleRows(self))
        return 0u;
    return TerminalMarkdownView_rowCount(self) - TerminalMarkdownView_visibleRows(self);
}

bool TerminalMarkdownView_setScroll(TerminalMarkdownView *self, bytes row) {
    if (self == nullptr) {
        THROW("TerminalMarkdownView scroll: null view");
        return false;
    }
    bytes maximum = TerminalMarkdownView_maxScroll(self);
    TerminalMarkdownView_setScroll(self, row > maximum ? maximum : row);
    return true;
}

bytes TerminalMarkdownView_getScroll(const TerminalMarkdownView *self) {
    return self != nullptr ? TerminalMarkdownView_scroll(self) : 0u;
}

bool TerminalMarkdownView_scrollBy(TerminalMarkdownView *self, i64 delta) {
    if (self == nullptr)
        return false;
    bytes maximum = TerminalMarkdownView_maxScroll(self);
    bytes wanted = TerminalMarkdownView_scroll(self);
    if (delta < 0) {
        bytes amount = (bytes) -delta;
        wanted = amount > wanted ? 0u : wanted - amount;
    } else if (delta > 0) {
        wanted += (bytes) delta;
        if (wanted > maximum)
            wanted = maximum;
    }
    if (wanted == TerminalMarkdownView_scroll(self))
        return false;
    TerminalMarkdownView_setScroll(self, wanted);
    return true;
}

bytes TerminalMarkdownView_contentWidth(const TerminalMarkdownView *self) {
    return self != nullptr ? TerminalMarkdownView_contentWidth(self) : 0u;
}

bytes TerminalMarkdownView_maxScrollX(const TerminalMarkdownView *self) {
    if (self == nullptr)
        return 0u;
    if (TerminalMarkdownView_contentWidth(self) <= TerminalMarkdownView_width(self))
        return 0u;
    return TerminalMarkdownView_contentWidth(self) - TerminalMarkdownView_width(self);
}

// The pan one row may take. Prose, headings and code never move: their wrapped
// extent is already the viewport. Only a table row may pan, and only by the
// overflow it actually has, so a table that fits stays put even when a wider one
// shares the document-wide offset.
static bytes rowScrollX(const TerminalMarkdownView *self, bytes row) {
    if (self == nullptr || row >= TerminalMarkdownView_rowCount(self))
        return 0u;
    const MarkdownRow *record = &TerminalMarkdownView_rowsMut(self)[row];
    if (!MarkdownRow_table(record) && !MarkdownRow_codeBlock(record))
        return 0u;
    bytes limit = MarkdownRow_extent(record) > TerminalMarkdownView_width(self) ? MarkdownRow_extent(record) - TerminalMarkdownView_width(self) : 0u;
    return TerminalMarkdownView_scrollX(self) < limit ? TerminalMarkdownView_scrollX(self) : limit;
}

bool TerminalMarkdownView_setScrollX(TerminalMarkdownView *self, bytes column) {
    if (self == nullptr) {
        THROW("TerminalMarkdownView scrollX: null view");
        return false;
    }
    bytes maximum = TerminalMarkdownView_maxScrollX(self);
    TerminalMarkdownView_setScrollX(self, column > maximum ? maximum : column);
    return true;
}

bytes TerminalMarkdownView_getScrollX(const TerminalMarkdownView *self) {
    return self != nullptr ? TerminalMarkdownView_scrollX(self) : 0u;
}

bytes TerminalMarkdownView_rowScrollX(const TerminalMarkdownView *self, bytes row) {
    return rowScrollX(self, row);
}

bool TerminalMarkdownView_scrollXBy(TerminalMarkdownView *self, i64 delta) {
    if (self == nullptr)
        return false;
    bytes maximum = TerminalMarkdownView_maxScrollX(self);
    bytes wanted = TerminalMarkdownView_scrollX(self);
    if (delta < 0) {
        bytes amount = (bytes) -delta;
        wanted = amount > wanted ? 0u : wanted - amount;
    } else if (delta > 0) {
        wanted += (bytes) delta;
        if (wanted > maximum)
            wanted = maximum;
    }
    if (wanted == TerminalMarkdownView_scrollX(self))
        return false;
    TerminalMarkdownView_setScrollX(self, wanted);
    return true;
}

bytes TerminalMarkdownView_codeBlockCount(const TerminalMarkdownView *self) {
    return self != nullptr ? TerminalMarkdownCodeBlockPool_count(TerminalMarkdownView_codeBlocks(self)) : 0u;
}

TerminalMarkdownCodeBlock TerminalMarkdownView_codeBlockAt(const TerminalMarkdownView *self,
                                                           bytes row) {
    TerminalMarkdownCodeBlock none = {0};
    if (self == nullptr)
        return none;
    bytes index = 0;
    if (!TerminalMarkdownCodeBlockPool_indexAt(TerminalMarkdownView_codeBlocks(self), row, &index))
        return none;
    return TerminalMarkdownCodeBlockPool_at(TerminalMarkdownView_codeBlocks(self), index);
}

TerminalMarkdownCodeLine TerminalMarkdownView_codeBlockLine(const TerminalMarkdownView *self,
                                                            TerminalMarkdownCodeBlock block,
                                                            bytes line) {
    if (self == nullptr)
        return (TerminalMarkdownCodeLine) {0};
    return TerminalMarkdownCodeBlock_lineOf(TerminalMarkdownView_lines(self), TerminalMarkdownView_lineCount(self), block, line);
}

bool TerminalMarkdownView_isReady(const TerminalMarkdownView *self) {
    return self != nullptr && TerminalMarkdownView_ready(self);
}

bool TerminalMarkdownView_isTruncated(const TerminalMarkdownView *self) {
    return self != nullptr && TerminalMarkdownView_truncated(self);
}

bytes TerminalMarkdownView_exhaustionCount(const TerminalMarkdownView *self) {
    return self != nullptr ? TerminalMarkdownView_refusals(self) : 0u;
}

fn TerminalMarkdownView_clearExhaustion(TerminalMarkdownView *self) {
    if (self != nullptr)
        TerminalMarkdownView_setRefusals(self, 0);
}

bool TerminalMarkdownView_setStyle(TerminalMarkdownView *self, TerminalMarkdownStyle style) {
    if (self == nullptr) {
        THROW("TerminalMarkdownView style: null view");
        return false;
    }
    TerminalMarkdownView_setStyle(self, style);
    TerminalMarkdownView_setTheme(self, TerminalMarkdownTheme_fromStyle(style));
    return true;
}

TerminalMarkdownStyle TerminalMarkdownView_getStyle(const TerminalMarkdownView *self) {
    return self != nullptr ? TerminalMarkdownView_style(self) : (TerminalMarkdownStyle) {0};
}

bool TerminalMarkdownView_setTheme(TerminalMarkdownView *self, TerminalMarkdownTheme theme) {
    if (self == nullptr) {
        THROW("TerminalMarkdownView theme: null view");
        return false;
    }
    TerminalMarkdownView_setTheme(self, theme);
    TerminalMarkdownView_setStyle(self, TerminalMarkdownTheme_style(TerminalMarkdownView_themeMut(self)));
    return true;
}

TerminalMarkdownTheme TerminalMarkdownView_getTheme(const TerminalMarkdownView *self) {
    return self != nullptr ? TerminalMarkdownView_theme(self) : (TerminalMarkdownTheme) {0};
}

// Draw the copy affordance of one fence on its first row. It is painted AFTER
// the text so it owns its cells even when a code line runs to the right edge; the
// glyph is the theme's, and `copiedBlock` decides which of the two it is.
static fn renderCopyIcon(const TerminalMarkdownView *self, TerminalCanvas *canvas,
                           bytes screenRow, bytes width, bytes blockIndex,
                           bytes glyphCells) {
    const TerminalMarkdownTheme *theme = TerminalMarkdownView_themeMut(self);
    bool copied = TerminalMarkdownView_copiedBlock(self) == blockIndex;
    const char *glyph = copied ? TerminalMarkdownTheme_copiedGlyph(theme) : TerminalMarkdownTheme_copyGlyph(theme);
    if (glyph == nullptr)
        return;
    if (width < glyphCells + 1u)
        return;
    bytes column = TerminalMarkdownAction_iconColumn(width, glyphCells);
    TerminalPen blockPen = TerminalMarkdownTheme_pen(TerminalMarkdownView_themeMut(self),
                                                    TERMINAL_MARKDOWN_FENCE, 0, false);
    TerminalPen pen = copied ? TerminalMarkdownTheme_copied(theme) : TerminalMarkdownTheme_copy(theme);
    pen.bg = blockPen.bg;
    TerminalCanvas_text(canvas, column, screenRow, glyph, strlen(glyph), pen);
}

fn TerminalMarkdownView_render(const TerminalMarkdownView *self, TerminalCanvas *canvas) {
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    if (!TerminalMarkdownView_ready(self))
        return;
    const TerminalMarkdownTheme *theme = TerminalMarkdownView_themeMut(self);
    const char *copyGlyph = TerminalMarkdownTheme_copyGlyph(theme);
    bytes visible = TerminalCanvas_getHeight(canvas);
    bytes width = TerminalCanvas_getWidth(canvas);
    bytes glyphCells = cellsOf(copyGlyph, copyGlyph != nullptr ? strlen(copyGlyph) : 0u);
    if (glyphCells == 0)
        glyphCells = 1u;
    for (bytes row = 0; row < visible && TerminalMarkdownView_scroll(self) + row < TerminalMarkdownView_rowCount(self); ++row) {
        bytes documentRow = TerminalMarkdownView_scroll(self) + row;
        const MarkdownRow *record = &TerminalMarkdownView_rowsMut(self)[documentRow];
        // Horizontal pan is the table's own. Every other row paints at its
        // document origin, so prose stays fixed while a warped table slides.
        bytes scrollX = rowScrollX(self, documentRow);
        bytes blockIndex = MARKDOWN_NO_INDEX;
        bool firstBlockRow = false;
        if (MarkdownRow_codeBlock(record)
            && TerminalMarkdownCodeBlockPool_indexAt(TerminalMarkdownView_codeBlocks(self), documentRow,
                                                     &blockIndex)) {
            // The wash is painted UNDER the text so code glyph cells keep their own
            // foreground while every cell of the row carries the block background.
            TerminalPen block = TerminalMarkdownTheme_pen(TerminalMarkdownView_themeMut(self),
                                                          TERMINAL_MARKDOWN_FENCE, 0, false);
            block.style = 0;
            block.fg = TERMINAL_COLOR_DEFAULT;
            TerminalCanvas_hline(canvas, row, ' ', block);
            TerminalMarkdownCodeBlock recordBlock =
                TerminalMarkdownCodeBlockPool_at(TerminalMarkdownView_codeBlocks(self), blockIndex);
            firstBlockRow = recordBlock.firstRow == documentRow;
        }
        bytes x = 0;
        for (bytes index = 0; index < MarkdownRow_segmentCount(record); ++index) {
            const MarkdownSegment *segment = &TerminalMarkdownView_segmentsMut(self)[MarkdownRow_firstSegment(record) + index];
            bytes advance = cellsOf(MarkdownSegment_text(segment), MarkdownSegment_length(segment));
            bytes totalSpan = advance * MarkdownSegment_repeat(segment);
            bool hovered = MarkdownSegment_link(segment) != nullptr
                        && TerminalMarkdownView_hoverRow(self) == documentRow
                        && TerminalMarkdownView_hoverColumn(self) >= x
                        && (TerminalMarkdownView_hoverColumn(self) < x + totalSpan
                            || (TerminalMarkdownView_hoverColumn(self) == x && totalSpan == 0));
            TerminalPen pen = TerminalMarkdownTheme_pen(TerminalMarkdownView_themeMut(self), MarkdownSegment_role(segment),
                                                        MarkdownSegment_variant(segment), hovered);
            pen.style = (u16) (pen.style | MarkdownSegment_style(segment));
            for (bytes repeat = 0; repeat < MarkdownSegment_repeat(segment); ++repeat) {
                if (x >= scrollX + width) {
                    x += advance;
                    break;
                }
                if (x + advance <= scrollX) {
                    x += advance;
                    continue;
                }
                if (x >= scrollX && x + advance <= scrollX + width) {
                    TerminalCanvas_text(canvas, x - scrollX, row, MarkdownSegment_text(segment), MarkdownSegment_length(segment), pen);
                    x += advance;
                    continue;
                }
                bytes at = 0;
                while (at < MarkdownSegment_length(segment)) {
                    TerminalCanvasDecodedScalar decoded =
                        TerminalCanvas_utf8Decode(MarkdownSegment_text(segment) + at, MarkdownSegment_length(segment) - at);
                    bytes consumed = decoded.bytes == 0 ? 1u : decoded.bytes;
                    bytes cellW = decoded.bytes == 0 ? 1u : TerminalCanvas_widthOf(decoded.codepoint);
                    if (x >= scrollX && x + cellW <= scrollX + width) {
                        TerminalCanvas_text(canvas, x - scrollX, row, MarkdownSegment_text(segment) + at, consumed, pen);
                    }
                    x += cellW;
                    at += consumed;
                }
                if (advance == 0)
                    break;
            }
            if (x >= scrollX + width)
                break;
        }
        if (firstBlockRow && blockIndex != MARKDOWN_NO_INDEX)
            renderCopyIcon(self, canvas, row, width, blockIndex, glyphCells);
    }
}

// What a press on this DOCUMENT cell means. The view reports it; it never opens
// the link and never touches the clipboard — that is the caller's tier.
TerminalMarkdownAction TerminalMarkdownView_actionAt(const TerminalMarkdownView *self,
                                                     bytes row, bytes column) {
    TerminalMarkdownAction none = {0};
    if (self == nullptr || !TerminalMarkdownView_ready(self) || row >= TerminalMarkdownView_rowCount(self))
        return none;
    bytes blockIndex = MARKDOWN_NO_INDEX;
    if (TerminalMarkdownCodeBlockPool_indexAt(TerminalMarkdownView_codeBlocks(self), row, &blockIndex)) {
        TerminalMarkdownCodeBlock block =
            TerminalMarkdownCodeBlockPool_at(TerminalMarkdownView_codeBlocks(self), blockIndex);
        if (block.firstRow == row) {
            const TerminalMarkdownTheme *theme = TerminalMarkdownView_themeMut(self);
            const char *copyGlyph = TerminalMarkdownTheme_copyGlyph(theme);
            bytes glyphCells = cellsOf(copyGlyph,
                                        copyGlyph != nullptr ? strlen(copyGlyph) : 0u);
            if (glyphCells == 0)
                glyphCells = 1u;
            bytes screenCol = column >= rowScrollX(self, row) ? column - rowScrollX(self, row) : column;
            if (TerminalMarkdownAction_iconHit(TerminalMarkdownView_width(self), screenCol, glyphCells))
                return (TerminalMarkdownAction) { TERMINAL_MARKDOWN_ACTION_COPY_CODE,
                                                  nullptr, 0, blockIndex };
        }
    }
    const MarkdownRow *record = &TerminalMarkdownView_rowsMut(self)[row];
    bytes x = 0;
    for (bytes index = 0; index < MarkdownRow_segmentCount(record); ++index) {
        const MarkdownSegment *segment = &TerminalMarkdownView_segmentsMut(self)[MarkdownRow_firstSegment(record) + index];
        bytes advance = cellsOf(MarkdownSegment_text(segment), MarkdownSegment_length(segment)) * MarkdownSegment_repeat(segment);
        if (MarkdownSegment_link(segment) != nullptr && column >= x && column < x + advance)
            return (TerminalMarkdownAction) { TERMINAL_MARKDOWN_ACTION_LINK, MarkdownSegment_link(segment),
                                              MarkdownSegment_linkLength(segment), 0 };
        if (advance > SIZE_MAX - x)
            break;
        x += advance;
    }
    return none;
}

bool TerminalMarkdownView_setHover(TerminalMarkdownView *self, bytes row, bytes column) {
    if (self == nullptr)
        return false;
    TerminalMarkdownView_setHoverRow(self, row);
    TerminalMarkdownView_setHoverColumn(self, column);
    return TerminalMarkdownView_actionAt(self, row, column).kind
           != TERMINAL_MARKDOWN_ACTION_NONE;
}

fn TerminalMarkdownView_clearHover(TerminalMarkdownView *self) {
    if (self == nullptr)
        return;
    TerminalMarkdownView_setHoverRow(self, MARKDOWN_NO_INDEX);
    TerminalMarkdownView_setHoverColumn(self, MARKDOWN_NO_INDEX);
}

bool TerminalMarkdownView_copyBlock(const TerminalMarkdownView *self, bytes block,
                                    char *dest, bytes capacity, bytes *written) {
    if (self == nullptr)
        return false;
    return TerminalMarkdownCodeBlockPool_copy(TerminalMarkdownView_codeBlocks(self), TerminalMarkdownView_lines(self),
                                              TerminalMarkdownView_lineCount(self), block, dest, capacity, written);
}

bool TerminalMarkdownView_setCopied(TerminalMarkdownView *self, bytes block, bool copied) {
    if (self == nullptr)
        return false;
    if (!copied) {
        if (block == MARKDOWN_NO_INDEX || TerminalMarkdownView_copiedBlock(self) == block)
            TerminalMarkdownView_setCopiedBlock(self, MARKDOWN_NO_INDEX);
        return true;
    }
    if (block >= TerminalMarkdownCodeBlockPool_count(TerminalMarkdownView_codeBlocks(self)))
        return false;
    TerminalMarkdownView_setCopiedBlock(self, block);
    return true;
}

bool TerminalMarkdownView_isCopied(const TerminalMarkdownView *self, bytes block) {
    if (self == nullptr || block == MARKDOWN_NO_INDEX)
        return false;
    return TerminalMarkdownView_copiedBlock(self) == block
        && block < TerminalMarkdownCodeBlockPool_count(TerminalMarkdownView_codeBlocks(self));
}

TerminalProjection TerminalMarkdownView_toString(const TerminalMarkdownView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalMarkdownView{rows=%zu,width=%zu,contentWidth=%zu,scroll=%zu,scrollX=%zu,ready=%d,truncated=%d}",
        TerminalMarkdownView_rowCount(self), TerminalMarkdownView_width(self), TerminalMarkdownView_contentWidth(self), TerminalMarkdownView_scroll(self), TerminalMarkdownView_scrollX(self), TerminalMarkdownView_ready(self), TerminalMarkdownView_truncated(self));
}

TerminalProjection TerminalMarkdownView_toStringStruct(const TerminalMarkdownView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalMarkdownView{lines=<borrowed:%zu>,lineCount=%zu,dirty=%d,ready=%d,"
        "truncated=%d,failed=%d,width=%zu,contentWidth=%zu,visibleRows=%zu,scroll=%zu,scrollX=%zu,rows=<owned:%zu>,"
        "rowCount=%zu,rowCapacity=%zu,segments=<owned:%zu>,segmentCount=%zu,"
        "segmentCapacity=%zu,codeBlocks=<owned:%zu>,theme=<pens>,hoverRow=%zu,"
        "hoverColumn=%zu,copiedBlock=%zu,refusals=%zu}",
        TerminalMarkdownView_lineCount(self), TerminalMarkdownView_lineCount(self), TerminalMarkdownView_dirty(self), TerminalMarkdownView_ready(self),
        TerminalMarkdownView_truncated(self), TerminalMarkdownView_failed(self), TerminalMarkdownView_width(self), TerminalMarkdownView_contentWidth(self), TerminalMarkdownView_visibleRows(self),
        TerminalMarkdownView_scroll(self), TerminalMarkdownView_scrollX(self), TerminalMarkdownView_rowCapacity(self), TerminalMarkdownView_rowCount(self), TerminalMarkdownView_rowCapacity(self),
        TerminalMarkdownView_segmentCapacity(self), TerminalMarkdownView_segmentCount(self), TerminalMarkdownView_segmentCapacity(self),
        TerminalMarkdownCodeBlockPool_count(TerminalMarkdownView_codeBlocks(self)),
        TerminalMarkdownView_hoverRow(self), TerminalMarkdownView_hoverColumn(self), TerminalMarkdownView_copiedBlock(self), TerminalMarkdownView_refusals(self));
}
