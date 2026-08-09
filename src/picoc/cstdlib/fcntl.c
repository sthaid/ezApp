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

#define DEFINE_OPEN_FLAG(name) \
    do { \
        static int name##_value = name; \
        VariableDefinePlatformVar(pc, NULL, #name, &pc->IntType, \
                                  (union AnyValue *)&name##_value, false); \
    } while (0)

void FcntlSetupFunc(Picoc *pc)
{   
    DEFINE_OPEN_FLAG(O_RDONLY);
    DEFINE_OPEN_FLAG(O_WRONLY);
    DEFINE_OPEN_FLAG(O_RDWR);
    DEFINE_OPEN_FLAG(O_APPEND);
    DEFINE_OPEN_FLAG(O_CREAT);
    DEFINE_OPEN_FLAG(O_EXCL);
    DEFINE_OPEN_FLAG(O_SYNC);
    DEFINE_OPEN_FLAG(O_TRUNC);
}

