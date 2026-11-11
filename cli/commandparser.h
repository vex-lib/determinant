#ifndef DETERMINANT_COMMANDPARSER_H
#define DETERMINANT_COMMANDPARSER_H
#include "cli/command.h"
#include "cli/commandinfo.h"
// Plain whitespace-separated ASCII tokens; no quotes, shell expansion or exec.
// Length-based form rejects embedded NUL/control/non-ASCII before allocation.
// Empty/whitespace is an ordinary no-command result, not an error.
Command *CommandParser_parse(const char *line);
Command *CommandParser_parseBytes(const char *line, bytes length);
// Chain form: splits a line on standalone "&&" tokens into a CommandChain
// where each segment is classified by CommandInfo_fromCommand. Empty segments
// (leading/trailing/consecutive &&) reject. "" -> nullptr.
CommandChain *CommandParser_parseChain(const char *line);
CommandChain *CommandParser_parseChainBytes(const char *line, bytes length);
#endif
