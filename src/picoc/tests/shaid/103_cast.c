#include <stdio.h>

int main()
{
    long          s64;
    unsigned long u64;
    int           s32;
    signed char   s8;
    unsigned char u8;

    printf("cast floating point 123.456 to long ...\n");
    s64 = (long)123.456;
    printf("s64 = %ld\n", s64);

    printf("cast int 123 to long ...\n");
    s64 = (long)123;
    printf("s64 = %ld\n", s64);

    printf("cast unsigned char 123 to long ...\n");
    u8 = 123;
    s64 = u8;
    printf("s64 = %ld\n", s64);

    printf("cast unsigned char 150 to long ...\n");
    u8 = 150;
    s64 = u8;
    printf("s64 = %ld\n", s64);

    printf("cast signed char 150 to long ...\n");
    s8 = 150;
    s64 = s8;
    printf("s64 = %ld\n", s64);

    printf("cast long -1 to int ...\n");
    s64 = -1;
    s32 = s64;
    printf("s32 = %d\n", s32);

    printf("cast integer constant 7fffffff to s64 ...\n");
    s64 = 0x7fffffff;
    printf("s64 = %ld\n", s64);

    printf("cast integer constant 100000000 to u64 ...\n");
    u64 = 0x100000000;
    printf("u64 = %ld\n", u64);

    printf("evaluate 's64 = (long)0x7fffffff + 1' ...\n");
    s64 = (long)0x7fffffff + 1;
    printf("s64 = %ld\n", s64);

    printf("evaluate 'u64 = (unsigned long)0xffffffff + 1' ...\n");
    u64 = (unsigned long)0xffffffff + 1;
    printf("u64 = %lu\n", u64);

    printf("evaluate 'u64 = 0xffffffff + 1' ...\n");
    u64 = 0xffffffff + 1;
    printf("u64 = %lu\n", u64);

    printf("evaluate 'u64 = (unsigned long)(0xffffffff + 1)' ...\n");
    u64 = (unsigned long)(0xffffffff + 1);
    printf("u64 = %lu\n", u64);

    return 0;
}
