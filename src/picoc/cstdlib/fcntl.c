#include <fcntl.h>

#include "../interpreter.h"

void FcntlOpen(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    char *path  = Param[0]->Val->Pointer;
    int   flags = Param[1]->Val->Integer;
    int   mode  = Param[2]->Val->Integer;
    int   rc;

    rc = open(path, flags, mode);
    ReturnValue->Val->Integer = rc;
}   

struct LibraryFunction FcntlFunctions[] =
{
    { FcntlOpen, "int open(char*, int, int);" },
    {NULL, NULL}
};  

void FcntlSetupFunc(Picoc *pc)
{   
    static int O_RDONLY_value = O_RDONLY;
    static int O_RDWR_value   = O_RDWR;

    // xxx add more flags
    VariableDefinePlatformVar(pc, NULL, "O_RDONLY", &pc->IntType,
        (union AnyValue *)&O_RDONLY_value, false);
    VariableDefinePlatformVar(pc, NULL, "O_RDWR", &pc->IntType,
        (union AnyValue *)&O_RDWR_value, false);
}

