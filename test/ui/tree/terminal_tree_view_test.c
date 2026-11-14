// Owner: TerminalTreeView — the scrollable, collapsible tree behind a file browser.
//
// It proves the two things that decide whether this widget is usable at all: the
// PRE-ORDER LAYOUT stays honest through every insertion and removal (a node's subtree
// really is the contiguous range its subtreeCount claims), and the VISIBLE ROWS are
// right for every fold state. Both are checked against counts computed HERE by walking
// the visible rows — independently of the arrays the widget maintains — so an assertion
// cannot pass by the widget agreeing with itself.
//
// It also proves the boundaries a caller will actually hit: an unknown id changes
// nothing, a stale id is never reused to address a neighbour, a click past the last row
// is not a click on a row, and the viewport cannot be left scrolled past the end of a
// tree that just folded underneath it.
//
// There is no terminal here and there does not need to be: this class owns structure,
// folding, selection and the viewport, and owns NO drawing. That split is exactly why
// the interaction can be pinned down like this.
//
// STATED GAP: allocation failure is not injectable, so a refused node/row growth is not
// exercised — only its counting and its diagnostic are. The finite boundary this owner
// does prove is the one a caller reaches by accident: the viewport and the row list.
#include "ui/tree/terminal_tree_view.h"
#include "ui/surface/terminal_canvas.h"
#include "determinant/test/support.h"
#include <string.h>

// The number of nodes BELOW the one displayed at `row`, counted from the displayed rows
// themselves: every following row that is deeper than this one is inside its subtree, and
// the first row that is not deeper ends it. This is the pre-order invariant restated as a
// question about the screen, which is why it is a fair check on the internal array.
static bytes countedDescendants(const TerminalTreeView *view, bytes row) {
    bytes depth = TerminalTreeView_rowAt(view, row).depth;
    bytes count = 0;
    for (bytes at = row + 1; at < TerminalTreeView_visibleCount(view); ++at) {
        if (TerminalTreeView_rowAt(view, at).depth <= depth)
            break;
        ++count;
    }
    return count;
}

// Direct children only: the rows exactly one level deeper, inside the same subtree.
static bytes countedChildren(const TerminalTreeView *view, bytes row) {
    bytes depth = TerminalTreeView_rowAt(view, row).depth;
    bytes count = 0;
    for (bytes at = row + 1; at < TerminalTreeView_visibleCount(view); ++at) {
        bytes here = TerminalTreeView_rowAt(view, at).depth;
        if (here <= depth)
            break;
        if (here == depth + 1u)
            ++count;
    }
    return count;
}

// Folding may REMOVE rows; it must never reorder them. Captured once in the fully
// expanded state, then checked after every fold: the visible sequence is always a
// subsequence of this one.
#define ORDER_MAX 64u
typedef struct RowOrder {
    TerminalTreeNodeId ids[ORDER_MAX];
    bytes count;
} RowOrder;

static RowOrder captureOrder(const TerminalTreeView *view) {
    RowOrder order = {0};
    bytes count = TerminalTreeView_visibleCount(view);
    if (count > ORDER_MAX)
        count = ORDER_MAX;
    for (bytes row = 0; row < count; ++row)
        order.ids[row] = TerminalTreeView_rowAt(view, row).id;
    order.count = count;
    return order;
}

static bool isSubsequenceOf(const TerminalTreeView *view, RowOrder order) {
    bytes at = 0;
    for (bytes row = 0; row < TerminalTreeView_visibleCount(view); ++row) {
        TerminalTreeNodeId id = TerminalTreeView_rowAt(view, row).id;
        while (at < order.count && order.ids[at] != id)
            ++at;
        if (at == order.count)
            return false;
        ++at;
    }
    return true;
}

// A tiny deterministic generator, so a fold fuzz repeats exactly and a failure can be
// reproduced from the seed printed on failure.
static u32 lcg(u32 *state) {
    *state = (*state * 1664525u) + 1013904223u;
    return *state >> 8;
}

// Rebuild the design's tree in a fresh view: vexgraph / src / three files, one of which
// grows a nested folder. Returns the view and reports the eight ids through `ids`.
static TerminalTreeView *buildTree(TerminalTreeNodeId *ids) {
    static const char *labels[] = { "vexgraph", "src", "opencode.json", "hello.txt",
                                    "example.java", "nested", "deep.c", "whatever.c" };
    TerminalTreeView *view = TerminalTreeView();
    ids[0] = TerminalTreeView_addChild(view, TERMINAL_TREE_NO_NODE, labels[0], true);
    ids[1] = TerminalTreeView_addChild(view, ids[0], labels[1], true);
    ids[2] = TerminalTreeView_addChild(view, ids[1], labels[2], false);
    ids[3] = TerminalTreeView_addChild(view, ids[1], labels[3], false);
    ids[4] = TerminalTreeView_addChild(view, ids[1], labels[4], false);
    ids[5] = TerminalTreeView_addChild(view, ids[4], labels[5], true);
    ids[6] = TerminalTreeView_addChild(view, ids[5], labels[6], false);
    ids[7] = TerminalTreeView_addChild(view, ids[0], labels[7], false);
    return view;
}

int main() {
    // --- Null safety, on every entry point ---
    assert(TerminalTreeView_visibleCount(nullptr) == 0);
    assert(TerminalTreeView_scrollRow(nullptr) == 0);
    assert(TerminalTreeView_maxScrollRow(nullptr) == 0);
    assert(TerminalTreeView_setScrollRow(nullptr, 5) == 0);
    assert(TerminalTreeView_scrollBy(nullptr, 5) == 0);
    assert(TerminalTreeView_nodeCount(nullptr) == 0);
    assert(TerminalTreeView_capacity(nullptr) == 0);
    assert(TerminalTreeView_exhaustionCount(nullptr) == 0);
    assert(TerminalTreeView_viewportRows(nullptr) == 0);
    assert(TerminalTreeView_getSelected(nullptr) == TERMINAL_TREE_NO_NODE);
    assert(TerminalTreeView_getLabel(nullptr, 1u) == nullptr);
    assert(TerminalTreeView_depthOf(nullptr, 1u) == 0);
    assert(TerminalTreeView_childCountOf(nullptr, 1u) == 0);
    assert(TerminalTreeView_subtreeCountOf(nullptr, 1u) == 0);
    assert(TerminalTreeView_rowOf(nullptr, 1u) == TERMINAL_TREE_NO_ROW);
    assert(TerminalTreeView_nodeAtOffset(nullptr, 0) == TERMINAL_TREE_NO_NODE);
    assert(!TerminalTreeView_isNode(nullptr, 1u));
    assert(!TerminalTreeView_isFolder(nullptr, 1u));
    assert(!TerminalTreeView_isExpanded(nullptr, 1u));
    assert(!TerminalTreeView_isSelected(nullptr, 1u));
    assert(!TerminalTreeView_setSelected(nullptr, 1u));
    assert(!TerminalTreeView_setLabel(nullptr, 1u, "x"));
    assert(!TerminalTreeView_remove(nullptr, 1u));
    assert(!TerminalTreeView_setExpanded(nullptr, 1u, true));
    assert(!TerminalTreeView_toggle(nullptr, 1u));
    assert(!TerminalTreeView_reveal(nullptr, 1u));
    assert(!TerminalTreeView_setViewportRows(nullptr, 4));
    assert(TerminalTreeView_addChild(nullptr, TERMINAL_TREE_NO_NODE, "x", false)
           == TERMINAL_TREE_NO_NODE);
    TerminalTreeRow nothing = TerminalTreeView_rowAt(nullptr, 0);
    assert(nothing.id == TERMINAL_TREE_NO_NODE && nothing.label == nullptr && !nothing.folder);
    TerminalTreeView_clear(nullptr);
    TerminalTreeView_clearExhaustion(nullptr);
    TerminalTreeView_free(nullptr);                             // null is harmless
    assert(strcmp(TerminalTreeView_toString(nullptr).text, "nullptr") == 0);
    assert(strcmp(TerminalTreeView_toStringStruct(nullptr).text, "nullptr") == 0);

    // --- Construction, and refusals that change nothing ---
    TerminalTreeView *fresh = TerminalTreeView();
    assert(fresh != nullptr);
    assert(TerminalTreeView_nodeCount(fresh) == 0);
    assert(TerminalTreeView_visibleCount(fresh) == 0);
    assert(TerminalTreeView_capacity(fresh) == 0);              // nothing allocated yet
    assert(TerminalTreeView_viewportRows(fresh) == TERMINAL_TREE_VIEWPORT_DEFAULT);
    assert(TerminalTreeView_getSelected(fresh) == TERMINAL_TREE_NO_NODE);
    assert(TerminalTreeView_rowAt(fresh, 0).id == TERMINAL_TREE_NO_NODE);
    assert(TerminalTreeView_addChild(fresh, 1u, "orphan", false) == TERMINAL_TREE_NO_NODE);
    assert(TerminalTreeView_addChild(fresh, TERMINAL_TREE_NO_NODE, nullptr, false)
           == TERMINAL_TREE_NO_NODE);
    assert(TerminalTreeView_nodeCount(fresh) == 0);             // a refused add adds nothing
    assert(TerminalTreeView_capacity(fresh) == 0);
    TerminalTreeView_free(fresh);

    // --- The design's tree ---
    TerminalTreeNodeId id[8];
    TerminalTreeView *view = buildTree(id);
    for (bytes index = 0; index < 8; ++index)
        assert(id[index] == (TerminalTreeNodeId) (index + 1u));  // ids start at one, never zero
    // A node that gains its first child becomes a FOLDER and opens: `example.java` was
    // declared a leaf, and hiding its new child behind a stale flag would be a trap.
    assert(TerminalTreeView_isFolder(view, id[4]));
    assert(TerminalTreeView_isExpanded(view, id[4]));
    assert(TerminalTreeView_isFolder(view, id[5]));
    assert(TerminalTreeView_isFolder(view, id[0]));

    // --- A tree ARRIVES FOLDED ---
    // A declared folder is collapsed, so building five levels does not put five levels on
    // the screen. The root is the only row until something opens something.
    assert(TerminalTreeView_visibleCount(view) == 1u);
    assert(TerminalTreeView_rowAt(view, 0).id == id[0]);
    assert(TerminalTreeView_rowAt(view, 0).folder && !TerminalTreeView_rowAt(view, 0).expanded);
    assert(TerminalTreeView_rowOf(view, id[1]) == TERMINAL_TREE_NO_ROW);
    assert(TerminalTreeView_rowOf(view, id[7]) == TERMINAL_TREE_NO_ROW);
    // All eight nodes are there regardless: visibility is a presentation of the tree,
    // never a property of it.
    assert(TerminalTreeView_nodeCount(view) == 8u);
    assert(TerminalTreeView_nodeAtOffset(view, 0) == id[0]);
    assert(TerminalTreeView_nodeAtOffset(view, 1) == TERMINAL_TREE_NO_NODE);

    // --- A LEAF that gains a child is promoted AND opened ---
    // The asymmetry with a declared folder, kept in its own view so it cannot be confused
    // with the reveal below: this node was never declared foldable, so leaving it folded
    // would hide the row that was just added.
    {
        TerminalTreeView *promote = TerminalTreeView();
        TerminalTreeNodeId top = TerminalTreeView_addChild(promote, TERMINAL_TREE_NO_NODE, "top", true);
        TerminalTreeNodeId wasLeaf = TerminalTreeView_addChild(promote, top, "was-leaf", false);
        assert(!TerminalTreeView_isFolder(promote, wasLeaf));
        assert(TerminalTreeView_setExpanded(promote, top, true));
        assert(TerminalTreeView_visibleCount(promote) == 2u);
        TerminalTreeNodeId kid = TerminalTreeView_addChild(promote, wasLeaf, "kid", false);
        assert(TerminalTreeView_isFolder(promote, wasLeaf));
        assert(TerminalTreeView_isExpanded(promote, wasLeaf));
        assert(TerminalTreeView_visibleCount(promote) == 3u);      // top, was-leaf, kid
        assert(TerminalTreeView_rowOf(promote, kid) == 2u);
        assert(TerminalTreeView_depthOf(promote, kid) == 2u);
        TerminalTreeView_free(promote);
    }

    // --- Opening the path to one file, which is what "reveal the open file" is ---
    assert(TerminalTreeView_reveal(view, id[6]));
    assert(TerminalTreeView_visibleCount(view) == 8u);

    // --- The pre-order invariant, checked from the rows rather than from the array ---
    for (bytes row = 0; row + 1u < TerminalTreeView_visibleCount(view); ++row) {
        TerminalTreeNodeId node = TerminalTreeView_rowAt(view, row).id;
        assert(TerminalTreeView_subtreeCountOf(view, node) == countedDescendants(view, row) + 1u);
        assert(TerminalTreeView_childCountOf(view, node) == countedChildren(view, row));
    }
    assert(TerminalTreeView_subtreeCountOf(view, id[0]) == 8u);
    assert(TerminalTreeView_subtreeCountOf(view, id[4]) == 3u);   // java, nested, deep.c
    assert(TerminalTreeView_subtreeCountOf(view, id[5]) == 2u);   // nested, deep.c
    assert(TerminalTreeView_subtreeCountOf(view, id[7]) == 1u);   // a leaf is its own subtree
    assert(TerminalTreeView_childCountOf(view, id[0]) == 2u);     // src, whatever.c
    assert(TerminalTreeView_childCountOf(view, id[1]) == 3u);     // json, txt, java
    assert(TerminalTreeView_childCountOf(view, id[6]) == 0u);
    assert(TerminalTreeView_childCountOf(view, 999u) == 0u);
    assert(TerminalTreeView_subtreeCountOf(view, 999u) == 0u);

    // --- Depths and row order describe that same tree ---
    assert(TerminalTreeView_depthOf(view, id[0]) == 0u);
    assert(TerminalTreeView_depthOf(view, id[1]) == 1u);
    assert(TerminalTreeView_depthOf(view, id[2]) == 2u);
    assert(TerminalTreeView_depthOf(view, id[5]) == 3u);
    assert(TerminalTreeView_depthOf(view, id[6]) == 4u);
    assert(TerminalTreeView_depthOf(view, id[7]) == 1u);
    assert(TerminalTreeView_depthOf(view, 999u) == 0u);          // unknown reads as the top
    static const char *expected[] = { "vexgraph", "src", "opencode.json", "hello.txt",
                                      "example.java", "nested", "deep.c", "whatever.c" };
    static const bytes expectedDepths[] = { 0, 1, 2, 2, 2, 3, 4, 1 };
    for (bytes row = 0; row < 8; ++row) {
        TerminalTreeRow record = TerminalTreeView_rowAt(view, row);
        assert(record.label != nullptr && strcmp(record.label, expected[row]) == 0);
        assert(record.depth == expectedDepths[row]);
        assert(TerminalTreeView_rowOf(view, record.id) == row);
        assert(TerminalTreeView_isNode(view, record.id));
        assert(record.folder == TerminalTreeView_isFolder(view, record.id));
    }
    RowOrder expanded = captureOrder(view);

    // --- Folding hides exactly one subtree, and reorders nothing ---
    assert(TerminalTreeView_setExpanded(view, id[1], false));
    assert(!TerminalTreeView_isExpanded(view, id[1]));
    // The folded node keeps its OWN row and loses its subtree: vexgraph, src, whatever.c.
    assert(TerminalTreeView_visibleCount(view) == 3u);
    assert(TerminalTreeView_rowOf(view, id[1]) != TERMINAL_TREE_NO_ROW);
    assert(isSubsequenceOf(view, expanded));
    assert(TerminalTreeView_rowOf(view, id[2]) == TERMINAL_TREE_NO_ROW);
    assert(TerminalTreeView_isNode(view, id[2]));                // hidden, not gone
    assert(TerminalTreeView_getLabel(view, id[2]) != nullptr);
    assert(TerminalTreeView_subtreeCountOf(view, id[1]) == 6u);  // the subtree is untouched
    // A leaf has nothing to fold, and folding one is not an error.
    assert(TerminalTreeView_setExpanded(view, id[7], true));
    assert(TerminalTreeView_visibleCount(view) == 3u);
    // Folding an ancestor hides a folded child's rows too, without touching its state.
    assert(TerminalTreeView_setExpanded(view, id[0], false));
    assert(TerminalTreeView_visibleCount(view) == 1u);
    assert(TerminalTreeView_rowOf(view, id[7]) == TERMINAL_TREE_NO_ROW);
    assert(!TerminalTreeView_isExpanded(view, id[1]));           // still folded underneath
    assert(isSubsequenceOf(view, expanded));

    // --- reveal walks the ancestors open ---
    assert(TerminalTreeView_reveal(view, id[6]));
    assert(TerminalTreeView_isExpanded(view, id[0]));
    assert(TerminalTreeView_isExpanded(view, id[1]));
    assert(TerminalTreeView_isExpanded(view, id[4]));
    assert(TerminalTreeView_isExpanded(view, id[5]));
    assert(TerminalTreeView_visibleCount(view) == 8u);
    assert(TerminalTreeView_rowOf(view, id[6]) == 6u);
    for (bytes row = 0; row + 1u < TerminalTreeView_visibleCount(view); ++row) {
        TerminalTreeNodeId node = TerminalTreeView_rowAt(view, row).id;
        assert(TerminalTreeView_subtreeCountOf(view, node) == countedDescendants(view, row) + 1u);
    }
    assert(!TerminalTreeView_reveal(view, 999u));                // unknown is refused
    assert(!TerminalTreeView_toggle(view, 999u));
    assert(TerminalTreeView_visibleCount(view) == 8u);

    // toggle reports the state it left behind.
    assert(!TerminalTreeView_toggle(view, id[4]));               // java folds
    assert(TerminalTreeView_visibleCount(view) == 6u);           // nested and deep.c hide
    assert(TerminalTreeView_toggle(view, id[4]));                // and unfolds
    assert(TerminalTreeView_visibleCount(view) == 8u);

    // --- Selection ---
    assert(TerminalTreeView_getSelected(view) == TERMINAL_TREE_NO_NODE);
    assert(TerminalTreeView_setSelected(view, id[3]));
    assert(TerminalTreeView_isSelected(view, id[3]));
    assert(!TerminalTreeView_isSelected(view, id[2]));
    assert(TerminalTreeView_rowAt(view, 3).selected);
    assert(!TerminalTreeView_rowAt(view, 2).selected);
    assert(!TerminalTreeView_setSelected(view, 999u));           // unknown refused...
    assert(TerminalTreeView_getSelected(view) == id[3]);         // ...and the old one stands
    // A selection folded out of sight is still the selection. It is simply not shown.
    assert(TerminalTreeView_setExpanded(view, id[1], false));
    assert(TerminalTreeView_getSelected(view) == id[3]);
    assert(TerminalTreeView_rowOf(view, id[3]) == TERMINAL_TREE_NO_ROW);
    assert(TerminalTreeView_setExpanded(view, id[1], true));
    assert(TerminalTreeView_setSelected(view, TERMINAL_TREE_NO_NODE));
    assert(TerminalTreeView_getSelected(view) == TERMINAL_TREE_NO_NODE);
    assert(!TerminalTreeView_rowAt(view, 3).selected);
    assert(TerminalTreeView_setSelected(view, id[3]));

    // --- The viewport, and the click that lands past the last row ---
    assert(TerminalTreeView_setViewportRows(view, 3));
    assert(TerminalTreeView_maxScrollRow(view) == 5u);           // 8 rows, 3 shown
    assert(TerminalTreeView_nodeAtOffset(view, 0) == id[0]);
    assert(TerminalTreeView_nodeAtOffset(view, 2) == id[2]);
    assert(TerminalTreeView_nodeAtOffset(view, 3) == TERMINAL_TREE_NO_NODE);  // == viewport
    assert(TerminalTreeView_setScrollRow(view, 4) == 4u);
    assert(TerminalTreeView_nodeAtOffset(view, 0) == id[4]);
    assert(TerminalTreeView_nodeAtOffset(view, 2) == id[6]);
    assert(TerminalTreeView_nodeAtOffset(view, 3) == TERMINAL_TREE_NO_NODE);
    assert(TerminalTreeView_setScrollRow(view, 5) == 5u);
    assert(TerminalTreeView_nodeAtOffset(view, 0) == id[5]);
    assert(TerminalTreeView_nodeAtOffset(view, 2) == id[7]);
    assert(TerminalTreeView_nodeAtOffset(view, 3) == TERMINAL_TREE_NO_NODE);
    // Past the end is CLAMPED, not stored: an offset past the tree paints an empty pane
    // and reads as data loss rather than as a scroll.
    assert(TerminalTreeView_setScrollRow(view, 999u) == 5u);
    assert(TerminalTreeView_scrollRow(view) == 5u);
    assert(TerminalTreeView_scrollBy(view, 100) == 5u);
    assert(TerminalTreeView_scrollBy(view, -100) == 0u);
    assert(TerminalTreeView_scrollBy(view, -1) == 0u);           // no unsigned underflow
    assert(TerminalTreeView_scrollBy(view, 3) == 3u);
    // A viewport that already shows everything has nowhere to scroll, and resizing the
    // viewport clamps what was stored.
    assert(TerminalTreeView_setViewportRows(view, 99));
    assert(TerminalTreeView_maxScrollRow(view) == 0u);
    assert(TerminalTreeView_scrollRow(view) == 0u);
    assert(TerminalTreeView_nodeAtOffset(view, 7) == id[7]);
    assert(TerminalTreeView_nodeAtOffset(view, 8) == TERMINAL_TREE_NO_NODE);
    assert(TerminalTreeView_setViewportRows(view, 3));
    assert(TerminalTreeView_setScrollRow(view, 5) == 5u);
    // Folding the tree underneath the viewport clamps it AGAIN. This is the bug the whole
    // clamp exists for, and it is reachable by a user in one click.
    assert(TerminalTreeView_setExpanded(view, id[0], false));
    assert(TerminalTreeView_visibleCount(view) == 1u);
    assert(TerminalTreeView_scrollRow(view) == 0u);
    assert(TerminalTreeView_setExpanded(view, id[0], true));
    assert(TerminalTreeView_visibleCount(view) == 8u);

    // --- Removing a subtree ---
    assert(!TerminalTreeView_remove(view, 999u));
    assert(TerminalTreeView_nodeCount(view) == 8u);
    assert(TerminalTreeView_setSelected(view, id[6]));
    assert(TerminalTreeView_remove(view, id[4]));                 // java, nested, deep.c
    assert(TerminalTreeView_nodeCount(view) == 5u);
    assert(TerminalTreeView_visibleCount(view) == 5u);
    assert(!TerminalTreeView_isNode(view, id[4]));
    assert(!TerminalTreeView_isNode(view, id[5]));
    assert(!TerminalTreeView_isNode(view, id[6]));
    // The selection went away WITH the node, rather than surviving as a dangling id.
    assert(TerminalTreeView_getSelected(view) == TERMINAL_TREE_NO_NODE);
    static const char *surviving[] = { "vexgraph", "src", "opencode.json", "hello.txt", "whatever.c" };
    for (bytes row = 0; row < 5; ++row) {
        assert(strcmp(TerminalTreeView_rowAt(view, row).label, surviving[row]) == 0);
        TerminalTreeNodeId node = TerminalTreeView_rowAt(view, row).id;
        assert(TerminalTreeView_subtreeCountOf(view, node) == countedDescendants(view, row) + 1u);
        assert(TerminalTreeView_childCountOf(view, node) == countedChildren(view, row));
    }
    assert(TerminalTreeView_depthOf(view, id[7]) == 1u);         // the survivors kept their shape
    assert(TerminalTreeView_subtreeCountOf(view, id[0]) == 5u);
    assert(TerminalTreeView_childCountOf(view, id[0]) == 2u);
    assert(TerminalTreeView_childCountOf(view, id[1]) == 2u);    // json, hello
    // Ids are never reused, so the next node gets a NEW one and the freed ones stay dead.
    TerminalTreeNodeId inserted = TerminalTreeView_addChild(view, id[1], "replacement.c", false);
    assert(inserted > id[7]);
    assert(!TerminalTreeView_isNode(view, id[4]));
    assert(TerminalTreeView_isNode(view, inserted));
    assert(TerminalTreeView_nodeCount(view) == 6u);

    // --- Growth across the doubling boundary keeps the layout intact ---
    // The default capacity holds 16 nodes, so this crosses two doublings with a deep,
    // interleaved tree — the case where a pre-order memmove or a missed ancestor update
    // would put a row in the wrong place.
    TerminalTreeNodeId branch = TerminalTreeView_addChild(view, id[0], "many", true);
    for (bytes index = 0; index < 40; ++index) {
        TerminalTreeNodeId leaf = TerminalTreeView_addChild(view, branch, "leaf", false);
        assert(leaf != TERMINAL_TREE_NO_NODE);
        assert(TerminalTreeView_depthOf(view, leaf) == 2u);
        assert(TerminalTreeView_subtreeCountOf(view, leaf) == 1u);
    }
    assert(TerminalTreeView_childCountOf(view, branch) == 40u);
    assert(TerminalTreeView_subtreeCountOf(view, branch) == 41u);
    assert(TerminalTreeView_nodeCount(view) == 47u);             // 6 + many + 40
    assert(TerminalTreeView_capacity(view) >= 47u);
    // The 40 leaves went into a DECLARED folder, so they are built but not shown: the row
    // count is the tree above them, and opening the branch is what reveals them.
    // `many` is vexgraph's LAST child, so it lands after whatever.c in pre-order.
    assert(TerminalTreeView_visibleCount(view) == 7u);
    assert(TerminalTreeView_rowOf(view, branch) == 6u);
    assert(TerminalTreeView_rowOf(view, id[7]) == 5u);
    assert(strcmp(TerminalTreeView_rowAt(view, 5).label, "whatever.c") == 0);
    assert(TerminalTreeView_setExpanded(view, branch, true));
    assert(TerminalTreeView_visibleCount(view) == 47u);
    for (bytes row = 0; row + 1u < TerminalTreeView_visibleCount(view); ++row) {
        TerminalTreeNodeId node = TerminalTreeView_rowAt(view, row).id;
        assert(TerminalTreeView_subtreeCountOf(view, node) == countedDescendants(view, row) + 1u);
    }
    // Folding the branch hides exactly its 40 descendants, and nothing AROUND it moves:
    // the rows before it keep their indices, which is the pre-order layout doing its job.
    assert(TerminalTreeView_setExpanded(view, branch, false));
    assert(TerminalTreeView_visibleCount(view) == 7u);
    assert(TerminalTreeView_rowOf(view, branch) == 6u);
    assert(TerminalTreeView_rowOf(view, id[7]) == 5u);
    assert(strcmp(TerminalTreeView_rowAt(view, 5).label, "whatever.c") == 0);
    assert(TerminalTreeView_setExpanded(view, branch, true));
    assert(TerminalTreeView_visibleCount(view) == 47u);
    assert(strcmp(TerminalTreeView_rowAt(view, 46).label, "leaf") == 0);   // the last child
    assert(TerminalTreeView_rowOf(view, id[7]) == 5u);

    // --- A fold fuzz: every fold state is a subsequence of the expanded order ---
    // Deterministic, so a failure names a seed that reproduces it exactly.
    {
        u32 seed = 0x5eed1u;
        u32 state = seed;
        RowOrder reference = captureOrder(view);
        TerminalTreeNodeId folders[] = { id[0], id[1], branch };
        for (bytes round = 0; round < 200; ++round) {
            TerminalTreeNodeId target = folders[lcg(&state) % 3u];
            bool want = (lcg(&state) & 1u) != 0u;
            assert(TerminalTreeView_setExpanded(view, target, want));
            assert(isSubsequenceOf(view, reference));
            assert(TerminalTreeView_visibleCount(view) <= TerminalTreeView_nodeCount(view));
            assert(TerminalTreeView_scrollRow(view) <= TerminalTreeView_maxScrollRow(view));
            // Whatever is folded, the ROOT is a row: nothing above it can hide it.
            assert(TerminalTreeView_rowOf(view, id[0]) != TERMINAL_TREE_NO_ROW);
            // Every visible row is a real node, and every offset inside the viewport
            // resolves to one of them or to nothing at all.
            for (bytes offset = 0; offset < TerminalTreeView_viewportRows(view); ++offset) {
                TerminalTreeNodeId hit = TerminalTreeView_nodeAtOffset(view, offset);
                if (hit == TERMINAL_TREE_NO_NODE)
                    continue;
                assert(TerminalTreeView_isNode(view, hit));
                assert(TerminalTreeView_rowOf(view, hit)
                       == TerminalTreeView_scrollRow(view) + offset);
            }
        }
        assert(TerminalTreeView_setExpanded(view, id[0], true));
        assert(TerminalTreeView_setExpanded(view, id[1], true));
        assert(TerminalTreeView_setExpanded(view, branch, true));
    }

    // --- clear keeps the storage and rewinds every offset ---
    {
        bytes capacity = TerminalTreeView_capacity(view);
        assert(capacity > 0u);
        assert(TerminalTreeView_setSelected(view, id[1]));
        assert(TerminalTreeView_setScrollRow(view, 10) == 10u);
        TerminalTreeView_clear(view);
        assert(TerminalTreeView_nodeCount(view) == 0u);
        assert(TerminalTreeView_visibleCount(view) == 0u);
        assert(TerminalTreeView_capacity(view) == capacity);     // the storage stayed
        assert(TerminalTreeView_getSelected(view) == TERMINAL_TREE_NO_NODE);
        assert(TerminalTreeView_scrollRow(view) == 0u);
        assert(TerminalTreeView_nodeAtOffset(view, 0) == TERMINAL_TREE_NO_NODE);
        assert(TerminalTreeView_rowOf(view, id[0]) == TERMINAL_TREE_NO_ROW);
        assert(!TerminalTreeView_isNode(view, id[0]));
        // The tree is refillable, and the ids it hands out are NEW ones: a caller still
        // holding an old id finds nothing rather than somebody else's row.
        TerminalTreeNodeId refilled = TerminalTreeView_addChild(view, TERMINAL_TREE_NO_NODE,
                                                                "second", true);
        assert(refilled > inserted);
        assert(!TerminalTreeView_isNode(view, id[0]));
        assert(TerminalTreeView_isNode(view, refilled));
        assert(TerminalTreeView_visibleCount(view) == 1u);
        assert(TerminalTreeView_nodeAtOffset(view, 0) == refilled);
        // setLabel re-points a label without disturbing structure or selection.
        assert(TerminalTreeView_setSelected(view, refilled));
        assert(TerminalTreeView_setLabel(view, refilled, "renamed"));
        assert(strcmp(TerminalTreeView_getLabel(view, refilled), "renamed") == 0);
        assert(strcmp(TerminalTreeView_rowAt(view, 0).label, "renamed") == 0);
        assert(TerminalTreeView_visibleCount(view) == 1u);
        assert(TerminalTreeView_getSelected(view) == refilled);
        assert(!TerminalTreeView_setLabel(view, refilled, nullptr));
        assert(!TerminalTreeView_setLabel(view, 999u, "x"));
        assert(strcmp(TerminalTreeView_getLabel(view, refilled), "renamed") == 0);
        // And a rebuilt tree with no folds still matches its own rows.
        TerminalTreeNodeId child = TerminalTreeView_addChild(view, refilled, "kid", false);
        assert(child != TERMINAL_TREE_NO_NODE);
        assert(TerminalTreeView_visibleCount(view) == 1u);   // a declared folder, collapsed
        assert(TerminalTreeView_setExpanded(view, refilled, true));
        assert(TerminalTreeView_visibleCount(view) == 2u);
        assert(TerminalTreeView_depthOf(view, child) == 1u);
        assert(TerminalTreeView_subtreeCountOf(view, refilled) == 2u);
        assert(TerminalTreeView_childCountOf(view, refilled) == 1u);
        assert(TerminalTreeView_subtreeCountOf(view, refilled)
               == countedDescendants(view, 0) + 1u);
    }

    // --- Refusing an unknown id is a safe default, NOT a diagnostic ---
    // A stray click on empty space, a stale id after a refresh, a row that folded away
    // under a pointer: all ordinary, all silent. Only a resource running out reports, and
    // it reports once per epoch.
    beginDiagnostics();
    assert(!TerminalTreeView_setSelected(view, 999u));
    assert(!TerminalTreeView_setExpanded(view, 999u, true));
    assert(!TerminalTreeView_setLabel(view, 999u, "x"));
    assert(!TerminalTreeView_remove(view, 999u));
    assert(!TerminalTreeView_reveal(view, 999u));
    assert(!TerminalTreeView_toggle(view, 999u));
    assert(TerminalTreeView_addChild(view, 999u, "x", false) == TERMINAL_TREE_NO_NODE);
    assert(TerminalTreeView_rowAt(view, 9999u).id == TERMINAL_TREE_NO_NODE);
    assert(TerminalTreeView_nodeAtOffset(view, 9999u) == TERMINAL_TREE_NO_NODE);
    assert(!TerminalTreeView_isNode(view, 999u));
    endDiagnostics(0, nullptr);

    // --- Projections ---
    TERMINAL_PROJECT(TerminalTreeView, view);
    // The struct dump mirrors the STRUCT FIELDS block: every field appears by name.
    TerminalProjection dump = TerminalTreeView_toStringStruct(view);
    static const char *fields[] = { "nodes", "nodeCount", "nodeCapacity", "rows", "visibleCount",
                                    "rowsCapacity", "nextId", "selected", "scrollRow",
                                    "viewportRows", "nodeRefusals", "rowRefusals" };
    for (bytes index = 0; index < sizeof fields / sizeof fields[0]; ++index)
        assert(strstr(dump.text, fields[index]) != nullptr);
    assert(TerminalTreeView_exhaustionCount(view) == 0u);        // nothing ran out
    TerminalTreeView_clearExhaustion(view);
    assert(TerminalTreeView_exhaustionCount(view) == 0u);

    // --- Indent, icon configuration and uniform paint ---
    assert(TerminalTreeView_getIndent(view) == 2u);
    assert(TerminalTreeView_setIndent(view, 4u));
    assert(TerminalTreeView_getIndent(view) == 4u);
    assert(TerminalTreeView_getNerdIcons(view));
    assert(TerminalTreeView_setNerdIcons(view, false));
    assert(!TerminalTreeView_getNerdIcons(view));

    u32 cells[20 * 5];
    for (bytes i = 0; i < 20 * 5; ++i)
        cells[i] = ' ';
    TerminalCanvas canvas = TerminalCanvas_4(cells, nullptr, 20, 5);
    TerminalTreeView_paint(view, &canvas);
    TerminalTreeView_setNerdIcons(view, true);
    TerminalTreeView_paint(view, &canvas);

    TerminalTreeView_free(view);
    return 0;
}
