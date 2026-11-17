#include "vex.h"
#include "ui/list/terminal_list_view.h"
#include "format/terminal_style.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A generic list: borrowed labels drawn one per row with a scroll offset and a
 * single selection marker. Meaningless on its own — channels, users, commands
 * and settings are all the same widget. Owns no memory; the caller supplies the
 * labels and the two pens. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalListView (ui/list/terminal_list_view.c)
 * STRUCT FIELDS (Mirroring ui/list/terminal_list_view.h)
 * const char *const *labels  borrowed labels
 * bytes count               label count
 * bytes selection           selected index
 * bytes scroll              first visible index
 * const char *marker         selected-row marker (default "> ")
 * TerminalPen normal, selected  row pens
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalListView_0(), TerminalListView_2(labels, count)
 * Config: set/getLabels, set/getSelection, set/getScroll, set/getMarker,
 *   set/getPens
 * Navigation: bool TerminalListView_move(self, delta, visibleRows)
 * Render: TerminalListView_render(self, canvas)
 * Projections: TerminalProjection TerminalListView_toString/toStringStruct(self)
 * CONTRACT: null/invalid canvas no-ops; rows past the region clip; a null label
 * renders blank; render never allocates or logs.
 * ======================================================================== */


;;GETTER
static inline bytes TerminalListView_count(const TerminalListView *s) { return (*s).count; }
;;GETTER
static inline bytes *TerminalListView_countMut(TerminalListView *s) { return &(*s).count; }
;;SETTER
static inline fn TerminalListView_setCount(TerminalListView *s, bytes v) { (*s).count = v; }
;;GETTER
static inline const char *const * TerminalListView_labels(const TerminalListView *s) { return (*s).labels; }
;;GETTER
static inline const char *const * *TerminalListView_labelsMut(TerminalListView *s) { return &(*s).labels; }
;;SETTER
static inline fn TerminalListView_setLabels(TerminalListView *s, const char *const * v) { (*s).labels = v; }
;;GETTER
static inline const char * TerminalListView_marker(const TerminalListView *s) { return (*s).marker; }
;;GETTER
static inline const char * *TerminalListView_markerMut(TerminalListView *s) { return &(*s).marker; }
;;SETTER
static inline fn TerminalListView_setMarker(TerminalListView *s, const char * v) { (*s).marker = v; }
;;GETTER
static inline TerminalPen TerminalListView_normal(const TerminalListView *s) { return (*s).normal; }
;;GETTER
static inline TerminalPen *TerminalListView_normalMut(TerminalListView *s) { return &(*s).normal; }
;;SETTER
static inline fn TerminalListView_setNormal(TerminalListView *s, TerminalPen v) { (*s).normal = v; }
;;GETTER
static inline bytes TerminalListView_scroll(const TerminalListView *s) { return (*s).scroll; }
;;GETTER
static inline bytes *TerminalListView_scrollMut(TerminalListView *s) { return &(*s).scroll; }
;;SETTER
static inline fn TerminalListView_setScroll(TerminalListView *s, bytes v) { (*s).scroll = v; }
;;GETTER
static inline TerminalPen TerminalListView_selected(const TerminalListView *s) { return (*s).selected; }
;;GETTER
static inline TerminalPen *TerminalListView_selectedMut(TerminalListView *s) { return &(*s).selected; }
;;SETTER
static inline fn TerminalListView_setSelected(TerminalListView *s, TerminalPen v) { (*s).selected = v; }
;;GETTER
static inline bytes TerminalListView_selection(const TerminalListView *s) { return (*s).selection; }
;;GETTER
static inline bytes *TerminalListView_selectionMut(TerminalListView *s) { return &(*s).selection; }
;;SETTER
static inline fn TerminalListView_setSelection(TerminalListView *s, bytes v) { (*s).selection = v; }

TerminalListView TerminalListView_0() {
    TerminalListView view = {0};
    view.marker = TERMINAL_LIST_MARKER_DEFAULT;
    view.normal = TerminalPen_0();
    view.selected = TerminalPen_1(TERMINAL_STYLE_REVERSE);
    return view;
}

TerminalListView TerminalListView_2(const char *const *labels, bytes count) {
    TerminalListView view = TerminalListView_0();
    view.labels = (count != 0) ? labels : nullptr;
    view.count = (labels != nullptr) ? count : 0;
    return view;
}

bool TerminalListView_setLabels(TerminalListView *self, const char *const *labels, bytes count) {
    if (self == nullptr || (count != 0 && labels == nullptr)) {
        THROW("TerminalListView labels: null list");
        return false;
    }
    TerminalListView_setLabels(self, labels);
    TerminalListView_setCount(self, count);
    return true;
}
TerminalListViewLabels TerminalListView_getLabels(const TerminalListView *self) {
    return self ? (TerminalListViewLabels) { TerminalListView_labels(self), TerminalListView_count(self) }
                : (TerminalListViewLabels) { nullptr, 0u };
}
bool TerminalListView_setSelection(TerminalListView *self, bytes selection) {
    if (self == nullptr) { THROW("TerminalListView selection: null"); return false; }
    TerminalListView_setSelection(self, selection);
    return true;
}
bytes TerminalListView_getSelection(const TerminalListView *self) { return self ? TerminalListView_selection(self) : 0; }
bool TerminalListView_setScroll(TerminalListView *self, bytes scroll) {
    if (self == nullptr) { THROW("TerminalListView scroll: null"); return false; }
    TerminalListView_setScroll(self, scroll);
    return true;
}
bytes TerminalListView_getScroll(const TerminalListView *self) { return self ? TerminalListView_scroll(self) : 0; }
bool TerminalListView_setMarker(TerminalListView *self, const char *marker) {
    if (self == nullptr) { THROW("TerminalListView marker: null"); return false; }
    TerminalListView_setMarker(self, marker);
    return true;
}
const char *TerminalListView_getMarker(const TerminalListView *self) {
    return (self && TerminalListView_marker(self)) ? TerminalListView_marker(self) : TERMINAL_LIST_MARKER_DEFAULT;
}
fn TerminalListView_setPens(TerminalListView *self, TerminalPen normal, TerminalPen selected) {
    if (self != nullptr) { (*TerminalListView_normalMut(self)) = normal; (*TerminalListView_selectedMut(self)) = selected; }
}
TerminalListViewPens TerminalListView_getPens(const TerminalListView *self) {
    return self ? (TerminalListViewPens) { TerminalListView_normal(self), TerminalListView_selected(self) }
                : (TerminalListViewPens) { TerminalPen_0(), TerminalPen_0() };
}

bool TerminalListView_move(TerminalListView *self, i64 delta, bytes visibleRows) {
    if (self == nullptr || TerminalListView_count(self) == 0)
        return false;
    bytes selection = TerminalListView_selection(self);
    if (delta < 0) {
        if (selection == 0)
            return false;
        selection -= (bytes) -delta;
    } else {
        bytes next = selection + (bytes) delta;
        if (next >= TerminalListView_count(self))
            next = TerminalListView_count(self) - 1;
        if (next == selection)
            return false;
        selection = next;
    }
    TerminalListView_setSelection(self, selection);
    if (visibleRows > 0) {
        if (selection < TerminalListView_scroll(self))
            TerminalListView_setScroll(self, selection);
        else if (selection >= TerminalListView_scroll(self) + visibleRows)
            TerminalListView_setScroll(self, selection - visibleRows + 1);
    }
    return true;
}

fn TerminalListView_render(const TerminalListView *self, TerminalCanvas *canvas) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    bytes rows = TerminalCanvas_getHeight(canvas);
    bytes markerLen = self ? strlen(TerminalListView_getMarker(self)) : 0;
    for (bytes row = 0; row < rows; ++row) {
        bytes index = TerminalListView_scroll(self) + row;
        if (index >= TerminalListView_count(self))
            break;
        bool selected = index == TerminalListView_selection(self);
        TerminalPen pen = selected ? TerminalListView_selected(self) : TerminalListView_normal(self);
        if (selected)
            TerminalCanvas_text(canvas, 0, row, TerminalListView_getMarker(self), markerLen, pen);
        const char *label = TerminalListView_labels(self)[index];
        if (label != nullptr)
            TerminalCanvas_text(canvas, markerLen, row, label, strlen(label), pen);
    }
}

TerminalProjection TerminalListView_toString(const TerminalListView *self) {
    return TerminalListView_toStringStruct(self);
}

TerminalProjection TerminalListView_toStringStruct(const TerminalListView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalListView{count=%zu,selection=%zu,scroll=%zu,marker=\"%s\"}",
        TerminalListView_count(self), TerminalListView_selection(self), TerminalListView_scroll(self), TerminalListView_getMarker(self));
}
