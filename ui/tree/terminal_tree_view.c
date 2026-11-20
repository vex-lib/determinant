#include "vex.h"
#include "ui/tree/terminal_tree_view.h"
#include "ui/surface/terminal_canvas.h"
#include "nio/mem.h"
#include "oop/oop.h"
#include <string.h>
#include "core/determinant_memory.h"

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


;;GETTER
static inline bytes TerminalTreeNode_depth(const TerminalTreeNode *s) { return (*s).depth; }
;;GETTER
static inline bytes *TerminalTreeNode_depthMut(TerminalTreeNode *s) { return &(*s).depth; }
;;SETTER
static inline fn TerminalTreeNode_setDepth(TerminalTreeNode *s, bytes v) { (*s).depth = v; }
;;GETTER
static inline bool TerminalTreeNode_expanded(const TerminalTreeNode *s) { return (*s).expanded; }
;;GETTER
static inline bool *TerminalTreeNode_expandedMut(TerminalTreeNode *s) { return &(*s).expanded; }
;;SETTER
static inline fn TerminalTreeNode_setExpanded(TerminalTreeNode *s, bool v) { (*s).expanded = v; }
;;GETTER
static inline bool TerminalTreeNode_folder(const TerminalTreeNode *s) { return (*s).folder; }
;;GETTER
static inline bool *TerminalTreeNode_folderMut(TerminalTreeNode *s) { return &(*s).folder; }
;;SETTER
static inline fn TerminalTreeNode_setFolder(TerminalTreeNode *s, bool v) { (*s).folder = v; }
;;GETTER
static inline TerminalTreeNodeId TerminalTreeNode_id(const TerminalTreeNode *s) { return (*s).id; }
;;GETTER
static inline TerminalTreeNodeId *TerminalTreeNode_idMut(TerminalTreeNode *s) { return &(*s).id; }
;;SETTER
static inline fn TerminalTreeNode_setId(TerminalTreeNode *s, TerminalTreeNodeId v) { (*s).id = v; }
;;GETTER
static inline const char * TerminalTreeNode_label(const TerminalTreeNode *s) { return (*s).label; }
;;GETTER
static inline const char * *TerminalTreeNode_labelMut(TerminalTreeNode *s) { return &(*s).label; }
;;SETTER
static inline fn TerminalTreeNode_setLabel(TerminalTreeNode *s, const char * v) { (*s).label = v; }
;;GETTER
static inline bytes TerminalTreeNode_subtreeCount(const TerminalTreeNode *s) { return (*s).subtreeCount; }
;;GETTER
static inline bytes *TerminalTreeNode_subtreeCountMut(TerminalTreeNode *s) { return &(*s).subtreeCount; }
;;SETTER
static inline fn TerminalTreeNode_setSubtreeCount(TerminalTreeNode *s, bytes v) { (*s).subtreeCount = v; }

;;GETTER
static inline bytes TerminalTreeView_indentSpaces(const TerminalTreeView *s) { return (*s).indentSpaces; }
;;GETTER
static inline bytes *TerminalTreeView_indentSpacesMut(TerminalTreeView *s) { return &(*s).indentSpaces; }
;;SETTER
static inline fn TerminalTreeView_setIndentSpaces(TerminalTreeView *s, bytes v) { (*s).indentSpaces = v; }
;;GETTER
static inline bool TerminalTreeView_nerdIcons(const TerminalTreeView *s) { return (*s).nerdIcons; }
;;GETTER
static inline bool *TerminalTreeView_nerdIconsMut(TerminalTreeView *s) { return &(*s).nerdIcons; }
;;SETTER
static inline fn TerminalTreeView_setNerdIcons(TerminalTreeView *s, bool v) { (*s).nerdIcons = v; }
;;GETTER
static inline TerminalTreeNodeId TerminalTreeView_nextId(const TerminalTreeView *s) { return (*s).nextId; }
;;GETTER
static inline TerminalTreeNodeId *TerminalTreeView_nextIdMut(TerminalTreeView *s) { return &(*s).nextId; }
;;SETTER
static inline fn TerminalTreeView_setNextId(TerminalTreeView *s, TerminalTreeNodeId v) { (*s).nextId = v; }
;;GETTER
static inline bytes TerminalTreeView_nodeCapacity(const TerminalTreeView *s) { return (*s).nodeCapacity; }
;;GETTER
static inline bytes *TerminalTreeView_nodeCapacityMut(TerminalTreeView *s) { return &(*s).nodeCapacity; }
;;SETTER
static inline fn TerminalTreeView_setNodeCapacity(TerminalTreeView *s, bytes v) { (*s).nodeCapacity = v; }
;;GETTER
static inline bytes TerminalTreeView_nodeCount(const TerminalTreeView *s) { return (*s).nodeCount; }
;;GETTER
static inline bytes *TerminalTreeView_nodeCountMut(TerminalTreeView *s) { return &(*s).nodeCount; }
;;SETTER
static inline fn TerminalTreeView_setNodeCount(TerminalTreeView *s, bytes v) { (*s).nodeCount = v; }
;;GETTER
static inline bytes TerminalTreeView_nodeRefusals(const TerminalTreeView *s) { return (*s).nodeRefusals; }
;;GETTER
static inline bytes *TerminalTreeView_nodeRefusalsMut(TerminalTreeView *s) { return &(*s).nodeRefusals; }
;;SETTER
static inline fn TerminalTreeView_setNodeRefusals(TerminalTreeView *s, bytes v) { (*s).nodeRefusals = v; }
;;GETTER
static inline TerminalTreeNode * TerminalTreeView_nodes(const TerminalTreeView *s) { return (*s).nodes; }
;;GETTER
static inline TerminalTreeNode * *TerminalTreeView_nodesMut(TerminalTreeView *s) { return &(*s).nodes; }
;;SETTER
static inline fn TerminalTreeView_setNodes(TerminalTreeView *s, TerminalTreeNode * v) { (*s).nodes = v; }
;;GETTER
static inline bytes TerminalTreeView_rowRefusals(const TerminalTreeView *s) { return (*s).rowRefusals; }
;;GETTER
static inline bytes *TerminalTreeView_rowRefusalsMut(TerminalTreeView *s) { return &(*s).rowRefusals; }
;;SETTER
static inline fn TerminalTreeView_setRowRefusals(TerminalTreeView *s, bytes v) { (*s).rowRefusals = v; }
;;GETTER
static inline u32 * TerminalTreeView_rows(const TerminalTreeView *s) { return (*s).rows; }
;;GETTER
static inline u32 * *TerminalTreeView_rowsMut(TerminalTreeView *s) { return &(*s).rows; }
;;SETTER
static inline fn TerminalTreeView_setRows(TerminalTreeView *s, u32 * v) { (*s).rows = v; }
;;GETTER
static inline bytes TerminalTreeView_rowsCapacity(const TerminalTreeView *s) { return (*s).rowsCapacity; }
;;GETTER
static inline bytes *TerminalTreeView_rowsCapacityMut(TerminalTreeView *s) { return &(*s).rowsCapacity; }
;;SETTER
static inline fn TerminalTreeView_setRowsCapacity(TerminalTreeView *s, bytes v) { (*s).rowsCapacity = v; }
;;GETTER
static inline bytes TerminalTreeView_scrollRow(const TerminalTreeView *s) { return (*s).scrollRow; }
;;GETTER
static inline bytes *TerminalTreeView_scrollRowMut(TerminalTreeView *s) { return &(*s).scrollRow; }
;;SETTER
static inline fn TerminalTreeView_setScrollRow(TerminalTreeView *s, bytes v) { (*s).scrollRow = v; }
;;GETTER
static inline TerminalTreeNodeId TerminalTreeView_selected(const TerminalTreeView *s) { return (*s).selected; }
;;GETTER
static inline TerminalTreeNodeId *TerminalTreeView_selectedMut(TerminalTreeView *s) { return &(*s).selected; }
;;SETTER
static inline fn TerminalTreeView_setSelected(TerminalTreeView *s, TerminalTreeNodeId v) { (*s).selected = v; }
;;GETTER
static inline bytes TerminalTreeView_viewportRows(const TerminalTreeView *s) { return (*s).viewportRows; }
;;GETTER
static inline bytes *TerminalTreeView_viewportRowsMut(TerminalTreeView *s) { return &(*s).viewportRows; }
;;SETTER
static inline fn TerminalTreeView_setViewportRows(TerminalTreeView *s, bytes v) { (*s).viewportRows = v; }
;;GETTER
static inline bytes TerminalTreeView_visibleCount(const TerminalTreeView *s) { return (*s).visibleCount; }
;;GETTER
static inline bytes *TerminalTreeView_visibleCountMut(TerminalTreeView *s) { return &(*s).visibleCount; }
;;SETTER
static inline fn TerminalTreeView_setVisibleCount(TerminalTreeView *s, bytes v) { (*s).visibleCount = v; }

#define TERMINAL_TREE_NO_INDEX ((bytes) -1)
#define TERMINAL_TREE_NO_PARENT ((i32) -1)

static fn refuseNode(TerminalTreeView *self, const char *reason) {
    if (TerminalTreeView_nodeRefusals(self) == 0)
        THROW("TerminalTreeView exhausted: %s (node capacity %zu, live %zu)",
              reason, TerminalTreeView_nodeCapacity(self), TerminalTreeView_nodeCount(self));
    if (TerminalTreeView_nodeRefusals(self) != SIZE_MAX)
        TerminalTreeView_setNodeRefusals(self, TerminalTreeView_nodeRefusals(self) + 1u);
}

static fn refuseRow(TerminalTreeView *self, const char *reason) {
    if (TerminalTreeView_rowRefusals(self) == 0)
        THROW("TerminalTreeView exhausted: %s (row capacity %zu, live %zu)",
              reason, TerminalTreeView_rowsCapacity(self), TerminalTreeView_visibleCount(self));
    if (TerminalTreeView_rowRefusals(self) != SIZE_MAX)
        TerminalTreeView_setRowRefusals(self, TerminalTreeView_rowRefusals(self) + 1u);
}

static bool growNodes(TerminalTreeView *self) {
    bytes next = TerminalTreeView_nodeCapacity(self) == 0 ? TERMINAL_TREE_CHILDREN_DEFAULT
                                            : TerminalTreeView_nodeCapacity(self) * 2;
    if (next < TerminalTreeView_nodeCapacity(self) || next > SIZE_MAX / sizeof(TerminalTreeNode)) {
        refuseNode(self, "node capacity overflow");
        return false;
    }
    TerminalTreeNode *grown = Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), next * sizeof(TerminalTreeNode));
    if (grown == nullptr) {
        refuseNode(self, "node array allocation refused");
        return false;
    }
    if (TerminalTreeView_nodeCapacity(self) != 0)
        memcpy(grown, TerminalTreeView_nodes(self), TerminalTreeView_nodeCapacity(self) * sizeof(TerminalTreeNode));
    memset(grown + TerminalTreeView_nodeCapacity(self), 0, (next - TerminalTreeView_nodeCapacity(self)) * sizeof(TerminalTreeNode));
    Memory_free(TerminalTreeView_nodes(self));
    TerminalTreeView_setNodes(self, grown);
    TerminalTreeView_setNodeCapacity(self, next);
    return true;
}

static bool growRows(TerminalTreeView *self) {
    bytes next = TerminalTreeView_rowsCapacity(self) == 0 ? TERMINAL_TREE_CHILDREN_DEFAULT
                                            : TerminalTreeView_rowsCapacity(self) * 2;
    if (next < TerminalTreeView_rowsCapacity(self) || next > SIZE_MAX / sizeof(u32)) {
        refuseRow(self, "row capacity overflow");
        return false;
    }
    u32 *grown = Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), next * sizeof(u32));
    if (grown == nullptr) {
        refuseRow(self, "row array allocation refused");
        return false;
    }
    if (TerminalTreeView_rowsCapacity(self) != 0)
        memcpy(grown, TerminalTreeView_rows(self), TerminalTreeView_rowsCapacity(self) * sizeof(u32));
    Memory_free(TerminalTreeView_rows(self));
    TerminalTreeView_setRows(self, grown);
    TerminalTreeView_setRowsCapacity(self, next);
    return true;
}

// Identity is never positional: a node keeps its id when its neighbours shift, which
// they do on every insertion because the array is pre-order. Cold, and deliberately a
// scan — the alternative is a second index to keep in step with this one.
static bytes indexFor(const TerminalTreeView *self, TerminalTreeNodeId id) {
    if (self == nullptr || id == TERMINAL_TREE_NO_NODE)
        return TERMINAL_TREE_NO_INDEX;
    for (bytes index = 0; index < TerminalTreeView_nodeCount(self); ++index)
        if (TerminalTreeView_nodes(self)[index].id == id)
            return index;
    return TERMINAL_TREE_NO_INDEX;
}

static bool appendRow(TerminalTreeView *self, bytes index) {
    if (TerminalTreeView_visibleCount(self) == TerminalTreeView_rowsCapacity(self) && !growRows(self))
        return false;
    TerminalTreeView_rows(self)[TerminalTreeView_visibleCount(self)] = (u32) index;
    TerminalTreeView_setVisibleCount(self, TerminalTreeView_visibleCount(self) + 1u);
    return true;
}

// Recompute the rows that are actually shown. The pre-order layout is the whole trick: a
// folded node's subtree is one contiguous range, so folding skips it in a single step and
// unfolding descends into it one node at a time.
static fn rebuild(TerminalTreeView *self) {
    TerminalTreeView_setVisibleCount(self, 0);
    bytes at = 0;
    while (at < TerminalTreeView_nodeCount(self)) {
        if (!appendRow(self, at))
            break;                       // reported and counted; keep what we resolved
        const TerminalTreeNode *node = &TerminalTreeView_nodesMut(self)[at];
        if (TerminalTreeNode_folder(node) && TerminalTreeNode_expanded(node))
            at += 1;
        else
            at += TerminalTreeNode_subtreeCount(node);
    }
    bytes maximum = TerminalTreeView_maxScrollRow(self);
    if (TerminalTreeView_scrollRow(self) > maximum)
        TerminalTreeView_setScrollRow(self, maximum);
}

TerminalTreeView *TerminalTreeView_0() {
    TerminalTreeView *self = Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), sizeof(TerminalTreeView));
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
    Memory_free(TerminalTreeView_nodes(self));
    Memory_free(TerminalTreeView_rows(self));
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
    if (TerminalTreeView_nodeCount(self) == TerminalTreeView_nodeCapacity(self) && !growNodes(self))
        return TERMINAL_TREE_NO_NODE;
    // A new node lands at the END of its parent's subtree. Everything after that point
    // shifts up by one, and every ancestor's subtreeCount grows to swallow it — which is
    // what keeps the array in pre-order without a single child list.
    bytes at = parentIndex == TERMINAL_TREE_NO_INDEX
                    ? TerminalTreeView_nodeCount(self)
                    : parentIndex + TerminalTreeView_nodes(self)[parentIndex].subtreeCount;
    if (at < TerminalTreeView_nodeCount(self))
        memmove(&TerminalTreeView_nodesMut(self)[at + 1], &TerminalTreeView_nodesMut(self)[at],
                (TerminalTreeView_nodeCount(self) - at) * sizeof(TerminalTreeNode));
    TerminalTreeNode *node = &TerminalTreeView_nodesMut(self)[at];
    *node = (TerminalTreeNode) {
        .id = TerminalTreeView_nextId(self),
        .parent = parentIndex == TERMINAL_TREE_NO_INDEX ? TERMINAL_TREE_NO_PARENT
                                                        : (i32) parentIndex,
        .subtreeCount = 1,
        .depth = parentIndex == TERMINAL_TREE_NO_INDEX
                     ? 0
                     : TerminalTreeView_nodes(self)[parentIndex].depth + 1,
        .folder = folder,
        .expanded = false,
        .label = label,
    };
    TerminalTreeView_setNodeCount(self, TerminalTreeView_nodeCount(self) + 1u);
    TerminalTreeView_setNextId(self, TerminalTreeView_nextId(self) + 1u);
    for (bytes walk = parentIndex; walk != TERMINAL_TREE_NO_INDEX; ) {
        ++TerminalTreeView_nodesMut(self)[walk].subtreeCount;
        walk = TerminalTreeView_nodes(self)[walk].parent == TERMINAL_TREE_NO_PARENT
                   ? TERMINAL_TREE_NO_INDEX
                   : (bytes) TerminalTreeView_nodes(self)[walk].parent;
    }
    // A node that gains its first child is a folder, and it opens. A caller that just
    // added a child plainly wants it seen, and the alternative — rows that exist in the
    // array and are invisible because a flag elsewhere says "leaf" — is a silent trap
    // rather than a design.
    if (parentIndex != TERMINAL_TREE_NO_INDEX && !TerminalTreeView_nodes(self)[parentIndex].folder) {
        TerminalTreeView_nodes(self)[parentIndex].folder = true;
        TerminalTreeView_nodes(self)[parentIndex].expanded = true;
    }
    rebuild(self);
    return TerminalTreeView_nodes(self)[at].id;
}

bool TerminalTreeView_setLabel(TerminalTreeView *self, TerminalTreeNodeId id, const char *label) {
    if (label == nullptr)
        return false;
    bytes index = indexFor(self, id);
    if (index == TERMINAL_TREE_NO_INDEX)
        return false;
    TerminalTreeView_nodes(self)[index].label = label;
    return true;
}

bool TerminalTreeView_remove(TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    if (index == TERMINAL_TREE_NO_INDEX)
        return false;
    const bytes removed = TerminalTreeView_nodes(self)[index].subtreeCount;
    const i32 parent = TerminalTreeView_nodes(self)[index].parent;
    // A selection inside the subtree is cleared rather than silently re-pointed: the
    // selected thing went away, and ids are never reused, so there is nothing to fall
    // back to that would be honest.
    for (bytes at = index; at < index + removed; ++at) {
        if (TerminalTreeView_nodes(self)[at].id == TerminalTreeView_selected(self)) {
            TerminalTreeView_setSelected(self, TERMINAL_TREE_NO_NODE);
            break;
        }
    }
    for (bytes walk = parent == TERMINAL_TREE_NO_PARENT ? TERMINAL_TREE_NO_INDEX : (bytes) parent;
         walk != TERMINAL_TREE_NO_INDEX; ) {
        TerminalTreeView_nodes(self)[walk].subtreeCount -= removed;
        walk = TerminalTreeView_nodes(self)[walk].parent == TERMINAL_TREE_NO_PARENT
                   ? TERMINAL_TREE_NO_INDEX
                   : (bytes) TerminalTreeView_nodes(self)[walk].parent;
    }
    if (index + removed < TerminalTreeView_nodeCount(self))
        memmove(&TerminalTreeView_nodesMut(self)[index], &TerminalTreeView_nodesMut(self)[index + removed],
                (TerminalTreeView_nodeCount(self) - index - removed) * sizeof(TerminalTreeNode));
    TerminalTreeView_setNodeCount(self, TerminalTreeView_nodeCount(self) - (removed));
    rebuild(self);
    return true;
}

fn TerminalTreeView_clear(TerminalTreeView *self) {
    if (self == nullptr)
        return;
    TerminalTreeView_setNodeCount(self, 0);
    TerminalTreeView_setSelected(self, TERMINAL_TREE_NO_NODE);
    TerminalTreeView_setScrollRow(self, 0);
    TerminalTreeView_setVisibleCount(self, 0);
    // nextId is deliberately NOT rewound. A refresh hands out new ids, so a caller still
    // holding one from before finds nothing instead of somebody else's row.
}

bool TerminalTreeView_setExpanded(TerminalTreeView *self, TerminalTreeNodeId id, bool expanded) {
    bytes index = indexFor(self, id);
    if (index == TERMINAL_TREE_NO_INDEX)
        return false;
    if (TerminalTreeView_nodes(self)[index].expanded == expanded)
        return true;
    TerminalTreeView_nodes(self)[index].expanded = expanded;
    rebuild(self);
    return true;
}

bool TerminalTreeView_toggle(TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    if (index == TERMINAL_TREE_NO_INDEX)
        return false;
    TerminalTreeView_nodes(self)[index].expanded = !TerminalTreeView_nodes(self)[index].expanded;
    rebuild(self);
    return TerminalTreeView_nodes(self)[index].expanded;
}

bool TerminalTreeView_isExpanded(const TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    return index != TERMINAL_TREE_NO_INDEX && TerminalTreeView_nodes(self)[index].expanded;
}

bool TerminalTreeView_isFolder(const TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    return index != TERMINAL_TREE_NO_INDEX && TerminalTreeView_nodes(self)[index].folder;
}

bool TerminalTreeView_reveal(TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    if (index == TERMINAL_TREE_NO_INDEX)
        return false;
    bytes walk = index;
    while (walk != TERMINAL_TREE_NO_INDEX) {
        TerminalTreeView_nodes(self)[walk].expanded = true;
        walk = TerminalTreeView_nodes(self)[walk].parent == TERMINAL_TREE_NO_PARENT
                   ? TERMINAL_TREE_NO_INDEX
                   : (bytes) TerminalTreeView_nodes(self)[walk].parent;
    }
    rebuild(self);
    return true;
}

bool TerminalTreeView_setSelected(TerminalTreeView *self, TerminalTreeNodeId id) {
    if (self == nullptr)
        return false;
    if (id != TERMINAL_TREE_NO_NODE && indexFor(self, id) == TERMINAL_TREE_NO_INDEX)
        return false;
    TerminalTreeView_setSelected(self, id);
    return true;
}

TerminalTreeNodeId TerminalTreeView_getSelected(const TerminalTreeView *self) {
    return self == nullptr ? TERMINAL_TREE_NO_NODE : TerminalTreeView_selected(self);
}

bool TerminalTreeView_isSelected(const TerminalTreeView *self, TerminalTreeNodeId id) {
    return self != nullptr && id != TERMINAL_TREE_NO_NODE && TerminalTreeView_selected(self) == id;
}

bytes TerminalTreeView_visibleCount(const TerminalTreeView *self) {
    return self == nullptr ? 0 : TerminalTreeView_visibleCount(self);
}

bool TerminalTreeView_setViewportRows(TerminalTreeView *self, bytes rows) {
    if (self == nullptr)
        return false;
    TerminalTreeView_setViewportRows(self, rows);
    bytes maximum = TerminalTreeView_maxScrollRow(self);
    if (TerminalTreeView_scrollRow(self) > maximum)
        TerminalTreeView_setScrollRow(self, maximum);
    return true;
}

bytes TerminalTreeView_viewportRows(const TerminalTreeView *self) {
    return self == nullptr ? 0 : TerminalTreeView_viewportRows(self);
}

bytes TerminalTreeView_scrollRow(const TerminalTreeView *self) {
    return self == nullptr ? 0 : TerminalTreeView_scrollRow(self);
}

bytes TerminalTreeView_maxScrollRow(const TerminalTreeView *self) {
    if (self == nullptr || TerminalTreeView_visibleCount(self) <= TerminalTreeView_viewportRows(self))
        return 0;
    return TerminalTreeView_visibleCount(self) - TerminalTreeView_viewportRows(self);
}

bytes TerminalTreeView_setScrollRow(TerminalTreeView *self, bytes row) {
    if (self == nullptr)
        return 0;
    bytes maximum = TerminalTreeView_maxScrollRow(self);
    TerminalTreeView_setScrollRow(self, row > maximum ? maximum : row);
    return TerminalTreeView_scrollRow(self);
}

bytes TerminalTreeView_scrollBy(TerminalTreeView *self, i64 rows) {
    if (self == nullptr)
        return 0;
    i64 wanted = (i64) TerminalTreeView_scrollRow(self) + rows;
    if (wanted < 0)
        wanted = 0;
    return TerminalTreeView_setScrollRow(self, (bytes) wanted);
}

TerminalTreeRow TerminalTreeView_rowAt(const TerminalTreeView *self, bytes row) {
    TerminalTreeRow empty = {0};
    if (self == nullptr || row >= TerminalTreeView_visibleCount(self))
        return empty;
    const TerminalTreeNode *node = &TerminalTreeView_nodesMut(self)[TerminalTreeView_rows(self)[row]];
    TerminalTreeRow record = {
        .id = TerminalTreeNode_id(node),
        .label = TerminalTreeNode_label(node),
        .depth = TerminalTreeNode_depth(node),
        .folder = TerminalTreeNode_folder(node),
        .expanded = TerminalTreeNode_expanded(node),
        .selected = TerminalTreeNode_id(node) == TerminalTreeView_selected(self),
    };
    return record;
}

TerminalTreeNodeId TerminalTreeView_nodeAtOffset(const TerminalTreeView *self, bytes offset) {
    if (self == nullptr || offset >= TerminalTreeView_viewportRows(self))
        return TERMINAL_TREE_NO_NODE;
    bytes row = TerminalTreeView_scrollRow(self) + offset;
    if (row >= TerminalTreeView_visibleCount(self))
        return TERMINAL_TREE_NO_NODE;
    return TerminalTreeView_nodes(self)[TerminalTreeView_rows(self)[row]].id;
}

bytes TerminalTreeView_rowOf(const TerminalTreeView *self, TerminalTreeNodeId id) {
    if (self == nullptr || id == TERMINAL_TREE_NO_NODE)
        return TERMINAL_TREE_NO_ROW;
    for (bytes row = 0; row < TerminalTreeView_visibleCount(self); ++row)
        if (TerminalTreeView_nodes(self)[TerminalTreeView_rows(self)[row]].id == id)
            return row;
    return TERMINAL_TREE_NO_ROW;
}

bool TerminalTreeView_isNode(const TerminalTreeView *self, TerminalTreeNodeId id) {
    return indexFor(self, id) != TERMINAL_TREE_NO_INDEX;
}

const char *TerminalTreeView_getLabel(const TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    return index == TERMINAL_TREE_NO_INDEX ? nullptr : TerminalTreeView_nodes(self)[index].label;
}

bytes TerminalTreeView_depthOf(const TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    return index == TERMINAL_TREE_NO_INDEX ? 0 : TerminalTreeView_nodes(self)[index].depth;
}

bytes TerminalTreeView_childCountOf(const TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    if (index == TERMINAL_TREE_NO_INDEX)
        return 0;
    // Direct children only, and they are not adjacent in a pre-order array — a child's
    // own subtree sits between it and its next sibling — so this walks the node's own
    // range rather than reading two numbers.
    bytes count = 0;
    bytes end = index + TerminalTreeView_nodes(self)[index].subtreeCount;
    for (bytes at = index + 1; at < end; ++at)
        if (TerminalTreeView_nodes(self)[at].parent == (i32) index)
            ++count;
    return count;
}

bytes TerminalTreeView_subtreeCountOf(const TerminalTreeView *self, TerminalTreeNodeId id) {
    bytes index = indexFor(self, id);
    return index == TERMINAL_TREE_NO_INDEX ? 0 : TerminalTreeView_nodes(self)[index].subtreeCount;
}

bytes TerminalTreeView_nodeCount(const TerminalTreeView *self) {
    return self == nullptr ? 0 : TerminalTreeView_nodeCount(self);
}

bytes TerminalTreeView_capacity(const TerminalTreeView *self) {
    return self == nullptr ? 0 : TerminalTreeView_nodeCapacity(self);
}

// Both resources are counted, because "the tree refused" is only half an answer: the
// caller needs to know whether it ran out of NODES or of ROWS, and the diagnostic above
// names which. This total is what a test asserts against.
bytes TerminalTreeView_exhaustionCount(const TerminalTreeView *self) {
    if (self == nullptr)
        return 0;
    if (SIZE_MAX - TerminalTreeView_nodeRefusals(self) < TerminalTreeView_rowRefusals(self))
        return SIZE_MAX;
    return TerminalTreeView_nodeRefusals(self) + TerminalTreeView_rowRefusals(self);
}

fn TerminalTreeView_clearExhaustion(TerminalTreeView *self) {
    if (self == nullptr)
        return;
    TerminalTreeView_setNodeRefusals(self, 0);
    TerminalTreeView_setRowRefusals(self, 0);
}

bool TerminalTreeView_setIndent(TerminalTreeView *self, bytes indentSpaces) {
    if (self == nullptr)
        return false;
    TerminalTreeView_setIndentSpaces(self, indentSpaces);
    return true;
}

bytes TerminalTreeView_getIndent(const TerminalTreeView *self) {
    return self != nullptr ? TerminalTreeView_indentSpaces(self) : 2u;
}

bool TerminalTreeView_setNerdIcons(TerminalTreeView *self, bool nerdIcons) {
    if (self == nullptr)
        return false;
    TerminalTreeView_setNerdIcons(self, nerdIcons);
    return true;
}

bool TerminalTreeView_getNerdIcons(const TerminalTreeView *self) {
    return self != nullptr ? TerminalTreeView_nerdIcons(self) : true;
}

fn TerminalTreeView_paint(const TerminalTreeView *self, TerminalCanvas *canvas) {
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    bytes visible = TerminalTreeView_visibleCount(self);
    bytes scroll = TerminalTreeView_scrollRow(self);
    bytes height = TerminalCanvas_getHeight(canvas);
    bytes width = TerminalCanvas_getWidth(canvas);
    bytes rowsToPaint = visible > scroll ? (visible - scroll) : 0u;
    if (rowsToPaint > height)
        rowsToPaint = height;

    for (bytes y = 0u; y < rowsToPaint; ++y) {
        TerminalTreeRow row = TerminalTreeView_rowAt(self, scroll + y);
        if (row.id == TERMINAL_TREE_NO_NODE)
            continue;
        bytes indent = row.depth * TerminalTreeView_indentSpaces(self);
        if (indent >= width)
            continue;

        const char *icon = "";
        if (row.folder) {
            if (TerminalTreeView_nerdIcons(self))
                icon = row.expanded ? "\xef\x84\x95 " : "\xef\x84\x94 ";
            else
                icon = row.expanded ? "[-] " : "[+] ";
        } else {
            if (TerminalTreeView_nerdIcons(self))
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
            bytes labelX = indent + (TerminalTreeView_nerdIcons(self) ? 3u : 4u);
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
        TerminalTreeView_nodeCount(self), TerminalTreeView_nodeCount(self) == 1u ? "" : "s", TerminalTreeView_visibleCount(self),
        TerminalTreeView_scrollRow(self), TerminalTreeView_maxScrollRow(self), TerminalTreeView_viewportRows(self));
}

TerminalProjection TerminalTreeView_toStringStruct(const TerminalTreeView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalTreeView { nodes=%p nodeCount=%zu nodeCapacity=%zu rows=%p visibleCount=%zu "
        "rowsCapacity=%zu nextId=%u selected=%u scrollRow=%zu viewportRows=%zu "
        "nodeRefusals=%zu rowRefusals=%zu indentSpaces=%zu nerdIcons=%d }",
        (const_any) TerminalTreeView_nodes(self), TerminalTreeView_nodeCount(self), TerminalTreeView_nodeCapacity(self),
        (const_any) TerminalTreeView_rows(self), TerminalTreeView_visibleCount(self), TerminalTreeView_rowsCapacity(self),
        (u32) TerminalTreeView_nextId(self), (u32) TerminalTreeView_selected(self), TerminalTreeView_scrollRow(self),
        TerminalTreeView_viewportRows(self), TerminalTreeView_nodeRefusals(self), TerminalTreeView_rowRefusals(self),
        TerminalTreeView_indentSpaces(self), TerminalTreeView_nerdIcons(self));
}
