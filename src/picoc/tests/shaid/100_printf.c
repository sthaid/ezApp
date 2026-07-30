#include <stdio.h>

int main()
{
    int int_var           = 0x7fffffff;
    unsigned int uint_var = 0x80000000;

    printf("INT ...\n");
    printf("%d %u %o %x %X\n", 
        int_var, int_var, int_var, int_var, int_var);
    printf("%d %u %o %x %X\n", 
        int_var, int_var, int_var, int_var, int_var);

    printf("UINT ...\n");
    printf("%d %u %o %x %X\n", 
        uint_var, uint_var, uint_var, uint_var, uint_var);
    printf("%d %u %o %x %X\n", 
        uint_var, uint_var, uint_var, uint_var, uint_var);

    long long_var           = 0x7fffffffffffffff;
    unsigned long ulong_var = 0x8000000000000000;

    printf("LONG ...\n");
    printf("%ld %lu %lo %lx %lX\n", 
        long_var, long_var, long_var, long_var, long_var);
    printf("%ld %lu %lo %lx %lX\n", 
        long_var, long_var, long_var, long_var, long_var);

    printf("ULONG ...\n");
    printf("%ld %lu %lo %lx %lX\n", 
        ulong_var, ulong_var, ulong_var, ulong_var, ulong_var);
    printf("%ld %lu %lo %lx %lX\n", 
        ulong_var, ulong_var, ulong_var, ulong_var, ulong_var);

    short short_var           = 0x7fff;
    unsigned short ushort_var = 0x8000;

    printf("SHORT ...\n");
    printf("%d %u %o %x %X\n", 
        short_var, short_var, short_var, short_var, short_var);
    printf("%d %u %o %x %X\n", 
        short_var, short_var, short_var, short_var, short_var);

    printf("USHORT ...\n");
    printf("%d %u %o %x %X\n", 
        ushort_var, ushort_var, ushort_var, ushort_var, ushort_var);
    printf("%d %u %o %x %X\n", 
        ushort_var, ushort_var, ushort_var, ushort_var, ushort_var);

    return 0;
}
