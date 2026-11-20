#include "vex.h"
#include "re.h"
#include "cli/scanner.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** The host feeds finite ASCII spans into caller-owned backing. Scanner has
 * no blocking stdin, OS object, allocation while feeding, or hidden thread.
 * Cold reserve copies remaining input to larger backing; token extraction
 * allocates R2 strings only after a delimiter or explicit EOF is available. */
;;OVERVIEW
/** ========================================================================
 * CLASS: Scanner (cli/scanner.c)
 * STRUCT FIELDS (Mirroring cli/scanner.h)
 * char *bytes                 borrowed input backing
 * bytes capacity             physical admitted extent
 * bytes length               initialized input bytes
 * bytes position             consumed prefix
 * bytes refusalCount         epoch capacity refusals
 * bool ended                  explicit caller EOF
 * FUNCTION REGISTRY (exports)
 * Constructors: Scanner Scanner_0()
 *   Scanner Scanner_2(char *bytes, bytes capacity)
 * Admission: bool Scanner_feed(Scanner *self, const char *bytes, bytes length,
 *     bool ended)
 *   bool Scanner_reserve(Scanner *self, char *bytes, bytes capacity)
 * Queries: bool Scanner_hasNextLine(const Scanner *self)
 *   bool Scanner_isEnded(const Scanner *self)
 *   const char *Scanner_getBytes(const Scanner *self)
 *   bytes Scanner_getLength(const Scanner *self)
 *   bytes Scanner_getCapacity(const Scanner *self)
 *   bytes Scanner_getPosition(const Scanner *self)
 *   bytes Scanner_exhaustionCount(const Scanner *self)
 * Extraction: u8 *Scanner_nextLine(Scanner *self)
 *   u8 *Scanner_nextWord(Scanner *self)
 * Lifecycle: fn Scanner_reset(Scanner *self) retains backing, clears epoch
 * Projections: bool Scanner_toString(const Scanner *self, char *dest,
 *     bytes cap, bool *outTruncated)
 *   bool Scanner_toStringStruct(const Scanner *self, char *dest,
 *     bytes cap, bool *outTruncated)
 * PRIVATE BEHAVIOR: wordSpace ASCII token delimiter; take allocates exact token
 * and advances position only after success. No caller span retained by feed.
 * CONTRACT: single externally serialized owner; disjoint live spans for reserve
 * and feed. ASCII admitted before mutation; NUL/ESC/control/non-ASCII reject.
 * CR is whitespace and stripped only immediately before LF. Empty line yields
 * an empty string, unlike pending/EOF. Buffer never silently truncates. reserve
 * supports cold growth, reset supports recovery; no permanent initial ceiling.
 * ======================================================================== */

;;GETTER
static inline char * Scanner_bytes(const Scanner *s) { return (*s).bytes; }
;;GETTER
static inline char * *Scanner_bytesMut(Scanner *s) { return &(*s).bytes; }
;;SETTER
static inline fn Scanner_setBytes(Scanner *s, char * v) { (*s).bytes = v; }
;;GETTER
static inline bytes Scanner_capacity(const Scanner *s) { return (*s).capacity; }
;;GETTER
static inline bytes *Scanner_capacityMut(Scanner *s) { return &(*s).capacity; }
;;SETTER
static inline fn Scanner_setCapacity(Scanner *s, bytes v) { (*s).capacity = v; }
;;GETTER
static inline bool Scanner_ended(const Scanner *s) { return (*s).ended; }
;;GETTER
static inline bool *Scanner_endedMut(Scanner *s) { return &(*s).ended; }
;;SETTER
static inline fn Scanner_setEnded(Scanner *s, bool v) { (*s).ended = v; }
;;GETTER
static inline bytes Scanner_length(const Scanner *s) { return (*s).length; }
;;GETTER
static inline bytes *Scanner_lengthMut(Scanner *s) { return &(*s).length; }
;;SETTER
static inline fn Scanner_setLength(Scanner *s, bytes v) { (*s).length = v; }
;;GETTER
static inline bytes Scanner_position(const Scanner *s) { return (*s).position; }
;;GETTER
static inline bytes *Scanner_positionMut(Scanner *s) { return &(*s).position; }
;;SETTER
static inline fn Scanner_setPosition(Scanner *s, bytes v) { (*s).position = v; }
;;GETTER
static inline bytes Scanner_refusalCount(const Scanner *s) { return (*s).refusalCount; }
;;GETTER
static inline bytes *Scanner_refusalCountMut(Scanner *s) { return &(*s).refusalCount; }
;;SETTER
static inline fn Scanner_setRefusalCount(Scanner *s, bytes v) { (*s).refusalCount = v; }

Scanner Scanner_0() { return (Scanner) {0}; }
Scanner Scanner_2(char *data, bytes capacity) {
    if (data == nullptr && capacity != 0) {
        THROW("Scanner construct: null backing");
        return Scanner_0();
    }
    return (Scanner) {.bytes = data, .capacity = capacity};
}
bool Scanner_feed(Scanner *self, const char *data, bytes length, bool ended) {
    if (self == nullptr || Scanner_ended(self) || (data == nullptr && length != 0)) {
        THROW("Scanner feed: invalid span or ended");
        return false;
    }
    if (length > Scanner_capacity(self) - Scanner_length(self)) {
        if (Scanner_refusalCount(self) == 0)
            THROW("Scanner exhausted: request %zu bytes, used %zu/%zu", length, Scanner_length(self), Scanner_capacity(self));
        if (Scanner_refusalCount(self) != SIZE_MAX)
            Scanner_setRefusalCount(self, Scanner_refusalCount(self) + 1u);
        return false;
    }
    for (bytes i = 0; i < length; ++i) {
        u8 c = (u8) data[i];
        if (c == 0 || c >= 127 || (c < 32 && c != '\n' && c != '\r' && c != '\t')) {
            THROW("Scanner feed: invalid ASCII byte at %zu", i);
            return false;
        }
    }
    if (length != 0)
        memcpy(Scanner_bytes(self) + Scanner_length(self), data, length);
    Scanner_setLength(self, Scanner_length(self) + (length));
    Scanner_setEnded(self, ended);
    return true;
}
bool Scanner_reserve(Scanner *self, char *data, bytes capacity) {
    if (self == nullptr || data == nullptr || capacity < Scanner_capacity(self)
        || (data == Scanner_bytes(self) && capacity != Scanner_capacity(self))) {
        THROW("Scanner reserve: invalid backing");
        return false;
    }
    bytes pending = Scanner_length(self) - Scanner_position(self);
    if (pending != 0)
        memmove(data, Scanner_bytes(self) + Scanner_position(self), pending);
    Scanner_setBytes(self, data);
    Scanner_setCapacity(self, capacity);
    Scanner_setLength(self, pending);
    Scanner_setPosition(self, 0);
    return true;
}
bool Scanner_hasNextLine(const Scanner *self) {
    if (self == nullptr || Scanner_position(self) == Scanner_length(self))
        return false;
    return Scanner_ended(self) || memchr(Scanner_bytes(self) + Scanner_position(self), '\n', Scanner_length(self) - Scanner_position(self)) != nullptr;
}
static u8 *take(Scanner *self, bytes start, bytes end, bytes next) {
    if (end - start >= UINT32_MAX) {
        THROW("Scanner take: string length overflow");
        return nullptr;
    }
    u8 *result = string_allocateBytes((const u8*) Scanner_bytes(self) + start, end - start);
    if (result != nullptr)
        Scanner_setPosition(self, next);
    return result;
}
u8 *Scanner_nextLine(Scanner *self) {
    if (self == nullptr) {
        THROW("Scanner nextLine: null scanner");
        return nullptr;
    }
    if (!Scanner_hasNextLine(self))
        return nullptr;
    bytes start = Scanner_position(self);
    bytes end = start;
    while (end < Scanner_length(self) && Scanner_bytes(self)[end] != '\n')
        ++end;
    bytes next = end < Scanner_length(self) ? end + 1 : end;
    bytes stop = end;
    if (stop > start && Scanner_bytes(self)[stop - 1] == '\r')
        --stop;
    return take(self, start, stop, next);
}
static bool wordSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
u8 *Scanner_nextWord(Scanner *self) {
    if (self == nullptr) {
        THROW("Scanner nextWord: null scanner");
        return nullptr;
    }
    bytes start = Scanner_position(self);
    while (start < Scanner_length(self) && wordSpace(Scanner_bytes(self)[start]))
        ++start;
    bytes end = start;
    while (end < Scanner_length(self) && !wordSpace(Scanner_bytes(self)[end]))
        ++end;
    if (start == end || (end == Scanner_length(self) && !Scanner_ended(self)))
        return nullptr;
    return take(self, start, end, end);
}
fn Scanner_reset(Scanner *self) {
    if (self == nullptr)
        return;
    Scanner_setLength(self, 0);
    Scanner_setPosition(self, 0);
    Scanner_setRefusalCount(self, 0);
    Scanner_setEnded(self, false);
}
bytes Scanner_getLength(const Scanner *self) { return self ? Scanner_length(self) : 0; }
bytes Scanner_getCapacity(const Scanner *self) { return self ? Scanner_capacity(self) : 0; }
bytes Scanner_getPosition(const Scanner *self) { return self ? Scanner_position(self) : 0; }
bool Scanner_isEnded(const Scanner *self) { return self && Scanner_ended(self); }
bytes Scanner_exhaustionCount(const Scanner *self) { return self ? Scanner_refusalCount(self) : 0; }
const char *Scanner_getBytes(const Scanner *self) { return self ? Scanner_bytes(self) : nullptr; }
bool Scanner_toString(const Scanner *self, char *dest, bytes cap, bool *outTruncated) {
    return Scanner_toStringStruct(self, dest, cap, outTruncated);
}
bool Scanner_toStringStruct(const Scanner *self, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("Scanner projection: missing destination");
        return false;
    }
    i32 n = self ? snprintf(dest, cap, "Scanner{bytes=<borrowed>,capacity=%zu,length=%zu,position=%zu,refusalCount=%zu,ended=%d}",
        Scanner_capacity(self), Scanner_length(self), Scanner_position(self), Scanner_refusalCount(self), Scanner_ended(self))
        : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (bytes) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("Scanner projection: truncated at %zu bytes", cap);
    return !cut;
}
