#include "cli/scanner.h"
#include "primitive/vex-string.h"
#include "exception/throw.h"
#include "annotation/annotation.h"
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
 * Constructors: Scanner Scanner_0(void)
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
 * Lifecycle: void Scanner_reset(Scanner *self) retains backing, clears epoch
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
Scanner Scanner_0(void) { return (Scanner) {0}; }
Scanner Scanner_2(char *bytes, bytes capacity) {
    if (bytes == nullptr && capacity != 0) {
        THROW("Scanner construct: null backing");
        return Scanner_0();
    }
    return (Scanner) {.bytes = bytes, .capacity = capacity};
}
bool Scanner_feed(Scanner *self, const char *bytes, bytes length, bool ended) {
    if (self == nullptr || (*self).ended || (bytes == nullptr && length != 0)) {
        THROW("Scanner feed: invalid span or ended");
        return false;
    }
    if (length > (*self).capacity - (*self).length) {
        if ((*self).refusalCount == 0)
            THROW("Scanner exhausted: request %zu bytes, used %zu/%zu", length, (*self).length, (*self).capacity);
        if ((*self).refusalCount != SIZE_MAX)
            ++(*self).refusalCount;
        return false;
    }
    for (bytes i = 0; i < length; ++i) {
        unsigned char c = (unsigned char) bytes[i];
        if (c == 0 || c >= 127 || (c < 32 && c != '\n' && c != '\r' && c != '\t')) {
            THROW("Scanner feed: invalid ASCII byte at %zu", i);
            return false;
        }
    }
    if (length != 0)
        memcpy((*self).bytes + (*self).length, bytes, length);
    (*self).length += length;
    (*self).ended = ended;
    return true;
}
bool Scanner_reserve(Scanner *self, char *bytes, bytes capacity) {
    if (self == nullptr || bytes == nullptr || capacity < (*self).capacity
        || (bytes == (*self).bytes && capacity != (*self).capacity)) {
        THROW("Scanner reserve: invalid backing");
        return false;
    }
    bytes pending = (*self).length - (*self).position;
    if (pending != 0)
        memmove(bytes, (*self).bytes + (*self).position, pending);
    (*self).bytes = bytes;
    (*self).capacity = capacity;
    (*self).length = pending;
    (*self).position = 0;
    return true;
}
bool Scanner_hasNextLine(const Scanner *self) {
    if (self == nullptr || (*self).position == (*self).length)
        return false;
    return (*self).ended || memchr((*self).bytes + (*self).position, '\n', (*self).length - (*self).position) != nullptr;
}
static u8 *take(Scanner *self, bytes start, bytes end, bytes next) {
    if (end - start >= UINT32_MAX) {
        THROW("Scanner take: string length overflow");
        return nullptr;
    }
    u8 *result = string_allocateBytes((const u8*) (*self).bytes + start, end - start);
    if (result != nullptr)
        (*self).position = next;
    return result;
}
u8 *Scanner_nextLine(Scanner *self) {
    if (self == nullptr) {
        THROW("Scanner nextLine: null scanner");
        return nullptr;
    }
    if (!Scanner_hasNextLine(self))
        return nullptr;
    bytes start = (*self).position;
    bytes end = start;
    while (end < (*self).length && (*self).bytes[end] != '\n')
        ++end;
    bytes next = end < (*self).length ? end + 1 : end;
    bytes stop = end;
    if (stop > start && (*self).bytes[stop - 1] == '\r')
        --stop;
    return take(self, start, stop, next);
}
static bool wordSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
u8 *Scanner_nextWord(Scanner *self) {
    if (self == nullptr) {
        THROW("Scanner nextWord: null scanner");
        return nullptr;
    }
    bytes start = (*self).position;
    while (start < (*self).length && wordSpace((*self).bytes[start]))
        ++start;
    bytes end = start;
    while (end < (*self).length && !wordSpace((*self).bytes[end]))
        ++end;
    if (start == end || (end == (*self).length && !(*self).ended))
        return nullptr;
    return take(self, start, end, end);
}
void Scanner_reset(Scanner *self) {
    if (self == nullptr)
        return;
    (*self).length = 0;
    (*self).position = 0;
    (*self).refusalCount = 0;
    (*self).ended = false;
}
bytes Scanner_getLength(const Scanner *self) { return self ? (*self).length : 0; }
bytes Scanner_getCapacity(const Scanner *self) { return self ? (*self).capacity : 0; }
bytes Scanner_getPosition(const Scanner *self) { return self ? (*self).position : 0; }
bool Scanner_isEnded(const Scanner *self) { return self && (*self).ended; }
bytes Scanner_exhaustionCount(const Scanner *self) { return self ? (*self).refusalCount : 0; }
const char *Scanner_getBytes(const Scanner *self) { return self ? (*self).bytes : nullptr; }
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
    int n = self ? snprintf(dest, cap, "Scanner{bytes=<borrowed>,capacity=%zu,length=%zu,position=%zu,refusalCount=%zu,ended=%d}",
        (*self).capacity, (*self).length, (*self).position, (*self).refusalCount, (*self).ended)
        : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (bytes) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("Scanner projection: truncated at %zu bytes", cap);
    return !cut;
}
