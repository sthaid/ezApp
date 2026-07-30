#include <stdio.h>

#define IS_DEF

int main()
{
#if 0
    printf("ERROR shouldnt print 1\n");
#else
    printf("should print 1\n");
#endif

#if 1
    printf("should print 2\n");
#endif

#ifdef NOT_DEF
    printf("ERROR shouldnt print 3\n");
#else
    printf("should print 3\n");
#endif

#ifdef IS_DEF
    printf("should print 4\n");
#else
    printf("ERROR shouldnt print 4\n");
#endif

#ifdef IS_DEF
    #if 1
    printf("should print 5\n");
    #endif
#endif

#if 1
    if (1) printf("should print 6\n");
#endif

#if 0
    if (1) printf("should not print 7\n");
#endif

#if 1
    if (1) printf("should print 8\n");
#else
    if (1) printf("should not print 9\n");
#endif

#if 0
    if (1) printf("should not print 10\n");
#else
    if (1) printf("should print 11\n");
#endif

#if 1
    #ifdef DEFINED
        if (1) printf("should print 12\n");
    #endif
#else
    #ifdef DEFINED
        if (1) printf("should not print 13\n");
    #endif
#endif

#if 0
    #ifdef DEFINED
        if (1) printf("should print 14\n");
    #endif
#else
    #ifdef DEFINED
        if (1) printf("should not print 15\n");
    #endif
#endif

    return 0;
}
