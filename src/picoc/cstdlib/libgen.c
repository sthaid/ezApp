#include <libgen.h>

#include "../interpreter.h"

void LibgenBasename(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    char *path = Param[0]->Val->Pointer;

    ReturnValue->Val->Pointer = basename(path);
}

void LibgenDirname(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    char *path = Param[0]->Val->Pointer;

    ReturnValue->Val->Pointer = dirname(path);
}

struct LibraryFunction LibgenFunctions[] =
{
    {LibgenBasename, "char *basename(char *);"},
    {LibgenDirname,  "char *dirname(char *);"},
    {NULL,           NULL}
};
