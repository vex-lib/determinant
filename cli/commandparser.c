#include "vex.h"
#include "cli/commandparser.h"
#include "primitive/vex-string.h"
#include "nio/mem.h"
#include "oop/oop.h"
#include <string.h>

;;DEFINITION
/** R3 cold whitespace tokenization over a caller-provided span. Complete byte
 * admission precedes allocation; each successful token is a real R2 string.
 * Partial construction releases every initialized token/directory on failure.
 * There is no shell syntax, stdin, execution or terminal output. */
;;OVERVIEW
/** ========================================================================
 * MODULE: CommandParser (cli/commandparser.c)
 * STATE: none; no retained caller span.
 * FUNCTION REGISTRY (exports)
 * Command *CommandParser_parse(const char *line)
 * Command *CommandParser_parseBytes(const char *line, bytes length)
 * CommandChain *CommandParser_parseChain(const char *line)
 * CommandChain *CommandParser_parseChainBytes(const char *line,
 *   bytes length)
 * PRIVATE HELPERS: bool space(unsigned char c), ASCII whitespace predicate;
 * static bool andToken(const char*,bytes,bytes), standalone && check;
 * static bool admitBytes(const char*,bytes,const char*), shared ASCII gate.
 * CONTRACT: NUL form requires a live terminated string; span form admits at
 * most UINT32_MAX-1 bytes (R2 string length bound), rejects NUL/non-ASCII and
 * non-whitespace controls. Empty/blank returns null quietly. Chain form
 * splits on standalone && tokens; any empty segment (leading, trailing or
 * consecutive &&) rejects the whole line and frees partial work. Allocation
 * failure is reported/counted by R2. Token bytes and pointer storage are all
 * owned; free returned Command/CommandChain before Memory teardown. Caller
 * externally serializes.
 * ======================================================================== */
static bool space(unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static bool andToken(const char *line, bytes start, bytes length) {
    return length == 2 && line[start] == '&' && line[start + 1] == '&';
}
static bool admitBytes(const char *line, bytes length, const char *what) {
    for (bytes i = 0; i < length; ++i) {
        unsigned char c = (unsigned char) line[i];
        if (c == 0 || c >= 127 || (c < 32 && !space(c))) {
            THROW("%s: invalid ASCII byte at %zu", what, i);
            return false;
        }
    }
    return true;
}
Command *CommandParser_parse(const char *line) {
    if (line == nullptr) {
        THROW("CommandParser parse: null line");
        return nullptr;
    }
    return CommandParser_parseBytes(line, strlen(line));
}
Command *CommandParser_parseBytes(const char *line, bytes length) {
    if ((line == nullptr && length != 0) || length >= UINT32_MAX) {
        THROW("CommandParser parse: null span or length overflow");
        return nullptr;
    }
    if (!admitBytes(line, length, "CommandParser parse"))
        return nullptr;
    bytes tokens = 0;
    bool inToken = false;
    for (bytes i = 0; i < length; ++i) {
        unsigned char c = (unsigned char) line[i];
        if (!space(c) && !inToken)
            ++tokens;
        inToken = !space(c);
    }
    if (tokens == 0)
        return nullptr;
    if (tokens > UINT32_MAX / sizeof(u8*)) {
        THROW("CommandParser parse: pointer directory overflow");
        return nullptr;
    }
    u8 **parts = Memory_alloc(TYPE_BYTE_ARRAY, tokens * sizeof(u8*));
    if (parts == nullptr)
        return nullptr;
    bytes built = 0;
    bytes i = 0;
    while (i < length) {
        while (i < length && space((unsigned char) line[i]))
            ++i;
        if (i == length)
            break;
        bytes start = i;
        while (i < length && !space((unsigned char) line[i]))
            ++i;
        parts[built] = string_allocateBytes((const u8*) line + start, i - start);
        if (parts[built] == nullptr)
            break;
        ++built;
    }
    Command *result = built == tokens ? Command_3(parts[0], parts + 1, tokens - 1) : nullptr;
    if (result == nullptr)
        for (bytes j = 0; j < built; ++j)
            string_free(parts[j]);
    Memory_free(parts);
    return result;
}
CommandChain *CommandParser_parseChain(const char *line) {
    if (line == nullptr) {
        THROW("CommandParser parseChain: null line");
        return nullptr;
    }
    return CommandParser_parseChainBytes(line, strlen(line));
}
CommandChain *CommandParser_parseChainBytes(const char *line, bytes length) {
    if ((line == nullptr && length != 0) || length >= UINT32_MAX) {
        THROW("CommandParser parseChain: null span or length overflow");
        return nullptr;
    }
    if (!admitBytes(line, length, "CommandParser parseChain"))
        return nullptr;
    bytes segments = 1;
    for (bytes i = 0; i < length;) {
        while (i < length && space((unsigned char) line[i]))
            ++i;
        bytes start = i;
        while (i < length && !space((unsigned char) line[i]))
            ++i;
        if (i - start == 2 && andToken(line, start, i - start))
            ++segments;
    }
    bytes firstToken = 0;
    while (firstToken < length && space((unsigned char) line[firstToken]))
        ++firstToken;
    if (segments > 1 && firstToken + 2 <= length && andToken(line, firstToken, 2)) {
        THROW("CommandParser parseChain: empty segment");
        return nullptr;
    }
    if (segments > UINT32_MAX / sizeof(CommandInfo*)) {
        THROW("CommandParser parseChain: pointer directory overflow");
        return nullptr;
    }
    CommandInfo **infos = segments > 0 ? Memory_alloc(TYPE_BYTE_ARRAY, segments * sizeof(CommandInfo*)) : nullptr;
    if (segments > 0 && infos == nullptr)
        return nullptr;
    bytes count = 0;
    bytes segStart = 0;
    for (bytes i = 0; i <= length; ++i) {
        bool split = false;
        bytes next = 0;
        if (i == length) {
            split = true;
            next = length;
        }
        else if (space((unsigned char) line[i])) {
            bytes s = i;
            while (s < length && space((unsigned char) line[s]))
                ++s;
            if (s + 1 < length && line[s] == '&' && line[s + 1] == '&') {
                bytes after = s + 2;
                if (after == length || space((unsigned char) line[after])) {
                    split = true;
                    next = after;
                }
            }
        }
        if (!split)
            continue;
        /* for a space-triggered split, i is position of space before &&; segment ends at i */
        bytes first = segStart;
        while (first < i && space((unsigned char) line[first]))
            ++first;
        if (first == i) {
            THROW("CommandParser parseChain: empty segment");
            for (bytes k = 0; k < count; ++k)
                CommandInfo_free(infos[k]);
            Memory_free(infos);
            return nullptr;
        }
        bytes segEnd = i;
        {
            bytes p = first;
            while (p < segEnd && !space((unsigned char) line[p]))
                ++p;
            if (p - first == 2 && andToken(line, first, 2)) {
                THROW("CommandParser parseChain: empty segment");
                for (bytes k = 0; k < count; ++k)
                    CommandInfo_free(infos[k]);
                Memory_free(infos);
                return nullptr;
            }
        }
        /* trailing &&: split at position after last token, next==length and rest is spaces */
        if (i < length) {
            bytes rest = next;
            while (rest < length && space((unsigned char) line[rest]))
                ++rest;
            if (rest == length) {
                THROW("CommandParser parseChain: empty segment");
                for (bytes k = 0; k < count; ++k)
                    CommandInfo_free(infos[k]);
                Memory_free(infos);
                /* need to parse current segment to own? no: return before building */
                return nullptr;
            }
        }
        Command *cmd = CommandParser_parseBytes(line + first, segEnd - first);
        if (cmd == nullptr) {
            for (bytes k = 0; k < count; ++k)
                CommandInfo_free(infos[k]);
            Memory_free(infos);
            return nullptr;
        }
        CommandInfo *info = CommandInfo_fromCommand(cmd);
        if (info == nullptr) {
            Command_free(cmd);
            for (bytes k = 0; k < count; ++k)
                CommandInfo_free(infos[k]);
            Memory_free(infos);
            return nullptr;
        }
        infos[count++] = info;
        if (i == length)
            break;
        /* after a split, next is position after && (which may be mid-token) */
        segStart = next;
        if (segStart > length) {
            THROW("CommandParser parseChain: empty segment");
            for (bytes k = 0; k < count; ++k)
                CommandInfo_free(infos[k]);
            Memory_free(infos);
            return nullptr;
        }
        /* next segment starts after &&; skip following spaces to next token */
        {
            bytes p = segStart;
            while (p < length && space((unsigned char) line[p]))
                ++p;
            if (p == length) {
                THROW("CommandParser parseChain: empty segment");
                for (bytes k = 0; k < count; ++k)
                    CommandInfo_free(infos[k]);
                Memory_free(infos);
                return nullptr;
            }
        }
        i = next;
        while (i < length && space((unsigned char) line[i]))
            ++i;
    }
    CommandChain *chain = count == segments ? CommandChain_2(infos, count) : nullptr;
    if (chain == nullptr)
        for (bytes k = 0; k < count; ++k)
            CommandInfo_free(infos[k]);
    Memory_free(infos);
    return chain;
}