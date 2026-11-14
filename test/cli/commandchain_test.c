// Owner: command kind titles, specific classification table, chain splitting
// on &&, empty-segment rejection, and ownership cleanup on failure.

#include "vex.h"
#include "cli/commandparser.h"
#include "cli/commandinfo.h"
#include "nio/mem.h"
#include "oop/oop.h"
#include "determinant/test/support.h"
#include <stdlib.h>
static CommandChain *parseOk(const char *line) {
    CommandChain *chain = CommandParser_parseChain(line);
    assert(chain != nullptr);
    return chain;
}
int main() {
    beginDiagnostics();
    assert(strcmp(CommandKind_title(COMMAND_LS), "COMMAND_LS") == 0);
    assert(strcmp(CommandKind_title(COMMAND_PYTHON), "COMMAND_PYTHON") == 0);
    assert(strcmp(CommandKind_title(COMMAND_CD), "COMMAND_CD") == 0);
    assert(strcmp(CommandKind_title(COMMAND_WHOAMI), "COMMAND_WHOAMI") == 0);
    assert(strcmp(CommandKind_title(COMMAND_CURL), "COMMAND_CURL") == 0);
    assert(strcmp(CommandKind_title(COMMAND_NPM), "COMMAND_NPM") == 0);
    assert(strcmp(CommandKind_title(COMMAND_GIT), "COMMAND_GIT") == 0);
    assert(strcmp(CommandKind_title(COMMAND_GH), "COMMAND_GH") == 0);
    assert(strcmp(CommandKind_title(COMMAND_GREP), "COMMAND_GREP") == 0);
    assert(strcmp(CommandKind_title(COMMAND_NANO), "COMMAND_NANO") == 0);
    assert(strcmp(CommandKind_title(COMMAND_BUN), "COMMAND_BUN") == 0);
    assert(strcmp(CommandKind_title(COMMAND_VULKANINFO), "COMMAND_VULKANINFO") == 0);
    assert(strcmp(CommandKind_title(COMMAND_UNKNOWN), "COMMAND_UNKNOWN") == 0);
    CommandChain *chain = parseOk("ls");
    assert(CommandChain_getCount(chain) == 1);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 0)) == COMMAND_LS);
    assert(strcmp((const char*) Command_name(CommandInfo_getCommand(CommandChain_getEntry(chain, 0))), "ls") == 0);
    assert(CommandChain_getEntry(chain, 1) == nullptr);
    CommandChain_free(chain);
    chain = parseOk("ls && python3 main.py && cd /tmp");
    assert(CommandChain_getCount(chain) == 3);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 0)) == COMMAND_LS);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 1)) == COMMAND_PYTHON);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 2)) == COMMAND_CD);
    assert(strcmp((const char*) Command_name(CommandInfo_getCommand(CommandChain_getEntry(chain, 1))), "python3") == 0);
    assert(strcmp((const char*) Command_argument(CommandInfo_getCommand(CommandChain_getEntry(chain, 1)), 0), "main.py") == 0);
    CommandChain_free(chain);
    chain = parseOk("nano notes.txt && xterm && whoami && date");
    assert(CommandChain_getCount(chain) == 4);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 0)) == COMMAND_NANO);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 1)) == COMMAND_XTERM);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 2)) == COMMAND_WHOAMI);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 3)) == COMMAND_DATE);
    CommandChain_free(chain);
    chain = parseOk("npm install && curl https://example.com && git status && gh pr list && grep foo bar.txt && vulkaninfo && echo done");
    assert(CommandChain_getCount(chain) == 7);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 0)) == COMMAND_NPM);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 1)) == COMMAND_CURL);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 2)) == COMMAND_GIT);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 3)) == COMMAND_GH);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 4)) == COMMAND_GREP);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 5)) == COMMAND_VULKANINFO);
    CommandChain_free(chain);
    chain = parseOk("bun build");
    assert(CommandChain_getCount(chain) == 1);
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 0)) == COMMAND_BUN);
    CommandChain_free(chain);
    chain = parseOk("rm file.txt");
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 0)) == COMMAND_RM);
    assert(CommandInfo_isDelete(CommandChain_getEntry(chain, 0)));
    assert(!CommandInfo_isRecursiveDelete(CommandChain_getEntry(chain, 0)));
    CommandChain_free(chain);
    chain = parseOk("rm -rf build");
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 0)) == COMMAND_RM);
    assert(CommandInfo_isRecursiveDelete(CommandChain_getEntry(chain, 0)));
    CommandChain_free(chain);
    chain = parseOk("rmdir old");
    assert(CommandInfo_getKind(CommandChain_getEntry(chain, 0)) == COMMAND_RMDIR);
    CommandChain_free(chain);
    assert(CommandInfo_getKind(nullptr) == COMMAND_UNKNOWN);
    assert(CommandInfo_getCommand(nullptr) == nullptr);
    assert(CommandChain_getCount(nullptr) == 0);
    assert(CommandChain_getEntry(nullptr, 0) == nullptr);
    CommandInfo_free(nullptr);
    CommandChain_free(nullptr);
    endDiagnostics(0, nullptr);
    REJECT(CommandParser_parseChain(nullptr), "null");
    REJECT(CommandParser_parseChainBytes(nullptr, 1), "null");
    REJECT(CommandParser_parseChain("x\x1b[31m"), "invalid ASCII");
    REJECT(CommandParser_parseChain("&& ls"), "empty segment");
    REJECT(CommandParser_parseChain("ls &&"), "empty segment");
    REJECT(CommandParser_parseChain("ls && && python3 x.py"), "empty segment");
    Command *bad = CommandParser_parse("ls");
    assert(bad);
    REJECT(CommandInfo_3((CommandKind) 999, COMMAND_OPERATION_UNKNOWN, bad), "out of range");
    Command_free(bad);
    Command *cmd = CommandParser_parse("ls");
    assert(cmd);
    assert(CommandInfo_fromCommand(cmd) != nullptr);
    Command_free(cmd);
    Memory_freeAll();
    beginDiagnostics();
    chain = CommandParser_parseChainBytes("ls && cd /tmp", 12);
    assert(chain && CommandChain_getCount(chain) == 2);
    CommandChain_free(chain);
    Memory_freeAll();
    endDiagnostics(0, nullptr);
    return 0;
}
