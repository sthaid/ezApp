#include <time.h>

#include "../interpreter.h"


static int CLOCKS_PER_SECValue = CLOCKS_PER_SEC;

#ifdef CLK_PER_SEC
static int CLK_PER_SECValue = CLK_PER_SEC;
#endif

#ifdef CLK_TCK
static int CLK_TCKValue = CLK_TCK;
#endif

void StdAsctime(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    ReturnValue->Val->Pointer = asctime(Param[0]->Val->Pointer);
}

void StdClock(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    ReturnValue->Val->LongInteger = clock();
}

void StdCtime(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    ReturnValue->Val->Pointer = ctime(Param[0]->Val->Pointer);
}

void StdDifftime(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    ReturnValue->Val->FP = difftime((time_t)Param[0]->Val->LongInteger,
        Param[1]->Val->LongInteger);
}

void StdGmtime(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    ReturnValue->Val->Pointer = gmtime(Param[0]->Val->Pointer);
}

void StdLocaltime(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    ReturnValue->Val->Pointer = localtime(Param[0]->Val->Pointer);
}

void StdLocaltime_r(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    ReturnValue->Val->Pointer = localtime_r(Param[0]->Val->Pointer,
                                            Param[1]->Val->Pointer);
}

void StdMktime(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    ReturnValue->Val->LongInteger = mktime(Param[0]->Val->Pointer);
}

void StdTime(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    ReturnValue->Val->LongInteger = time(Param[0]->Val->Pointer);
}

void StdStrftime(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    ReturnValue->Val->LongInteger = strftime(Param[0]->Val->Pointer,
        Param[1]->Val->LongInteger, Param[2]->Val->Pointer, Param[3]->Val->Pointer);
}

#ifndef WIN32
void StdStrptime(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    extern char *strptime(const char *s, const char *format, struct tm *tm);

    ReturnValue->Val->Pointer = strptime(Param[0]->Val->Pointer,
        Param[1]->Val->Pointer, Param[2]->Val->Pointer);
}

void StdGmtime_r(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    ReturnValue->Val->Pointer = (void*)gmtime_r(Param[0]->Val->Pointer,
        Param[1]->Val->Pointer);
}

void StdTimegm(struct ParseState *Parser, struct Value *ReturnValue,
    struct Value **Param, int NumArgs)
{
    ReturnValue->Val->LongInteger = timegm(Param[0]->Val->Pointer);
}
#endif

/* handy structure definitions */
const char StdTimeDefs[] = "\
typedef long time_t; \
typedef long clock_t; \
struct tm { int tm_sec; int tm_min; int tm_hour; int tm_mday; int tm_mon; int tm_year; int tm_wday; int tm_yday; int tm_isdst; long tm_gmtoff; char *tm_zone; char reserved[64]; }; \
";

/* all string.h functions */
struct LibraryFunction StdTimeFunctions[] =
{
    {StdAsctime, "char *asctime(struct tm *);"},
    {StdClock, "clock_t clock();"},
    {StdCtime, "char *ctime(time_t *);"},
    {StdDifftime, "double difftime(time_t, time_t);"},
    {StdGmtime, "struct tm *gmtime(time_t *);"},
    {StdLocaltime, "struct tm *localtime(time_t *);"},
    {StdLocaltime_r, "struct tm *localtime_r(time_t *, struct tm *);"},
    {StdMktime, "time_t mktime(struct tm *ptm);"},
    {StdTime, "time_t time(time_t *);"},
    {StdStrftime, "int strftime(char *, int, char *, struct tm *);"},
#ifndef WIN32
    {StdStrptime, "char *strptime(char *, char *, struct tm *);"},
    {StdGmtime_r, "struct tm *gmtime_r(time_t *, struct tm *);"},
    {StdTimegm, "time_t timegm(struct tm *);"},
#endif
    {NULL, NULL}
};

/* creates various system-dependent definitions */
void StdTimeSetupFunc(Picoc *pc)
{
    /* verify native sizeof time_t and clock_t */
    if (sizeof(time_t) != 8) {
        printf("ERROR picoc: sizeof time_t = %zd, must be 8\n", sizeof(time_t));
        exit(1);
    }
    if (sizeof(clock_t) != 8) {
        printf("ERROR picoc: sizeof clock_t = %zd, must be 8\n", sizeof(clock_t));
        exit(1);
    }

    /* define CLK_PER_SEC etc. */
    VariableDefinePlatformVar(pc, NULL, "CLOCKS_PER_SEC", &pc->IntType,
        (union AnyValue*)&CLOCKS_PER_SECValue, false);
#ifdef CLK_PER_SEC
    VariableDefinePlatformVar(pc, NULL, "CLK_PER_SEC", &pc->IntType,
        (union AnyValue*)&CLK_PER_SECValue, false);
#endif
#ifdef CLK_TCK
    VariableDefinePlatformVar(pc, NULL, "CLK_TCK", &pc->IntType,
        (union AnyValue*)&CLK_TCKValue, false);
#endif
}

