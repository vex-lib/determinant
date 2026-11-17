#include "vex.h"
#include "ui/field/terminal_input_line.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A generic single-line input over a caller-owned buffer: a prefix, the typed
 * text, and add/backspace/clear editing. It submits nothing and owns no storage
 * — overload is refused loudly at the buffer capacity. The application reads the
 * buffer on Enter and decides what a submission means. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalInputLine (ui/field/terminal_input_line.c)
 * STRUCT FIELDS (Mirroring ui/field/terminal_input_line.h)
 * char *buffer             borrowed storage
 * bytes capacity          usable bytes (index [length] stays NUL)
 * bytes length            current text length
 * const char *prefix       prompt prefix (default "> ")
 * TerminalPen prefixPen, textPen  pens
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalInputLine_0(), TerminalInputLine_2(buffer, capacity)
 * Config: bind/getBuffer/getCapacity/isBound, set/getPrefix, set/getPens
 * Editing: bool add(self, ch), bool backspace(self), fn clear(self)
 * Queries: bytes TerminalInputLine_length(self)
 * Render: TerminalInputLine_render(self, canvas)
 * Projections: TerminalProjection TerminalInputLine_toString/toStringStruct(self)
 * CONTRACT: null/unbound operations reject; add refuses (loud, counted by the
 * caller via the false result) at capacity and on non-printable input; render
 * clips and never allocates or logs.
 * ======================================================================== */


;;GETTER
static inline char * TerminalInputLine_buffer(const TerminalInputLine *s) { return (*s).buffer; }
;;GETTER
static inline char * *TerminalInputLine_bufferMut(TerminalInputLine *s) { return &(*s).buffer; }
;;SETTER
static inline fn TerminalInputLine_setBuffer(TerminalInputLine *s, char * v) { (*s).buffer = v; }
;;GETTER
static inline bytes TerminalInputLine_capacity(const TerminalInputLine *s) { return (*s).capacity; }
;;GETTER
static inline bytes *TerminalInputLine_capacityMut(TerminalInputLine *s) { return &(*s).capacity; }
;;SETTER
static inline fn TerminalInputLine_setCapacity(TerminalInputLine *s, bytes v) { (*s).capacity = v; }
;;GETTER
static inline bytes TerminalInputLine_length(const TerminalInputLine *s) { return (*s).length; }
;;GETTER
static inline bytes *TerminalInputLine_lengthMut(TerminalInputLine *s) { return &(*s).length; }
;;SETTER
static inline fn TerminalInputLine_setLength(TerminalInputLine *s, bytes v) { (*s).length = v; }
;;GETTER
static inline const char * TerminalInputLine_prefix(const TerminalInputLine *s) { return (*s).prefix; }
;;GETTER
static inline const char * *TerminalInputLine_prefixMut(TerminalInputLine *s) { return &(*s).prefix; }
;;SETTER
static inline fn TerminalInputLine_setPrefix(TerminalInputLine *s, const char * v) { (*s).prefix = v; }
;;GETTER
static inline TerminalPen TerminalInputLine_prefixPen(const TerminalInputLine *s) { return (*s).prefixPen; }
;;GETTER
static inline TerminalPen *TerminalInputLine_prefixPenMut(TerminalInputLine *s) { return &(*s).prefixPen; }
;;SETTER
static inline fn TerminalInputLine_setPrefixPen(TerminalInputLine *s, TerminalPen v) { (*s).prefixPen = v; }
;;GETTER
static inline TerminalPen TerminalInputLine_textPen(const TerminalInputLine *s) { return (*s).textPen; }
;;GETTER
static inline TerminalPen *TerminalInputLine_textPenMut(TerminalInputLine *s) { return &(*s).textPen; }
;;SETTER
static inline fn TerminalInputLine_setTextPen(TerminalInputLine *s, TerminalPen v) { (*s).textPen = v; }

TerminalInputLine TerminalInputLine_0() {
    TerminalInputLine line = {0};
    line.prefix = TERMINAL_INPUT_PREFIX_DEFAULT;
    line.prefixPen = TerminalPen_1(TERMINAL_STYLE_BOLD);
    line.textPen = TerminalPen_0();
    return line;
}

TerminalInputLine TerminalInputLine_2(char *buffer, bytes capacity) {
    TerminalInputLine line = TerminalInputLine_0();
    if (buffer != nullptr && capacity >= 2) {
        line.buffer = buffer;
        line.capacity = capacity;
        line.buffer[0] = '\0';
    }
    return line;
}

bool TerminalInputLine_bind(TerminalInputLine *self, char *buffer, bytes capacity) {
    if (self == nullptr || buffer == nullptr || capacity < 2) {
        THROW("TerminalInputLine bind: null or too small");
        return false;
    }
    TerminalInputLine_setBuffer(self, buffer);
    TerminalInputLine_setCapacity(self, capacity);
    TerminalInputLine_setLength(self, 0);
    buffer[0] = '\0';
    return true;
}
const char *TerminalInputLine_getBuffer(const TerminalInputLine *self) { return self ? TerminalInputLine_buffer(self) : nullptr; }
bytes TerminalInputLine_getCapacity(const TerminalInputLine *self) { return self ? TerminalInputLine_capacity(self) : 0; }
bytes TerminalInputLine_length(const TerminalInputLine *self) { return self ? TerminalInputLine_length(self) : 0; }
bool TerminalInputLine_isBound(const TerminalInputLine *self) { return self && TerminalInputLine_buffer(self) != nullptr; }
bool TerminalInputLine_setPrefix(TerminalInputLine *self, const char *prefix) {
    if (self == nullptr || prefix == nullptr) { THROW("TerminalInputLine prefix: null"); return false; }
    TerminalInputLine_setPrefix(self, prefix);
    return true;
}
const char *TerminalInputLine_getPrefix(const TerminalInputLine *self) {
    return (self && TerminalInputLine_prefix(self)) ? TerminalInputLine_prefix(self) : TERMINAL_INPUT_PREFIX_DEFAULT;
}
fn TerminalInputLine_setPens(TerminalInputLine *self, TerminalPen prefixPen, TerminalPen textPen) {
    if (self != nullptr) { (*TerminalInputLine_prefixPenMut(self)) = prefixPen; (*TerminalInputLine_textPenMut(self)) = textPen; }
}
TerminalInputLinePens TerminalInputLine_getPens(const TerminalInputLine *self) {
    return self ? (TerminalInputLinePens) { TerminalInputLine_prefixPen(self), TerminalInputLine_textPen(self) }
                : (TerminalInputLinePens) { TerminalPen_0(), TerminalPen_0() };
}

bool TerminalInputLine_add(TerminalInputLine *self, i32 character) {
    if (self == nullptr || TerminalInputLine_buffer(self) == nullptr || character < 32 || character >= 127) {
        THROW("TerminalInputLine add: unbound or non-printable");
        return false;
    }
    if (TerminalInputLine_length(self) + 1 >= TerminalInputLine_capacity(self)) {
        THROW("TerminalInputLine add: capacity refused");
        return false;
    }
    TerminalInputLine_buffer(self)[(*TerminalInputLine_lengthMut(self))++] = (char) character;
    TerminalInputLine_buffer(self)[TerminalInputLine_length(self)] = '\0';
    return true;
}
bool TerminalInputLine_backspace(TerminalInputLine *self) {
    if (self == nullptr || TerminalInputLine_buffer(self) == nullptr || TerminalInputLine_length(self) == 0)
        return false;
    TerminalInputLine_buffer(self)[--(*TerminalInputLine_lengthMut(self))] = '\0';
    return true;
}
fn TerminalInputLine_clear(TerminalInputLine *self) {
    if (self == nullptr || TerminalInputLine_buffer(self) == nullptr)
        return;
    TerminalInputLine_setLength(self, 0);
    TerminalInputLine_buffer(self)[0] = '\0';
}

fn TerminalInputLine_render(const TerminalInputLine *self, TerminalCanvas *canvas) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    const char *prefix = TerminalInputLine_getPrefix(self);
    bytes prefixLen = strlen(prefix);
    TerminalCanvas_text(canvas, 0, 0, prefix, prefixLen, TerminalInputLine_prefixPen(self));
    if (TerminalInputLine_buffer(self) != nullptr)
        TerminalCanvas_text(canvas, prefixLen, 0, TerminalInputLine_buffer(self), TerminalInputLine_length(self), TerminalInputLine_textPen(self));
}

TerminalProjection TerminalInputLine_toString(const TerminalInputLine *self) {
    return TerminalInputLine_toStringStruct(self);
}

TerminalProjection TerminalInputLine_toStringStruct(const TerminalInputLine *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalInputLine{bound=%d,length=%zu,capacity=%zu,prefix=\"%s\"}",
                                     TerminalInputLine_buffer(self) != nullptr, TerminalInputLine_length(self), TerminalInputLine_capacity(self),
                                     TerminalInputLine_getPrefix(self));
}
