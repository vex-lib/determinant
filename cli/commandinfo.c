#include "cli/commandinfo.h"
#include "oop/type.h"
#include "nio/mem.h"
#include "exception/throw.h"
#include "annotation/annotation.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A classified view over an owned Command. Callers build one through
 * CommandInfo_fromCommand when they need a kind/operation title, or CommandInfo_3 when
 * they already know the kind and operation. Ownership of the Command stays with the info;
 * freeing the info frees the command. */
;;OVERVIEW
/** ========================================================================
 * CLASS: CommandInfo (cli/commandinfo.c)
 * STRUCT FIELDS (Mirroring cli/commandinfo.h)
 * CommandKind kind            COMMAND_* classification, one per binary
 * CommandOperation operation  specific thing done within the kind
 * Command *command            owned Command
 * FUNCTION REGISTRY (exports)
 * Constructors: CommandInfo *CommandInfo_3(CommandKind kind,
 *   CommandOperation operation, Command *command)
 * Getters: CommandKind CommandInfo_getKind(const CommandInfo *self)
 *   CommandOperation CommandInfo_getOperation(const CommandInfo *self)
 *   Command *CommandInfo_getCommand(const CommandInfo *self)
 * Lifecycle: void CommandInfo_free(CommandInfo *self)
 * Projections: bool CommandInfo_toString/CommandInfo_toStringStruct(
 *   const CommandInfo *self, char *dest, size_t cap, bool *outTruncated)
 * CONTRACT: takes ownership of command only on success; null command rejects
 * before transfer. Free null is harmless. Projections redact owned strings.
 * ======================================================================== */
const char *CommandKind_title(CommandKind kind) {
    switch (kind) {
        case COMMAND_CD:
            return "COMMAND_CD";
        case COMMAND_PWD:
            return "COMMAND_PWD";
        case COMMAND_LS:
            return "COMMAND_LS";
        case COMMAND_TREE:
            return "COMMAND_TREE";
        case COMMAND_FIND:
            return "COMMAND_FIND";
        case COMMAND_LOCATE:
            return "COMMAND_LOCATE";
        case COMMAND_DU:
            return "COMMAND_DU";
        case COMMAND_DF:
            return "COMMAND_DF";
        case COMMAND_MOUNT:
            return "COMMAND_MOUNT";
        case COMMAND_UMOUNT:
            return "COMMAND_UMOUNT";
        case COMMAND_CAT:
            return "COMMAND_CAT";
        case COMMAND_LESS:
            return "COMMAND_LESS";
        case COMMAND_MORE:
            return "COMMAND_MORE";
        case COMMAND_HEAD:
            return "COMMAND_HEAD";
        case COMMAND_TAIL:
            return "COMMAND_TAIL";
        case COMMAND_CP:
            return "COMMAND_CP";
        case COMMAND_MV:
            return "COMMAND_MV";
        case COMMAND_RM:
            return "COMMAND_RM";
        case COMMAND_MKDIR:
            return "COMMAND_MKDIR";
        case COMMAND_RMDIR:
            return "COMMAND_RMDIR";
        case COMMAND_TOUCH:
            return "COMMAND_TOUCH";
        case COMMAND_LN:
            return "COMMAND_LN";
        case COMMAND_CHMOD:
            return "COMMAND_CHMOD";
        case COMMAND_CHOWN:
            return "COMMAND_CHOWN";
        case COMMAND_STAT:
            return "COMMAND_STAT";
        case COMMAND_FILE:
            return "COMMAND_FILE";
        case COMMAND_NANO:
            return "COMMAND_NANO";
        case COMMAND_VIM:
            return "COMMAND_VIM";
        case COMMAND_VI:
            return "COMMAND_VI";
        case COMMAND_EMACS:
            return "COMMAND_EMACS";
        case COMMAND_CODE:
            return "COMMAND_CODE";
        case COMMAND_XTERM:
            return "COMMAND_XTERM";
        case COMMAND_ALACRITTY:
            return "COMMAND_ALACRITTY";
        case COMMAND_KITTY:
            return "COMMAND_KITTY";
        case COMMAND_WEZTERM:
            return "COMMAND_WEZTERM";
        case COMMAND_TMUX:
            return "COMMAND_TMUX";
        case COMMAND_SCREEN:
            return "COMMAND_SCREEN";
        case COMMAND_PYTHON:
            return "COMMAND_PYTHON";
        case COMMAND_NODE:
            return "COMMAND_NODE";
        case COMMAND_DENO:
            return "COMMAND_DENO";
        case COMMAND_BUN:
            return "COMMAND_BUN";
        case COMMAND_RUBY:
            return "COMMAND_RUBY";
        case COMMAND_PERL:
            return "COMMAND_PERL";
        case COMMAND_PHP:
            return "COMMAND_PHP";
        case COMMAND_LUA:
            return "COMMAND_LUA";
        case COMMAND_GO:
            return "COMMAND_GO";
        case COMMAND_RUSTC:
            return "COMMAND_RUSTC";
        case COMMAND_JAVA:
            return "COMMAND_JAVA";
        case COMMAND_JAVAC:
            return "COMMAND_JAVAC";
        case COMMAND_MAKE:
            return "COMMAND_MAKE";
        case COMMAND_CMAKE:
            return "COMMAND_CMAKE";
        case COMMAND_NINJA:
            return "COMMAND_NINJA";
        case COMMAND_CARGO:
            return "COMMAND_CARGO";
        case COMMAND_GRADLE:
            return "COMMAND_GRADLE";
        case COMMAND_MAVEN:
            return "COMMAND_MAVEN";
        case COMMAND_GCC:
            return "COMMAND_GCC";
        case COMMAND_GPP:
            return "COMMAND_GPP";
        case COMMAND_CLANG:
            return "COMMAND_CLANG";
        case COMMAND_CLANGPP:
            return "COMMAND_CLANGPP";
        case COMMAND_NPM:
            return "COMMAND_NPM";
        case COMMAND_PNPM:
            return "COMMAND_PNPM";
        case COMMAND_YARN:
            return "COMMAND_YARN";
        case COMMAND_PIP:
            return "COMMAND_PIP";
        case COMMAND_BREW:
            return "COMMAND_BREW";
        case COMMAND_APT:
            return "COMMAND_APT";
        case COMMAND_YUM:
            return "COMMAND_YUM";
        case COMMAND_DNF:
            return "COMMAND_DNF";
        case COMMAND_PACMAN:
            return "COMMAND_PACMAN";
        case COMMAND_GIT:
            return "COMMAND_GIT";
        case COMMAND_GH:
            return "COMMAND_GH";
        case COMMAND_HG:
            return "COMMAND_HG";
        case COMMAND_SVN:
            return "COMMAND_SVN";
        case COMMAND_GREP:
            return "COMMAND_GREP";
        case COMMAND_RG:
            return "COMMAND_RG";
        case COMMAND_AG:
            return "COMMAND_AG";
        case COMMAND_PING:
            return "COMMAND_PING";
        case COMMAND_CURL:
            return "COMMAND_CURL";
        case COMMAND_WGET:
            return "COMMAND_WGET";
        case COMMAND_SSH:
            return "COMMAND_SSH";
        case COMMAND_SCP:
            return "COMMAND_SCP";
        case COMMAND_SFTP:
            return "COMMAND_SFTP";
        case COMMAND_NC:
            return "COMMAND_NC";
        case COMMAND_DIG:
            return "COMMAND_DIG";
        case COMMAND_NSLOOKUP:
            return "COMMAND_NSLOOKUP";
        case COMMAND_IFCONFIG:
            return "COMMAND_IFCONFIG";
        case COMMAND_IP:
            return "COMMAND_IP";
        case COMMAND_SS:
            return "COMMAND_SS";
        case COMMAND_NETSTAT:
            return "COMMAND_NETSTAT";
        case COMMAND_TRACEROUTE:
            return "COMMAND_TRACEROUTE";
        case COMMAND_PS:
            return "COMMAND_PS";
        case COMMAND_TOP:
            return "COMMAND_TOP";
        case COMMAND_HTOP:
            return "COMMAND_HTOP";
        case COMMAND_KILL:
            return "COMMAND_KILL";
        case COMMAND_KILLALL:
            return "COMMAND_KILLALL";
        case COMMAND_SLEEP:
            return "COMMAND_SLEEP";
        case COMMAND_JOBS:
            return "COMMAND_JOBS";
        case COMMAND_UNAME:
            return "COMMAND_UNAME";
        case COMMAND_WHOAMI:
            return "COMMAND_WHOAMI";
        case COMMAND_HOSTNAME:
            return "COMMAND_HOSTNAME";
        case COMMAND_ECHO:
            return "COMMAND_ECHO";
        case COMMAND_ENV:
            return "COMMAND_ENV";
        case COMMAND_DATE:
            return "COMMAND_DATE";
        case COMMAND_HISTORY:
            return "COMMAND_HISTORY";
        case COMMAND_CLEAR:
            return "COMMAND_CLEAR";
        case COMMAND_EXIT:
            return "COMMAND_EXIT";
        case COMMAND_HELP:
            return "COMMAND_HELP";
        case COMMAND_MAN:
            return "COMMAND_MAN";
        case COMMAND_WHICH:
            return "COMMAND_WHICH";
        case COMMAND_TAR:
            return "COMMAND_TAR";
        case COMMAND_ZIP:
            return "COMMAND_ZIP";
        case COMMAND_UNZIP:
            return "COMMAND_UNZIP";
        case COMMAND_SED:
            return "COMMAND_SED";
        case COMMAND_AWK:
            return "COMMAND_AWK";
        case COMMAND_SORT:
            return "COMMAND_SORT";
        case COMMAND_UNIQ:
            return "COMMAND_UNIQ";
        case COMMAND_WC:
            return "COMMAND_WC";
        case COMMAND_CUT:
            return "COMMAND_CUT";
        case COMMAND_XARGS:
            return "COMMAND_XARGS";
        case COMMAND_VULKANINFO:
            return "COMMAND_VULKANINFO";
        case COMMAND_GLXINFO:
            return "COMMAND_GLXINFO";
        case COMMAND_VKVIA:
            return "COMMAND_VKVIA";
        case COMMAND_OPEN:
            return "COMMAND_OPEN";
        case COMMAND_SUDO:
            return "COMMAND_SUDO";
        case COMMAND_EXPORT:
            return "COMMAND_EXPORT";
        case COMMAND_KIND_COUNT:
        case COMMAND_UNKNOWN:
            break;
    }
    return "COMMAND_UNKNOWN";
}
static const struct { const char *name; CommandKind kind; } kindRows[] = {
    {"cd", COMMAND_CD},
    {"pwd", COMMAND_PWD},
    {"ls", COMMAND_LS},
    {"tree", COMMAND_TREE},
    {"find", COMMAND_FIND},
    {"locate", COMMAND_LOCATE},
    {"du", COMMAND_DU},
    {"df", COMMAND_DF},
    {"mount", COMMAND_MOUNT},
    {"umount", COMMAND_UMOUNT},
    {"cat", COMMAND_CAT},
    {"less", COMMAND_LESS},
    {"more", COMMAND_MORE},
    {"head", COMMAND_HEAD},
    {"tail", COMMAND_TAIL},
    {"cp", COMMAND_CP},
    {"mv", COMMAND_MV},
    {"rm", COMMAND_RM},
    {"mkdir", COMMAND_MKDIR},
    {"rmdir", COMMAND_RMDIR},
    {"touch", COMMAND_TOUCH},
    {"ln", COMMAND_LN},
    {"chmod", COMMAND_CHMOD},
    {"chown", COMMAND_CHOWN},
    {"stat", COMMAND_STAT},
    {"file", COMMAND_FILE},
    {"nano", COMMAND_NANO},
    {"vim", COMMAND_VIM},
    {"vi", COMMAND_VI},
    {"emacs", COMMAND_EMACS},
    {"code", COMMAND_CODE},
    {"xterm", COMMAND_XTERM},
    {"alacritty", COMMAND_ALACRITTY},
    {"kitty", COMMAND_KITTY},
    {"wezterm", COMMAND_WEZTERM},
    {"tmux", COMMAND_TMUX},
    {"screen", COMMAND_SCREEN},
    {"python3", COMMAND_PYTHON},
    {"python", COMMAND_PYTHON},
    {"node", COMMAND_NODE},
    {"deno", COMMAND_DENO},
    {"bun", COMMAND_BUN},
    {"ruby", COMMAND_RUBY},
    {"perl", COMMAND_PERL},
    {"php", COMMAND_PHP},
    {"lua", COMMAND_LUA},
    {"go", COMMAND_GO},
    {"rustc", COMMAND_RUSTC},
    {"java", COMMAND_JAVA},
    {"javac", COMMAND_JAVAC},
    {"make", COMMAND_MAKE},
    {"cmake", COMMAND_CMAKE},
    {"ninja", COMMAND_NINJA},
    {"cargo", COMMAND_CARGO},
    {"gradle", COMMAND_GRADLE},
    {"mvn", COMMAND_MAVEN},
    {"gcc", COMMAND_GCC},
    {"g++", COMMAND_GPP},
    {"clang", COMMAND_CLANG},
    {"clang++", COMMAND_CLANGPP},
    {"npm", COMMAND_NPM},
    {"pnpm", COMMAND_PNPM},
    {"yarn", COMMAND_YARN},
    {"pip3", COMMAND_PIP},
    {"pip", COMMAND_PIP},
    {"brew", COMMAND_BREW},
    {"apt", COMMAND_APT},
    {"yum", COMMAND_YUM},
    {"dnf", COMMAND_DNF},
    {"pacman", COMMAND_PACMAN},
    {"git", COMMAND_GIT},
    {"gh", COMMAND_GH},
    {"hg", COMMAND_HG},
    {"svn", COMMAND_SVN},
    {"grep", COMMAND_GREP},
    {"rg", COMMAND_RG},
    {"ag", COMMAND_AG},
    {"ping", COMMAND_PING},
    {"curl", COMMAND_CURL},
    {"wget", COMMAND_WGET},
    {"ssh", COMMAND_SSH},
    {"scp", COMMAND_SCP},
    {"sftp", COMMAND_SFTP},
    {"nc", COMMAND_NC},
    {"dig", COMMAND_DIG},
    {"nslookup", COMMAND_NSLOOKUP},
    {"ifconfig", COMMAND_IFCONFIG},
    {"ip", COMMAND_IP},
    {"ss", COMMAND_SS},
    {"netstat", COMMAND_NETSTAT},
    {"traceroute", COMMAND_TRACEROUTE},
    {"ps", COMMAND_PS},
    {"top", COMMAND_TOP},
    {"htop", COMMAND_HTOP},
    {"kill", COMMAND_KILL},
    {"killall", COMMAND_KILLALL},
    {"sleep", COMMAND_SLEEP},
    {"jobs", COMMAND_JOBS},
    {"uname", COMMAND_UNAME},
    {"whoami", COMMAND_WHOAMI},
    {"hostname", COMMAND_HOSTNAME},
    {"echo", COMMAND_ECHO},
    {"env", COMMAND_ENV},
    {"date", COMMAND_DATE},
    {"history", COMMAND_HISTORY},
    {"clear", COMMAND_CLEAR},
    {"exit", COMMAND_EXIT},
    {"help", COMMAND_HELP},
    {"man", COMMAND_MAN},
    {"which", COMMAND_WHICH},
    {"tar", COMMAND_TAR},
    {"zip", COMMAND_ZIP},
    {"unzip", COMMAND_UNZIP},
    {"sed", COMMAND_SED},
    {"awk", COMMAND_AWK},
    {"sort", COMMAND_SORT},
    {"uniq", COMMAND_UNIQ},
    {"wc", COMMAND_WC},
    {"cut", COMMAND_CUT},
    {"xargs", COMMAND_XARGS},
    {"vulkaninfo", COMMAND_VULKANINFO},
    {"glxinfo", COMMAND_GLXINFO},
    {"vkvia", COMMAND_VKVIA},
    {"open", COMMAND_OPEN},
    {"sudo", COMMAND_SUDO},
    {"export", COMMAND_EXPORT},
};
static CommandKind classify(const char *name) { if (name == nullptr) return COMMAND_UNKNOWN; for (size_t i = 0; i < sizeof(kindRows)/sizeof(kindRows[0]); ++i) if (strcmp(name, kindRows[i].name) == 0) return kindRows[i].kind; return COMMAND_UNKNOWN; }
static bool flagHas(const char *arg, char letter) {
    if (arg == nullptr || arg[0] != '-')
        return false;
    for (size_t j = 1; arg[j] != 0; ++j)
        if (arg[j] == letter)
            return true;
    return false;
}
static bool anyFlag(Command *command, char letter) {
    size_t argc = Command_argumentCount(command);
    for (size_t i = 0; i < argc; ++i) {
        const char *arg = (const char*) Command_argument(command, i);
        if (flagHas(arg, letter) || (letter == 'r' && flagHas(arg, 'R')))
            return true;
    }
    return false;
}
static CommandOperation classifyOperation(CommandKind kind, Command *command) {
    size_t argc = Command_argumentCount(command);
    const char *arg0 = argc > 0 ? (const char*) Command_argument(command, 0) : nullptr;
    switch (kind) {
        case COMMAND_CD:
            return COMMAND_CD_CHANGE_DIR;
        case COMMAND_PWD:
            return COMMAND_PWD_PRINT;
        case COMMAND_LS:
            return COMMAND_LS_LIST;
        case COMMAND_TREE:
            return COMMAND_TREE_LIST;
        case COMMAND_FIND:
            return COMMAND_FIND_SEARCH;
        case COMMAND_LOCATE:
            return COMMAND_LOCATE_SEARCH;
        case COMMAND_DU:
            return COMMAND_DU_USAGE;
        case COMMAND_DF:
            return COMMAND_DF_FREE;
        case COMMAND_MOUNT:
            return COMMAND_MOUNT_MOUNT;
        case COMMAND_UMOUNT:
            return COMMAND_UMOUNT_UNMOUNT;
        case COMMAND_CAT:
            return COMMAND_CAT_SHOW;
        case COMMAND_LESS:
            return COMMAND_LESS_SHOW;
        case COMMAND_MORE:
            return COMMAND_MORE_SHOW;
        case COMMAND_HEAD:
            return COMMAND_HEAD_SHOW;
        case COMMAND_TAIL:
            return COMMAND_TAIL_SHOW;
        case COMMAND_CP:
            return COMMAND_CP_COPY;
        case COMMAND_MV:
            return COMMAND_MV_MOVE;
        case COMMAND_RM:
            return anyFlag(command, 'r') ? COMMAND_RM_RECURSIVE_DELETE : COMMAND_RM_DELETE_FILE;
        case COMMAND_MKDIR:
            return COMMAND_MKDIR_MAKE_DIR;
        case COMMAND_RMDIR:
            return COMMAND_RMDIR_DELETE_FOLDER;
        case COMMAND_TOUCH:
            return COMMAND_TOUCH_TOUCH;
        case COMMAND_LN:
            return COMMAND_LN_LINK;
        case COMMAND_CHMOD:
            return COMMAND_CHMOD_CHANGE_MODE;
        case COMMAND_CHOWN:
            return COMMAND_CHOWN_CHANGE_OWNER;
        case COMMAND_STAT:
            return COMMAND_STAT_SHOW;
        case COMMAND_FILE:
            return COMMAND_FILE_TYPE;
        case COMMAND_NANO:
            return COMMAND_NANO_EDIT;
        case COMMAND_VIM:
            return COMMAND_VIM_EDIT;
        case COMMAND_VI:
            return COMMAND_VI_EDIT;
        case COMMAND_EMACS:
            return COMMAND_EMACS_EDIT;
        case COMMAND_CODE:
            return COMMAND_CODE_EDIT;
        case COMMAND_XTERM:
            return COMMAND_XTERM_LAUNCH;
        case COMMAND_ALACRITTY:
            return COMMAND_ALACRITTY_LAUNCH;
        case COMMAND_KITTY:
            return COMMAND_KITTY_LAUNCH;
        case COMMAND_WEZTERM:
            return COMMAND_WEZTERM_LAUNCH;
        case COMMAND_TMUX:
            return COMMAND_TMUX_LAUNCH;
        case COMMAND_SCREEN:
            return COMMAND_SCREEN_LAUNCH;
        case COMMAND_PYTHON:
            return COMMAND_PYTHON_RUN_SCRIPT;
        case COMMAND_NODE:
            return COMMAND_NODE_RUN;
        case COMMAND_DENO:
            return COMMAND_DENO_RUN;
        case COMMAND_BUN:
            return COMMAND_BUN_RUN;
        case COMMAND_RUBY:
            return COMMAND_RUBY_RUN;
        case COMMAND_PERL:
            return COMMAND_PERL_RUN;
        case COMMAND_PHP:
            return COMMAND_PHP_RUN;
        case COMMAND_LUA:
            return COMMAND_LUA_RUN;
        case COMMAND_GO:
            return COMMAND_GO_BUILD;
        case COMMAND_RUSTC:
            return COMMAND_RUSTC_COMPILE;
        case COMMAND_JAVA:
            return COMMAND_JAVA_RUN;
        case COMMAND_JAVAC:
            return COMMAND_JAVAC_COMPILE;
        case COMMAND_MAKE:
            return COMMAND_MAKE_BUILD;
        case COMMAND_CMAKE:
            return COMMAND_CMAKE_CONFIGURE;
        case COMMAND_NINJA:
            return COMMAND_NINJA_BUILD;
        case COMMAND_CARGO:
            return COMMAND_CARGO_BUILD;
        case COMMAND_GRADLE:
            return COMMAND_GRADLE_BUILD;
        case COMMAND_MAVEN:
            return COMMAND_MAVEN_BUILD;
        case COMMAND_GCC:
            return COMMAND_GCC_COMPILE;
        case COMMAND_GPP:
            return COMMAND_GPP_COMPILE;
        case COMMAND_CLANG:
            return COMMAND_CLANG_COMPILE;
        case COMMAND_CLANGPP:
            return COMMAND_CLANGPP_COMPILE;
        case COMMAND_NPM: {
            if (arg0 == nullptr) return COMMAND_NPM_RUN;
            if (strcmp(arg0, "install") == 0 || strcmp(arg0, "i") == 0) return COMMAND_NPM_INSTALL;
            if (strcmp(arg0, "run") == 0) return COMMAND_NPM_RUN;
            if (strcmp(arg0, "publish") == 0) return COMMAND_NPM_PUBLISH;
            if (strcmp(arg0, "uninstall") == 0 || strcmp(arg0, "remove") == 0 || strcmp(arg0, "rm") == 0) return COMMAND_NPM_UNINSTALL;
            if (strcmp(arg0, "test") == 0 || strcmp(arg0, "t") == 0) return COMMAND_NPM_TEST;
            if (strcmp(arg0, "exec") == 0 || strcmp(arg0, "x") == 0) return COMMAND_NPM_EXEC;
            if (strcmp(arg0, "init") == 0) return COMMAND_NPM_INIT;
            return COMMAND_NPM_RUN;
        }
        case COMMAND_PNPM: {
            if (arg0 != nullptr && strcmp(arg0, "install") == 0) return COMMAND_PNPM_INSTALL;
            return COMMAND_PNPM_RUN;
        }
        case COMMAND_YARN: {
            if (arg0 != nullptr && strcmp(arg0, "install") == 0) return COMMAND_YARN_INSTALL;
            return COMMAND_YARN_RUN;
        }
        case COMMAND_PIP:
            return COMMAND_PIP_INSTALL;
        case COMMAND_BREW:
            return COMMAND_BREW_INSTALL;
        case COMMAND_APT:
            return COMMAND_APT_INSTALL;
        case COMMAND_YUM:
            return COMMAND_YUM_INSTALL;
        case COMMAND_DNF:
            return COMMAND_DNF_INSTALL;
        case COMMAND_PACMAN:
            return COMMAND_PACMAN_INSTALL;
        case COMMAND_GIT: {
            static const struct { const char *sub; CommandOperation op; } gitOps[] = {
                {"push", COMMAND_GIT_PUSH}, {"pull", COMMAND_GIT_PULL},
                {"commit", COMMAND_GIT_COMMIT}, {"status", COMMAND_GIT_STATUS},
                {"checkout", COMMAND_GIT_CHECKOUT}, {"switch", COMMAND_GIT_CHECKOUT},
                {"add", COMMAND_GIT_ADD}, {"clone", COMMAND_GIT_CLONE},
                {"fetch", COMMAND_GIT_FETCH}, {"merge", COMMAND_GIT_MERGE},
                {"rebase", COMMAND_GIT_REBASE}, {"log", COMMAND_GIT_LOG},
                {"diff", COMMAND_GIT_DIFF}, {"branch", COMMAND_GIT_BRANCH},
                {"init", COMMAND_GIT_INIT}, {"reset", COMMAND_GIT_RESET},
                {"stash", COMMAND_GIT_STASH}
            };
            if (arg0 != nullptr)
                for (size_t i = 0; i < sizeof(gitOps) / sizeof(gitOps[0]); ++i)
                    if (strcmp(arg0, gitOps[i].sub) == 0)
                        return gitOps[i].op;
            return COMMAND_OPERATION_UNKNOWN;
        }
        case COMMAND_GH: {
            if (arg0 != nullptr && strcmp(arg0, "pr") == 0) return COMMAND_GH_PR;
            if (arg0 != nullptr && strcmp(arg0, "repo") == 0) return COMMAND_GH_REPO;
            return COMMAND_OPERATION_UNKNOWN;
        }
        case COMMAND_HG:
            return COMMAND_HG_COMMIT;
        case COMMAND_SVN:
            return COMMAND_SVN_COMMIT;
        case COMMAND_GREP: case COMMAND_RG: case COMMAND_AG: {
            return kind == COMMAND_GREP ? COMMAND_GREP_SEARCH : kind == COMMAND_RG ? COMMAND_RG_SEARCH : COMMAND_AG_SEARCH;
        }
        case COMMAND_PING:
            return COMMAND_PING_TEST;
        case COMMAND_CURL:
            return COMMAND_CURL_FETCH;
        case COMMAND_WGET:
            return COMMAND_WGET_FETCH;
        case COMMAND_SSH:
            return COMMAND_SSH_CONNECT;
        case COMMAND_SCP:
            return COMMAND_SCP_COPY;
        case COMMAND_SFTP:
            return COMMAND_SFTP_TRANSFER;
        case COMMAND_NC:
            return COMMAND_NC_CONNECT;
        case COMMAND_DIG:
            return COMMAND_DIG_LOOKUP;
        case COMMAND_NSLOOKUP:
            return COMMAND_NSLOOKUP_LOOKUP;
        case COMMAND_IFCONFIG:
            return COMMAND_IFCONFIG_SHOW;
        case COMMAND_IP:
            return COMMAND_IP_SHOW;
        case COMMAND_SS:
            return COMMAND_SS_SHOW;
        case COMMAND_NETSTAT:
            return COMMAND_NETSTAT_SHOW;
        case COMMAND_TRACEROUTE:
            return COMMAND_TRACEROUTE_TRACE;
        case COMMAND_PS:
            return COMMAND_PS_LIST;
        case COMMAND_TOP:
            return COMMAND_TOP_MONITOR;
        case COMMAND_HTOP:
            return COMMAND_HTOP_MONITOR;
        case COMMAND_KILL:
            return COMMAND_KILL_TERMINATE;
        case COMMAND_KILLALL:
            return COMMAND_KILLALL_TERMINATE;
        case COMMAND_SLEEP:
            return COMMAND_SLEEP_WAIT;
        case COMMAND_JOBS:
            return COMMAND_JOBS_LIST;
        case COMMAND_UNAME:
            return COMMAND_UNAME_SHOW;
        case COMMAND_WHOAMI:
            return COMMAND_WHOAMI_SHOW;
        case COMMAND_HOSTNAME:
            return COMMAND_HOSTNAME_SHOW;
        case COMMAND_ECHO:
            return COMMAND_ECHO_PRINT;
        case COMMAND_ENV:
            return COMMAND_ENV_SHOW;
        case COMMAND_DATE:
            return COMMAND_DATE_SHOW;
        case COMMAND_HISTORY:
            return COMMAND_HISTORY_SHOW;
        case COMMAND_CLEAR:
            return COMMAND_CLEAR_CLEAR;
        case COMMAND_EXIT:
            return COMMAND_EXIT_EXIT;
        case COMMAND_HELP:
            return COMMAND_HELP_SHOW;
        case COMMAND_MAN:
            return COMMAND_MAN_SHOW;
        case COMMAND_WHICH:
            return COMMAND_WHICH_LOCATE;
        case COMMAND_TAR:
            return COMMAND_TAR_ARCHIVE;
        case COMMAND_ZIP:
            return COMMAND_ZIP_ARCHIVE;
        case COMMAND_UNZIP:
            return COMMAND_UNZIP_EXTRACT;
        case COMMAND_SED:
            return COMMAND_SED_EDIT;
        case COMMAND_AWK:
            return COMMAND_AWK_PROCESS;
        case COMMAND_SORT:
            return COMMAND_SORT_SORT;
        case COMMAND_UNIQ:
            return COMMAND_UNIQ_FILTER;
        case COMMAND_WC:
            return COMMAND_WC_COUNT;
        case COMMAND_CUT:
            return COMMAND_CUT_CUT;
        case COMMAND_XARGS:
            return COMMAND_XARGS_EXEC;
        case COMMAND_VULKANINFO:
            return COMMAND_VULKANINFO_QUERY;
        case COMMAND_GLXINFO:
            return COMMAND_GLXINFO_QUERY;
        case COMMAND_VKVIA:
            return COMMAND_VKVIA_QUERY;
        case COMMAND_OPEN:
            return COMMAND_OPEN_OPEN;
        case COMMAND_SUDO:
            return COMMAND_SUDO_RUN;
        case COMMAND_EXPORT:
            return COMMAND_EXPORT_SET;
        case COMMAND_KIND_COUNT:
        case COMMAND_UNKNOWN:
            break;
    }
    return COMMAND_OPERATION_UNKNOWN;
}
const char *CommandOperation_title(CommandOperation operation) {
    switch (operation) {
        case COMMAND_OPERATION_UNKNOWN:
            return "COMMAND_OPERATION_UNKNOWN";

#define TITLE(op) case op: return #op;
        TITLE(COMMAND_CD_CHANGE_DIR)
        TITLE(COMMAND_PWD_PRINT)
        TITLE(COMMAND_LS_LIST)
        TITLE(COMMAND_TREE_LIST)
        TITLE(COMMAND_FIND_SEARCH)
        TITLE(COMMAND_LOCATE_SEARCH)
        TITLE(COMMAND_DU_USAGE)
        TITLE(COMMAND_DF_FREE)
        TITLE(COMMAND_MOUNT_MOUNT)
        TITLE(COMMAND_UMOUNT_UNMOUNT)
        TITLE(COMMAND_CAT_SHOW)
        TITLE(COMMAND_LESS_SHOW)
        TITLE(COMMAND_MORE_SHOW)
        TITLE(COMMAND_HEAD_SHOW)
        TITLE(COMMAND_TAIL_SHOW)
        TITLE(COMMAND_CP_COPY)
        TITLE(COMMAND_MV_MOVE)
        TITLE(COMMAND_RM_DELETE_FILE)
        TITLE(COMMAND_RM_RECURSIVE_DELETE)
        TITLE(COMMAND_MKDIR_MAKE_DIR)
        TITLE(COMMAND_RMDIR_DELETE_FOLDER)
        TITLE(COMMAND_TOUCH_TOUCH)
        TITLE(COMMAND_LN_LINK)
        TITLE(COMMAND_CHMOD_CHANGE_MODE)
        TITLE(COMMAND_CHOWN_CHANGE_OWNER)
        TITLE(COMMAND_STAT_SHOW)
        TITLE(COMMAND_FILE_TYPE)
        TITLE(COMMAND_NANO_EDIT)
        TITLE(COMMAND_VIM_EDIT)
        TITLE(COMMAND_VI_EDIT)
        TITLE(COMMAND_EMACS_EDIT)
        TITLE(COMMAND_CODE_EDIT)
        TITLE(COMMAND_XTERM_LAUNCH)
        TITLE(COMMAND_ALACRITTY_LAUNCH)
        TITLE(COMMAND_KITTY_LAUNCH)
        TITLE(COMMAND_WEZTERM_LAUNCH)
        TITLE(COMMAND_TMUX_LAUNCH)
        TITLE(COMMAND_SCREEN_LAUNCH)
        TITLE(COMMAND_PYTHON_RUN_SCRIPT)
        TITLE(COMMAND_NODE_RUN)
        TITLE(COMMAND_DENO_RUN)
        TITLE(COMMAND_BUN_RUN)
        TITLE(COMMAND_RUBY_RUN)
        TITLE(COMMAND_PERL_RUN)
        TITLE(COMMAND_PHP_RUN)
        TITLE(COMMAND_LUA_RUN)
        TITLE(COMMAND_GO_BUILD)
        TITLE(COMMAND_RUSTC_COMPILE)
        TITLE(COMMAND_JAVA_RUN)
        TITLE(COMMAND_JAVAC_COMPILE)
        TITLE(COMMAND_MAKE_BUILD)
        TITLE(COMMAND_CMAKE_CONFIGURE)
        TITLE(COMMAND_NINJA_BUILD)
        TITLE(COMMAND_CARGO_BUILD)
        TITLE(COMMAND_GRADLE_BUILD)
        TITLE(COMMAND_MAVEN_BUILD)
        TITLE(COMMAND_GCC_COMPILE)
        TITLE(COMMAND_GPP_COMPILE)
        TITLE(COMMAND_CLANG_COMPILE)
        TITLE(COMMAND_CLANGPP_COMPILE)
        TITLE(COMMAND_NPM_INSTALL)
        TITLE(COMMAND_NPM_RUN)
        TITLE(COMMAND_NPM_PUBLISH)
        TITLE(COMMAND_NPM_UNINSTALL)
        TITLE(COMMAND_NPM_TEST)
        TITLE(COMMAND_NPM_EXEC)
        TITLE(COMMAND_NPM_INIT)
        TITLE(COMMAND_PNPM_INSTALL)
        TITLE(COMMAND_PNPM_RUN)
        TITLE(COMMAND_YARN_INSTALL)
        TITLE(COMMAND_YARN_RUN)
        TITLE(COMMAND_PIP_INSTALL)
        TITLE(COMMAND_BREW_INSTALL)
        TITLE(COMMAND_APT_INSTALL)
        TITLE(COMMAND_YUM_INSTALL)
        TITLE(COMMAND_DNF_INSTALL)
        TITLE(COMMAND_PACMAN_INSTALL)
        TITLE(COMMAND_GIT_PUSH)
        TITLE(COMMAND_GIT_PULL)
        TITLE(COMMAND_GIT_COMMIT)
        TITLE(COMMAND_GIT_STATUS)
        TITLE(COMMAND_GIT_CHECKOUT)
        TITLE(COMMAND_GIT_ADD)
        TITLE(COMMAND_GIT_CLONE)
        TITLE(COMMAND_GIT_FETCH)
        TITLE(COMMAND_GIT_MERGE)
        TITLE(COMMAND_GIT_REBASE)
        TITLE(COMMAND_GIT_LOG)
        TITLE(COMMAND_GIT_DIFF)
        TITLE(COMMAND_GIT_BRANCH)
        TITLE(COMMAND_GIT_INIT)
        TITLE(COMMAND_GIT_RESET)
        TITLE(COMMAND_GIT_STASH)
        TITLE(COMMAND_GH_PR)
        TITLE(COMMAND_GH_REPO)
        TITLE(COMMAND_HG_COMMIT)
        TITLE(COMMAND_SVN_COMMIT)
        TITLE(COMMAND_GREP_SEARCH)
        TITLE(COMMAND_RG_SEARCH)
        TITLE(COMMAND_AG_SEARCH)
        TITLE(COMMAND_PING_TEST)
        TITLE(COMMAND_CURL_FETCH)
        TITLE(COMMAND_WGET_FETCH)
        TITLE(COMMAND_SSH_CONNECT)
        TITLE(COMMAND_SCP_COPY)
        TITLE(COMMAND_SFTP_TRANSFER)
        TITLE(COMMAND_NC_CONNECT)
        TITLE(COMMAND_DIG_LOOKUP)
        TITLE(COMMAND_NSLOOKUP_LOOKUP)
        TITLE(COMMAND_IFCONFIG_SHOW)
        TITLE(COMMAND_IP_SHOW)
        TITLE(COMMAND_SS_SHOW)
        TITLE(COMMAND_NETSTAT_SHOW)
        TITLE(COMMAND_TRACEROUTE_TRACE)
        TITLE(COMMAND_PS_LIST)
        TITLE(COMMAND_TOP_MONITOR)
        TITLE(COMMAND_HTOP_MONITOR)
        TITLE(COMMAND_KILL_TERMINATE)
        TITLE(COMMAND_KILLALL_TERMINATE)
        TITLE(COMMAND_SLEEP_WAIT)
        TITLE(COMMAND_JOBS_LIST)
        TITLE(COMMAND_UNAME_SHOW)
        TITLE(COMMAND_WHOAMI_SHOW)
        TITLE(COMMAND_HOSTNAME_SHOW)
        TITLE(COMMAND_ECHO_PRINT)
        TITLE(COMMAND_ENV_SHOW)
        TITLE(COMMAND_DATE_SHOW)
        TITLE(COMMAND_HISTORY_SHOW)
        TITLE(COMMAND_CLEAR_CLEAR)
        TITLE(COMMAND_EXIT_EXIT)
        TITLE(COMMAND_HELP_SHOW)
        TITLE(COMMAND_MAN_SHOW)
        TITLE(COMMAND_WHICH_LOCATE)
        TITLE(COMMAND_TAR_ARCHIVE)
        TITLE(COMMAND_ZIP_ARCHIVE)
        TITLE(COMMAND_UNZIP_EXTRACT)
        TITLE(COMMAND_SED_EDIT)
        TITLE(COMMAND_AWK_PROCESS)
        TITLE(COMMAND_SORT_SORT)
        TITLE(COMMAND_UNIQ_FILTER)
        TITLE(COMMAND_WC_COUNT)
        TITLE(COMMAND_CUT_CUT)
        TITLE(COMMAND_XARGS_EXEC)
        TITLE(COMMAND_VULKANINFO_QUERY)
        TITLE(COMMAND_GLXINFO_QUERY)
        TITLE(COMMAND_VKVIA_QUERY)
        TITLE(COMMAND_OPEN_OPEN)
        TITLE(COMMAND_SUDO_RUN)
        TITLE(COMMAND_EXPORT_SET)
#undef TITLE
        case COMMAND_OPERATION_COUNT:
            break;
    }
    return "COMMAND_OPERATION_UNKNOWN";
}

CommandInfo *CommandInfo_3(CommandKind kind, CommandOperation operation, Command *command) {
    if (command == nullptr) {
        THROW("CommandInfo construct: null command");
        return nullptr;
    }
    if ((size_t) kind >= (size_t) COMMAND_KIND_COUNT || (size_t) operation >= (size_t) COMMAND_OPERATION_COUNT) {
        THROW("CommandInfo construct: kind or operation out of range");
        return nullptr;
    }
    CommandInfo *self = Memory_alloc(TYPE_BYTE_ARRAY, sizeof(CommandInfo));
    if (self == nullptr)
        return nullptr;
    (*self).kind = kind;
    (*self).operation = operation;
    (*self).command = command;
    return self;
}
CommandInfo *CommandInfo_fromCommand(Command *command) {
    if (command == nullptr) {
        THROW("CommandInfo fromCommand: null command");
        return nullptr;
    }
    const char *name = (const char*) Command_name(command);
    CommandKind kind = classify(name);
    CommandOperation operation = classifyOperation(kind, command);
    return CommandInfo_3(kind, operation, command);
}
CommandKind CommandInfo_getKind(const CommandInfo *self) { return self ? (*self).kind : COMMAND_UNKNOWN; }
CommandOperation CommandInfo_getOperation(const CommandInfo *self) { return self ? (*self).operation : COMMAND_OPERATION_UNKNOWN; }
Command *CommandInfo_getCommand(const CommandInfo *self) { return self ? (*self).command : nullptr; }
bool CommandInfo_isRecursiveDelete(const CommandInfo *self) { return self && (*self).operation == COMMAND_RM_RECURSIVE_DELETE; }
bool CommandInfo_isDelete(const CommandInfo *self) {
    return self && ((*self).operation == COMMAND_RM_DELETE_FILE || (*self).operation == COMMAND_RM_RECURSIVE_DELETE || (*self).operation == COMMAND_RMDIR_DELETE_FOLDER);
}
void CommandInfo_free(CommandInfo *self) {
    if (self == nullptr)
        return;
    Command_free((*self).command);
    Memory_free(self);
}
bool CommandInfo_toString(const CommandInfo *self, char *dest, size_t cap, bool *outTruncated) {
    return CommandInfo_toStringStruct(self, dest, cap, outTruncated);
}
bool CommandInfo_toStringStruct(const CommandInfo *self, char *dest, size_t cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("CommandInfo projection: missing destination");
        return false;
    }
    int n = self ? snprintf(dest, cap, "CommandInfo{kind=%s,command=<owned>}", CommandKind_title((*self).kind))
                 : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (size_t) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("CommandInfo projection: truncated at %zu bytes", cap);
    return !cut;
}
