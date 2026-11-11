#ifndef DETERMINANT_SCANNER_H
#define DETERMINANT_SCANNER_H
#include "util/constructor.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
typedef struct Scanner {
    char *bytes;
    bytes capacity;
    bytes length;
    bytes position;
    bytes refusalCount;
    bool ended;
} Scanner;
Scanner Scanner_0(void);
Scanner Scanner_2(char *bytes, bytes capacity);
#define Scanner(...) CONSTRUCTOR_DISPATCH(Scanner, __VA_ARGS__)
#define Scanner_zero() Scanner_0()
// Append whole span or reject unchanged. EOF is explicitly caller-fed. No stdin.
bool Scanner_feed(Scanner *self, const char *bytes, bytes length, bool ended);
bool Scanner_reserve(Scanner *self, char *bytes, bytes capacity);
bool Scanner_hasNextLine(const Scanner *self);
// Returned live R2 string belongs to caller; pending and EOF both return null.
u8 *Scanner_nextLine(Scanner *self);
u8 *Scanner_nextWord(Scanner *self);
void Scanner_reset(Scanner *self);
bytes Scanner_getLength(const Scanner *self);
bytes Scanner_getCapacity(const Scanner *self);
bytes Scanner_getPosition(const Scanner *self);
bool Scanner_isEnded(const Scanner *self);
bytes Scanner_exhaustionCount(const Scanner *self);
const char *Scanner_getBytes(const Scanner *self);
bool Scanner_toString(const Scanner *self, char *dest, bytes cap, bool *outTruncated);
bool Scanner_toStringStruct(const Scanner *self, char *dest, bytes cap, bool *outTruncated);
#endif
