#include "vex.h"
#include "ui/tree/terminal_tree_view.h"
#include "ui/surface/terminal_canvas.h"
#include "nio/mem.h"
#include "oop/oop.h"
#include <string.h>

;;DEFINITION
/** The scrollable, collapsible tree, and the reason it is separate from anything that
 * draws: a list, a file browser and a settings menu are the same widget, and the only
 * thing that differs is what a caller puts in a row.
 *
 * The storage is a PRE-ORDER array, which is what makes folding cheap. A node's whole
 * subtree is the contiguous range starting at its own index and running for
 * subtreeCount, so folding is "skip subtreeCount" and unfolding is "step forward one"
 * — there is no walk, no recursion, and no per-node child list to keep in step. Adding
 * a child inserts at the END of its parent's subtree and grows subtreeCount up the
 * parent chain, which preserves that layout in one pass.
 *
 * The visible rows are CACHED. A caller painting a frame asks for row n and gets a
 * record back — it never walks the tree, and it never has to know which rows a folded
 * parent is hiding. That cache is recomputed on every structural change and never
 * during a read, which is the same split the surface keeps between a paint and a
 * presentation.
 *
 * Scrolling is clamped HERE rather than by the caller. A viewport offset past the end
 * is the classic bug in this widget — it renders an empty pane and looks like data
 * loss — so every route into the offset goes through one clamp, and a shrink of the
 * tree (a directory collapsing under the viewport, a refresh) clamps it again.
 *
 * Nothing here is thread-safe and nothing claims to be: the caller serializes, exactly
 * as every other component in this toolkit does. */

;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalTreeView (ui/tree/terminal_tree_view.c)
 * STRUCT FIELDS (Mirroring ui/tree/terminal_tree_view.h)
 * TerminalTreeNode *nodes    owned pre-order node array
 * bytes nodeCount           live nodes
 * bytes nodeCapacity        admitted node slots
 * u32 *rows             owned visible-row array (node indices)
 * bytes visibleCount        rows currently visible
 * bytes rowsCapacity        admitted row slots
 * TerminalTreeNodeId nextId  identity source, never rewound
 * TerminalTreeNodeId selected  NO_NODE when nothing is selected
 * bytes scrollRow           first visible row
 * bytes viewportRows        rows the caller can show
 * bytes nodeRefusals        node-growth refusals in this epoch
 * bytes rowRefusals         row-growth refusals in this epoch
 * SLOT RECORD: TerminalTreeNode
 * TerminalTreeNodeId id      stable identity, never reused
 * i32 parent             parent's index, or NO_PARENT
 * bytes subtreeCount        nodes in this subtree, including this one
 * bytes depth               0 at the top level
 * bool folder                has foldable children
 * bool expanded              unfolded
 * const char *label          borrowed
 * SLOT RECORD: TerminalTreeRow
 * TerminalTreeNodeId id      the node this row shows
 * const char *label          borrowed label
 * bytes depth               nesting depth
 * bool folder/expanded/selected  what the caller paints with
 * PRIVATE HELPERS
 * TERMINAL_TREE_NO_INDEX      sentinel index meaning "no such node"
 * TERMINAL_TREE_NO_PARENT     parent index of a top-level node
 * refuseNode/refuseRow(self, reason)  count a refusal, reporting once per epoch
 * growNodes/growRows(self)    cold doubling growth preserving every live entry
 * indexFor(self, id)          id -> node index, or NO_INDEX (linear, cold)
 * appendRow(self, index)      one entry into the cached visible-row list
 * rebuild(self)               recompute the visible rows from the fold state
 * clampScroll(self)           hold the viewport inside the current tree
 * FUNCTION REGISTRY (exports)
 * Constructor: TerminalTreeView *TerminalTreeView_0() [zero/arity chooser]
 * Lifecycle: fn TerminalTreeView_free(self)
 * Structure: TerminalTreeNodeId addChild(self, parent, label, folder)
 *   bool setLabel(self, id, label) / remove(self, id) / clear(self)
 * Folding: bool setExpanded(self, id, expanded) / toggle(self, id) / reveal(self, id)
 *   bool isExpanded/isFolder(self, id)
 * Selection: bool setSelected(self, id) / isSelected(self, id)
 *   TerminalTreeNodeId getSelected(self)
 * Viewport: bytes visibleCount/viewportRows/scrollRow/maxScrollRow(self)
 *   bool setViewportRows(self, rows)
 *   bytes setScrollRow(self, row) / scrollBy(self, rows)
 * Rows: TerminalTreeRow TerminalTreeView_rowAt(self, row)
 *   TerminalTreeNodeId nodeAtOffset(self, offset)
 *   bytes rowOf(self, id)
 * Queries: bool isNode(self, id)
 *   const char *getLabel(self, id)
 *   bytes depthOf/childCountOf/subtreeCountOf/nodeCount/capacity/exhaustionCount(self)
 *   fn clearExhaustion(self)
 * Projections: TerminalProjection TerminalTreeView_toString/toStringStruct(self)
 * CONTRACT: every operation validates its id and refuses an unknown one by returning
 * the safe default (false / NO_NODE / NO_ROW / an empty row) WITHOUT mutating anything.
 * A node id is never reused, so a stale id stops being found rather than addressing a
 * neighbour. Labels are borrowed and are never copied or freed. Visible-row and scroll
 * queries are O(1) and allocation-free; structure changes are cold and may grow storage,
 * and a refused growth is counted and reported once per epoch, naming the resource.
 * ======================================================================== */

#define TERMINAL_TREE_NO_INDEX ((bytes) -1)
#define TERMINAL_TREE_NO_PARENT ((i32) -1)

static fn refuseNode(TerminalTreeView *self, const char *reason) {
    if ((*self).nodeRefusals == 0)
        THROW("TerminalTreeView exhausted: %s (node capacity %zu, live %zu)",
              reason, (*self).nodeCapacity, (*self).nodeCount);
    if ((*self).nodeRefusals != SIZE_MAX)
        ++(*self).nodeRefusals;
}

static fn refuseRow(TerminalTreeView *self, const char *reason) {
    if ((*self).rowRefusals == 0)
        THROW("TerminalTreeView exhausted: %s (row capacity %zu, live %zu)",
              reason, (*self).rowsCapacity, (*self).visibleCount);
    if ((*self).rowRefusals != SIZE_MAX)
        ++(*self).rowRefusals;
}

static bool growNodes(TerminalTreeView *self) {
    bytes next = (*self).nodeCapacity == 0 ? TERMINAL_TREE_CHILDREN_DEFAULT
                                            : (*self).nodeCapacity * 2;
    if (next < (*self).nodeCapacity || next > SIZE_MAX / sizeof(TerminalTreeNode)) {
        refuseNode(self, "node capacity overflow");
        return false;
    }
    TerminalTreeNode *grown = Memory_alloc(TYPE_BYTE_ARRAY, next * sizeof(TerminalTreeNode));
    if (grown == nullptr) {
        refuseNode(self, "node array allocation refused");
        return false;
    }
    if ((*self).nodeCapacity != 0)
        memcpy(grown, (*self).nodes, (*self).nodeCapacity * sizeof(TerminalTreeNode));
    memset(grown + (*self).nodeCapacity, 0, (next - (*self).nodeCapacity) * sizeof(TerminalTreeNode));
    Memory_free((*self).nodes);
    (*self).nodes = grown;
    (*self).nodeCapacity = next;
    return true;
}

static bool growRows(TerminalTreeView *self) {
    bytes next = (*self).rowsCapacity == 0 ? TERMINAL_TREE_CHILDREN_DEFAULT
                                            : (*self).rowsCapacity * 2;
    if (next < (*self).rowsCapacity || next > SIZE_MAX / sizeof(u32)) {
        refuseRow(self, "row capacity overflow");
        return false;
    }
    u32 *grown = Memory_alloc(TYPE_BYTE_ARRAY, next * sizeof(u32));
    if (grown == nullptr) {
        refuseRow(self, "row array allocation refused");
        return false;
    }
    if ((*self).rowsCapacity != 0)
        memcpy(grown, (*self).rows, (*self).rowsCapacity * sizeof(u32));
    Memory_free((*self).rows);
    (*self).rows = grown;
    (*self).rowsCapacity = next;
    return true;
}

// Identity is never positional: a node keeps its id when its neighbours shift, which
// they do on every insertion because the array is pre-order. Cold, and deliberately a
// scan — the alternative is a second index to keep in step with this one.
static bytes indexFor(const TerminalTreeView *self, TerminalTreeNodeId id) {
    if (self == nullptr || id == TERMINAL_TREE_NO_NODE)
        return TERMINAL_TREE_NO_INDEX;
    for (bytes index = 0; index < (*self).nodeCount; ++index)
        if ((*self).nodes[index].id == id)
            return index;
    return TERMINAL_TREE_NO_INDEX;
}

static bool appendRow(TerminalTreeView *self, bytes index) {
    if ((*self).visibleCount == (*self).rowsCapacity && !growRows(self))
        return false;
    (*self).rows[(*self).visibleCount] = (u32) index;
    ++(*self).visibleCount;
    return true;
}

// Recompute the rows that are actually shown. The pre-order layout is the whole trick: a
// folded node's subtree is one contiguous range, so folding skips it in a single step and
// unfolding descends into it one node at a time.
static fn rebuild(TerminalTreeView *self) {
    (*self).visibleCount = 0;
    bytes at = 0;
    while (at < (*self).nodeCount) {
        if (!appendRow(self, at))
            break;                       // reported and counted; keep what we resolved
        const TerminalTreeNode *node = &(*self).nodes[at];
        if ((*node).folder && (*node).expanded)
            at += 1;
        else
            at += (*node).subtreeCount;
    }
    bytes maximum = TerminalTreeView_maxScrollRow(self);
    if ((*self).scrollRow > maximum)
        (*self).scrollRow = maximum;
}

TerminalTreeView *TerminalTreeView_0() {
    TerminalTreeView *self = Memory_alloc(TYPE_BYTE_ARRAY, sizeof(TerminalTreeView));
    if (self == nullptr)
        return nullptr;
    *self = (TerminalTreeView) {
        .nextId = 1u,                    // zero is NO_NODE, so ids start at one
        .selected = TERMINAL_TREE_NO_NODE,
        .viewportRows = TERMINAL_TREE_VIEWPORT_DEFAULT,
        .indentSpaces = 2u,
        .nerdIcons = true,
    };
    return self;
}

fn TerminalTreeView_free(TerminalTreeView *self) {
    if (self == nullptr)
        return;
    Memory_free((*self).nodes);
    Memory_free((*self).rows);
    Memory_free(self);
}

TerminalTreeNodeId TerminalTreeView_addChild(TerminalTreeView *self, TerminalTreeNodeId parent,
                                             const char *label, bool folder) {
    if (self == nullptr || label == nullptr)
        return TERMINAL_TREE_NO_NODE;
    bytes parentIndex = TERMINAL_TREE_NO_INDEX;
    if (parent != TERMINAL_TREE_NO_NODE) {
        parentIndex = indexFor(self, parent);
        if (parentIndex == TERMINAL_TREE_NO_INDEX)
            return TERMINAL_TREE_NO_NODE;   // an unknown parent is refused, never guessed
    }
    if ((*self).nodeCount == (*self).nodeCapacity && !growNodes(self))
        return TERMINAL_TREE_NO_NODE;
    // A new node lands at the END of its parent's subtree. Everything after that point
    // shifts up by one, and every ancestor's subtreeCount grows to swallow it — which is
    // what keeps the array in pre-order without a single child list.
    bytes at = parentIndex == TERMINAL_TREE_NO_INDEX
                    ? (*self).nodeCount
                    : parentIndex + (*self).nodes[parentIndex].subtreeCount;
    if (at < (*self).nodeCount)
        memmove(&(*self).nodes[at + 1], &(*self).nodes[at],
                ((*self).nodeCount - at) * sizeof(TerminalTreeNode));
    TerminalTreeNode *node = &(*self).nodes[at];
    *node = (TerminalTreeNode) {
        .id = (*self).nextId,
        .parent = parentIndex == TERMINAL_TREE_NO_INDEX ? TERMINAL_TREE_NO_PARENT
                                                        : (i32) parentIndex,
        .subtreeCount = 1,
        .depth = parentIndex == TERMINAL_TREE_NO_INDEX
                     ? 0
                     : (*self).nodes[parentIndex].depth + 1,
        .folder = folder,
        .expanded = false,
        .label = label,
    };
    ++(*self).nodeCount;
    ++(*self).nextId;
    for (bytes walk = parentIndex; walk != TERMINAL_TREE_NO_INDEX; ) {
        ++(*self).nodes[walk].subtreeCount;
        walk = (*self).nodes[walk].parent == TERMINAL_TREE_NO_PARENT
                   ? TERMINAL_TREE_NO_INDEX
                   : (bytes) (*self).nodes[walk].parent;
    }
    // A node that gains its first child is a folder, and it opens. A caller that just
    // added a child plainly wants it seen, and the alternative — rows that exist in the
    // array and are invisible because a flag elsewhere says "leaf" — is a silent trap
    // rather than a design.
    if (parentIndex != TERMINAL_TREE_NO_INDEX && !(*self).nodes[parentIndex].folder) {
        (*self).nodes[parentIndex].folder = true;
        (*self).nodes[parentIndex].expanded = true;
    }
    rebuild(self);
    return (*self).nodes[at].id;
}

bool TerminalTreeView_setLabel(TerminalTreeView *self, TerminalTreeNodeId id, const char *label) {
    if (label == nullptr)
        return false;
    bytes index = indexFor(self, id);
    if (index == TERMINAL_TREE_NO_INDEX)
        return false;
    (*self).nodes[index].label = label;
    return true;
}

bool TerminalTreeView_remove(TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    if (index == TERMINAL_TREE_NO_INDEX)
        return false;
    const bytes removed = (*self).nodes[index].subtreeCount;
    const i32 parent = (*self).nodes[index].parent;
    // A selection inside the subtree is cleared rather than silently re-pointed: the
    // selected thing went away, and ids are never reused, so there is nothing to fall
    // back to that would be honest.
    for (bytes at = index; at < index + removed; ++at) {
        if ((*self).nodes[at].id == (*self).selected) {
            (*self).selected = TERMINAL_TREE_NO_NODE;
            break;
        }
    }
    for (bytes walk = parent == TERMINAL_TREE_NO_PARENT ? TERMINAL_TREE_NO_INDEX : (bytes) parent;
         walk != TERMINAL_TREE_NO_INDEX; ) {
        (*self).nodes[walk].subtreeCount -= removed;
        walk = (*self).nodes[walk].parent == TERMINAL_TREE_NO_PARENT
                   ? TERMINAL_TREE_NO_INDEX
                   : (bytes) (*self).nodes[walk].parent;
    }
    if (index + removed < (*self).nodeCount)
        memmove(&(*self).nodes[index], &(*self).nodes[index + removed],
                ((*self).nodeCount - index - removed) * sizeof(TerminalTreeNode));
    (*self).nodeCount -= removed;
    rebuild(self);
    return true;
}

fn TerminalTreeView_clear(TerminalTreeView *self) {
    if (self == nullptr)
        return;
    (*self).nodeCount = 0;
    (*self).selected = TERMINAL_TREE_NO_NODE;
    (*self).scrollRow = 0;
    (*self).visibleCount = 0;
    // nextId is deliberately NOT rewound. A refresh hands out new ids, so a caller still
    // holding one from before finds nothing instead of somebody else's row.
}

bool TerminalTreeView_setExpanded(TerminalTreeView *self, TerminalTreeNodeId id, bool expanded) {
    bytes index = indexFor(self, id);
    if (index == TERMINAL_TREE_NO_INDEX)
        return false;
    if ((*self).nodes[index].expanded == expanded)
        return true;
    (*self).nodes[index].expanded = expanded;
    rebuild(self);
    return true;
}

bool TerminalTreeView_toggle(TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    if (index == TERMINAL_TREE_NO_INDEX)
        return false;
    (*self).nodes[index].expanded = !(*self).nodes[index].expanded;
    rebuild(self);
    return (*self).nodes[index].expanded;
}

bool TerminalTreeView_isExpanded(const TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    return index != TERMINAL_TREE_NO_INDEX && (*self).nodes[index].expanded;
}

bool TerminalTreeView_isFolder(const TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    return index != TERMINAL_TREE_NO_INDEX && (*self).nodes[index].folder;
}

bool TerminalTreeView_reveal(TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    if (index == TERMINAL_TREE_NO_INDEX)
        return false;
    bytes walk = index;
    while (walk != TERMINAL_TREE_NO_INDEX) {
        (*self).nodes[walk].expanded = true;
        walk = (*self).nodes[walk].parent == TERMINAL_TREE_NO_PARENT
                   ? TERMINAL_TREE_NO_INDEX
                   : (bytes) (*self).nodes[walk].parent;
    }
    rebuild(self);
    return true;
}

bool TerminalTreeView_setSelected(TerminalTreeView *self, TerminalTreeNodeId id) {
    if (self == nullptr)
        return false;
    if (id != TERMINAL_TREE_NO_NODE && indexFor(self, id) == TERMINAL_TREE_NO_INDEX)
        return false;
    (*self).selected = id;
    return true;
}

TerminalTreeNodeId TerminalTreeView_getSelected(const TerminalTreeView *self) {
    return self == nullptr ? TERMINAL_TREE_NO_NODE : (*self).selected;
}

bool TerminalTreeView_isSelected(const TerminalTreeView *self, TerminalTreeNodeId id) {
    return self != nullptr && id != TERMINAL_TREE_NO_NODE && (*self).selected == id;
}

bytes TerminalTreeView_visibleCount(const TerminalTreeView *self) {
    return self == nullptr ? 0 : (*self).visibleCount;
}

bool TerminalTreeView_setViewportRows(TerminalTreeView *self, bytes rows) {
    if (self == nullptr)
        return false;
    (*self).viewportRows = rows;
    bytes maximum = TerminalTreeView_maxScrollRow(self);
    if ((*self).scrollRow > maximum)
        (*self).scrollRow = maximum;
    return true;
}

bytes TerminalTreeView_viewportRows(const TerminalTreeView *self) {
    return self == nullptr ? 0 : (*self).viewportRows;
}

bytes TerminalTreeView_scrollRow(const TerminalTreeView *self) {
    return self == nullptr ? 0 : (*self).scrollRow;
}

bytes TerminalTreeView_maxScrollRow(const TerminalTreeView *self) {
    if (self == nullptr || (*self).visibleCount <= (*self).viewportRows)
        return 0;
    return (*self).visibleCount - (*self).viewportRows;
}

bytes TerminalTreeView_setScrollRow(TerminalTreeView *self, bytes row) {
    if (self == nullptr)
        return 0;
    bytes maximum = TerminalTreeView_maxScrollRow(self);
    (*self).scrollRow = row > maximum ? maximum : row;
    return (*self).scrollRow;
}

bytes TerminalTreeView_scrollBy(TerminalTreeView *self, long rows) {
    if (self == nullptr)
        return 0;
    long wanted = (long) (*self).scrollRow + rows;
    if (wanted < 0)
        wanted = 0;
    return TerminalTreeView_setScrollRow(self, (bytes) wanted);
}

TerminalTreeRow TerminalTreeView_rowAt(const TerminalTreeView *self, bytes row) {
    TerminalTreeRow empty = {0};
    if (self == nullptr || row >= (*self).visibleCount)
        return empty;
    const TerminalTreeNode *node = &(*self).nodes[(*self).rows[row]];
    TerminalTreeRow record = {
        .id = (*node).id,
        .label = (*node).label,
        .depth = (*node).depth,
        .folder = (*node).folder,
        .expanded = (*node).expanded,
        .selected = (*node).id == (*self).selected,
    };
    return record;
}

TerminalTreeNodeId TerminalTreeView_nodeAtOffset(const TerminalTreeView *self, bytes offset) {
    if (self == nullptr || offset >= (*self).viewportRows)
        return TERMINAL_TREE_NO_NODE;
    bytes row = (*self).scrollRow + offset;
    if (row >= (*self).visibleCount)
        return TERMINAL_TREE_NO_NODE;
    return (*self).nodes[(*self).rows[row]].id;
}

bytes TerminalTreeView_rowOf(const TerminalTreeView *self, TerminalTreeNodeId id) {
    if (self == nullptr || id == TERMINAL_TREE_NO_NODE)
        return TERMINAL_TREE_NO_ROW;
    for (bytes row = 0; row < (*self).visibleCount; ++row)
        if ((*self).nodes[(*self).rows[row]].id == id)
            return row;
    return TERMINAL_TREE_NO_ROW;
}

bool TerminalTreeView_isNode(const TerminalTreeView *self, TerminalTreeNodeId id) {
    return indexFor(self, id) != TERMINAL_TREE_NO_INDEX;
}

const char *TerminalTreeView_getLabel(const TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    return index == TERMINAL_TREE_NO_INDEX ? nullptr : (*self).nodes[index].label;
}

bytes TerminalTreeView_depthOf(const TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    return index == TERMINAL_TREE_NO_INDEX ? 0 : (*self).nodes[index].depth;
}

bytes TerminalTreeView_childCountOf(const TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    if (index == TERMINAL_TREE_NO_INDEX)
        return 0;
    // Direct children only, and they are not adjacent in a pre-order array — a child's
    // own subtree sits between it and its next sibling — so this walks the node's own
    // range rather than reading two numbers.
    bytes count = 0;
    bytes end = index + (*self).nodes[index].subtreeCount;
    for (bytes at = index + 1; at < end; ++at)
        if ((*self).nodes[at].parent == (i32) index)
            ++count;
    return count;
}

bytes TerminalTreeView_subtreeCountOf(const TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    return index == TERMINAL_TREE_NO_INDEX ? 0 : (*self).nodes[index].subtreeCount;
}

bytes TerminalTreeView_nodeCount(const TerminalTreeView *self) {
    return self == nullptr ? 0 : (*self).nodeCount;
}

bytes TerminalTreeView_capacity(const TerminalTreeView *self) {
    return self == nullptr ? 0 : (*self).nodeCapacity;
}

// Both resources are counted, because "the tree refused" is only half an answer: the
// caller needs to know whether it ran out of NODES or of ROWS, and the diagnostic above
// names which. This total is what a test asserts against.
bytes TerminalTreeView_exhaustionCount(const TerminalTreeView *self) {
    if (self == nullptr)
        return 0;
    if (SIZE_MAX - (*self).nodeRefusals < (*self).rowRefusals)
        return SIZE_MAX;
    return (*self).nodeRefusals + (*self).rowRefusals;
}

fn TerminalTreeView_clearExhaustion(TerminalTreeView *self) {
    if (self == nullptr)
        return;
    (*self).nodeRefusals = 0;
    (*self).rowRefusals = 0;
}

bool TerminalTreeView_setIndent(TerminalTreeView *self, bytes indentSpaces) {
    if (self == nullptr)
        return false;
    (*self).indentSpaces = indentSpaces;
    return true;
}

bytes TerminalTreeView_getIndent(const TerminalTreeView *self) {
    return self != nullptr ? (*self).indentSpaces : 2u;
}

bool TerminalTreeView_setNerdIcons(TerminalTreeView *self, bool nerdIcons) {
    if (self == nullptr)
        return false;
    (*self).nerdIcons = nerdIcons;
    return true;
}

bool TerminalTreeView_getNerdIcons(const TerminalTreeView *self) {
    return self != nullptr ? (*self).nerdIcons : true;
}

fn TerminalTreeView_paint(const TerminalTreeView *self, TerminalCanvas *canvas) {
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    bytes visible = (*self).visibleCount;
    bytes scroll = (*self).scrollRow;
    bytes height = TerminalCanvas_getHeight(canvas);
    bytes width = TerminalCanvas_getWidth(canvas);
    bytes rowsToPaint = visible > scroll ? (visible - scroll) : 0u;
    if (rowsToPaint > height)
        rowsToPaint = height;

    for (bytes y = 0u; y < rowsToPaint; ++y) {
        TerminalTreeRow row = TerminalTreeView_rowAt(self, scroll + y);
        if (row.id == TERMINAL_TREE_NO_NODE)
            continue;
        bytes indent = row.depth * (*self).indentSpaces;
        if (indent >= width)
            continue;

        const char *icon = "";
        if (row.folder) {
            if ((*self).nerdIcons)
                icon = row.expanded ? "\xef\x84\x95 " : "\xef\x84\x94 ";
            else
                icon = row.expanded ? "[-] " : "[+] ";
        } else {
            if ((*self).nerdIcons)
                icon = "\xef\x85\x9b ";
            else
                icon = "    ";
        }

        TerminalPen pen = TerminalPen_0();
        if (row.selected)
            pen.style |= TERMINAL_STYLE_REVERSE;

        for (bytes i = 0u; i < indent; ++i)
            TerminalCanvas_text(canvas, i, y, " ", 1u, pen);

        bytes iconLen = strlen(icon);
        if (indent + 2u <= width)
            TerminalCanvas_text(canvas, indent, y, icon, iconLen, pen);

        if (row.label != nullptr) {
            bytes labelX = indent + ((*self).nerdIcons ? 3u : 4u);
            if (labelX < width)
                TerminalCanvas_text(canvas, labelX, y, row.label, strlen(row.label), pen);
        }
    }
}

TerminalProjection TerminalTreeView_toString(const TerminalTreeView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalTreeView(%zu node%s, %zu visible, scroll %zu/%zu, viewport %zu)",
        (*self).nodeCount, (*self).nodeCount == 1u ? "" : "s", (*self).visibleCount,
        (*self).scrollRow, TerminalTreeView_maxScrollRow(self), (*self).viewportRows);
}

TerminalProjection TerminalTreeView_toStringStruct(const TerminalTreeView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalTreeView { nodes=%p nodeCount=%zu nodeCapacity=%zu rows=%p visibleCount=%zu "
        "rowsCapacity=%zu nextId=%u selected=%u scrollRow=%zu viewportRows=%zu "
        "nodeRefusals=%zu rowRefusals=%zu indentSpaces=%zu nerdIcons=%d }",
        (const_any) (*self).nodes, (*self).nodeCount, (*self).nodeCapacity,
        (const_any) (*self).rows, (*self).visibleCount, (*self).rowsCapacity,
        (unsigned) (*self).nextId, (unsigned) (*self).selected, (*self).scrollRow,
        (*self).viewportRows, (*self).nodeRefusals, (*self).rowRefusals,
        (*self).indentSpaces, (*self).nerdIcons);
}
