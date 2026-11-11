#ifndef DETERMINANT_MARKDOWN_TEXT_H
#define DETERMINANT_MARKDOWN_TEXT_H

#include <stdbool.h>
#include <stddef.h>

// ui/markdown/markdown_text.h — terminal-cell measurement over borrowed bytes.
//
// The markdown owners all speak the same units: a byte span is measured in
// CELLS, never in bytes, because a CJK glyph occupies two cells and a combining
// mark occupies none. This is the one place that arithmetic lives, so the inline
// parser, the table model and the view cannot disagree about how wide a word is.
// Nothing here allocates or looks at a canvas; the functions are pure over the
// borrowed span.

// Display width of `length` bytes, decoding UTF-8 scalars.
bytes TerminalMarkdownText_cells(const char *text, bytes length);

// Bytes of `text` that fit in `cells` cells without splitting a scalar. A
// combining mark rides with the scalar before it. `usedCells` receives the width
// actually consumed (nullable).
bytes TerminalMarkdownText_bytesForCells(const char *text, bytes length, bytes cells,
                                  bytes *usedCells);

// Bytes of the first scalar, or 1 for malformed input.
bytes TerminalMarkdownText_scalarBytes(const char *text, bytes length);

// First byte offset of `needle` at or after `from`, or SIZE_MAX.
bytes TerminalMarkdownText_find(const char *text, bytes length, bytes from,
                         const char *needle, bytes needleLength);

// First byte offset of a run of exactly `run` copies of `glyph`, or SIZE_MAX.
bytes TerminalMarkdownText_findRun(const char *text, bytes length, bytes from, char glyph,
                            bytes run);

bool TerminalMarkdownText_isSpace(char character);
bool TerminalMarkdownText_isPunctuation(char character);

// Leading spaces/tabs, and the offset after the last non-space byte.
bytes TerminalMarkdownText_leadingSpaces(const char *text, bytes length);
bytes TerminalMarkdownText_trimRight(const char *text, bytes length);

// True when the span is empty or only spaces/tabs.
bool TerminalMarkdownText_blank(const char *text, bytes length);

#endif
