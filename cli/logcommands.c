#include "vex.h"
#include "cli/logcommands.h"
#include "io/file/file.h"
#include "io/logparser.h"
#include <stdio.h>

;;DEFINITION
/** Read-only R3 presentation of the existing R2 log format, always through a
 * borrowed host sink rather than stdout. Stable-file admission rejects incomplete
 * record extents. Rendering raw integer timestamps avoids the legacy formatter's
 * signed subtraction overflow on hostile timestamp pairs. No OS ownership moves. */
;;OVERVIEW
/** ========================================================================
 * MODULE: LogCommands (cli/logcommands.c)
 * PUBLIC REGISTRY
 * bool LogCommands_stat(const char *path, DeterminantOutputFn output,
 *     any hostContext)
 * bool LogCommands_cat(const char *path, i32 limit, DeterminantOutputFn output,
 *     any hostContext)
 * PRIVATE HELPERS: bool admit(path, output, outBytes); checks format and extent.
 * fn catHandler(context,kind,ts,v0,v1,v2,v3,v4); bounded per-record sink.
 * PRIVATE HELPERS: Cat (file-local behaviorless accumulator)
 * i64 shown             delivered record count
 * i64 total             parsed record count
 * i32 limit                 max records (<0 all)
 * bool failed               sticky sink/format failure
 * DeterminantOutputFn output borrowed callback
 * any hostContext         borrowed opaque sink state
 * CONTRACT: synchronous cold file reads may block, never hot/bounded teardown.
 * Externally serialize and keep file stable. Null/malformed extents reject
 * loudly; output/parse failures return false, preserve no misleading success.
 * Sink receives bounded literal text spans; no raw application printf. Cat
 * stops delivering after failure but parser continues (R2 has no cancel seam).
 * ======================================================================== */
typedef struct Cat {
    i64 shown;
    i64 total;
    i32 limit;
    bool failed;
    DeterminantOutputFn output;
    any hostContext;
} Cat;

;;GETTER
static inline bool Cat_failed(const Cat *s) { return (*s).failed; }
;;GETTER
static inline bool *Cat_failedMut(Cat *s) { return &(*s).failed; }
;;SETTER
static inline fn Cat_setFailed(Cat *s, bool v) { (*s).failed = v; }
;;GETTER
static inline any Cat_hostContext(const Cat *s) { return (*s).hostContext; }
;;GETTER
static inline any *Cat_hostContextMut(Cat *s) { return &(*s).hostContext; }
;;SETTER
static inline fn Cat_setHostContext(Cat *s, any v) { (*s).hostContext = v; }
;;GETTER
static inline i32 Cat_limit(const Cat *s) { return (*s).limit; }
;;GETTER
static inline i32 *Cat_limitMut(Cat *s) { return &(*s).limit; }
;;SETTER
static inline fn Cat_setLimit(Cat *s, i32 v) { (*s).limit = v; }
;;GETTER
static inline DeterminantOutputFn Cat_output(const Cat *s) { return (*s).output; }
;;GETTER
static inline DeterminantOutputFn *Cat_outputMut(Cat *s) { return &(*s).output; }
;;SETTER
static inline fn Cat_setOutput(Cat *s, DeterminantOutputFn v) { (*s).output = v; }
;;GETTER
static inline i64 Cat_shown(const Cat *s) { return (*s).shown; }
;;GETTER
static inline i64 *Cat_shownMut(Cat *s) { return &(*s).shown; }
;;SETTER
static inline fn Cat_setShown(Cat *s, i64 v) { (*s).shown = v; }
;;GETTER
static inline i64 Cat_total(const Cat *s) { return (*s).total; }
;;GETTER
static inline i64 *Cat_totalMut(Cat *s) { return &(*s).total; }
;;SETTER
static inline fn Cat_setTotal(Cat *s, i64 v) { (*s).total = v; }

static bool admit(const char *path, DeterminantOutputFn output, i64 *outBytes) {
    if (path == nullptr || output == nullptr) {
        THROW("LogCommands admission: null path or sink");
        return false;
    }
    if (!LogParser_isLogFile(path)) {
        THROW("LogCommands admission: invalid log");
        return false;
    }
    File *file = File_open(path, FILE_MODE_READ);
    i64 bytes = file ? File_size(file) : 0;
    if (file != nullptr)
        File_close(file);
    if (bytes < LOGPARSER_HEADER_BYTES || (bytes - LOGPARSER_HEADER_BYTES) % LOGPARSER_RECORD_BYTES != 0) {
        THROW("LogCommands admission: incomplete record extent");
        return false;
    }
    *outBytes = bytes;
    return true;
}
bool LogCommands_stat(const char *path, DeterminantOutputFn output, any hostContext) {
    i64 bytes;
    if (!admit(path, output, &bytes))
        return false;
    char text[160];
    i32 n = snprintf(text, sizeof(text), "log: records=%lld bytes=%lld payload=%lld",
        (i64) ((bytes - LOGPARSER_HEADER_BYTES) / LOGPARSER_RECORD_BYTES),
        (i64) bytes, (i64) (bytes - LOGPARSER_HEADER_BYTES));
    if (n < 0 || (bytes) n >= sizeof(text) || !output(hostContext, text, (bytes) n)) {
        THROW("LogCommands stat: formatting or sink failure");
        return false;
    }
    return true;
}
static fn catHandler(any context, i32 kind, i64 ts, i64 v0, i64 v1,
                       i64 v2, i64 v3, i64 v4) {
    Cat *cat = context;
    Cat_setTotal(cat, Cat_total(cat) + 1u);
    if (Cat_failed(cat) || (Cat_limit(cat) >= 0 && Cat_shown(cat) >= Cat_limit(cat)))
        return;
    char text[256]; // Seven int64/32 values + fixed separators fit by construction.
    i32 n = snprintf(text, sizeof(text), "timestamp=%lld kind=%d values=%lld %lld %lld %lld %lld",
        (i64) ts, kind, (i64) v0, (i64) v1, (i64) v2,
        (i64) v3, (i64) v4);
    if (n < 0 || (bytes) n >= sizeof(text) || !Cat_output(cat)(Cat_hostContext(cat), text, (bytes) n)) {
        Cat_setFailed(cat, true);
        return;
    }
    Cat_setShown(cat, Cat_shown(cat) + 1u);
}
bool LogCommands_cat(const char *path, i32 limit, DeterminantOutputFn output, any hostContext) {
    i64 bytes;
    if (!admit(path, output, &bytes))
        return false;
    Cat cat = {.limit = limit, .output = output, .hostContext = hostContext};
    i64 count = LogParser_parse(path, catHandler, &cat);
    if (count != (bytes - LOGPARSER_HEADER_BYTES) / LOGPARSER_RECORD_BYTES || cat.failed) {
        THROW("LogCommands cat: parse or sink failure");
        return false;
    }
    if (cat.shown < cat.total) {
        char text[64];
        i32 n = snprintf(text, sizeof(text), "(%lld more)", (i64) (cat.total - cat.shown));
        if (n < 0 || (bytes) n >= sizeof(text) || !output(hostContext, text, (bytes) n)) {
            THROW("LogCommands cat: tail sink failure");
            return false;
        }
    }
    return true;
}
