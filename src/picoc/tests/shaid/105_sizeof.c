#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main()
{
    printf("sizoef(char)           = %zd\n", sizeof(char));
    printf("sizoef(short)          = %zd\n", sizeof(short));
    printf("sizoef(int)            = %zd\n", sizeof(int));
    printf("sizoef(long)           = %zd\n", sizeof(long));
    printf("sizoef(float)          = %zd\n", sizeof(float));
    printf("sizoef(double)         = %zd\n", sizeof(double));

    printf("sizoef(size_t)         = %zd\n", sizeof(size_t));
    printf("sizoef(ssize_t)        = %zd\n", sizeof(ssize_t));
    printf("sizoef(off_t)          = %zd\n", sizeof(off_t));
    printf("sizoef(time_t)         = %zd\n", sizeof(time_t));
    printf("sizoef(clock_t)        = %zd\n", sizeof(clock_t));

    printf("sizeof(1)              = %zd\n", sizeof(1));
    printf("sizeof(1L)             = %zd\n", sizeof(1L));
    printf("sizeof(0xffffffff)     = %zd\n", sizeof(0xffffffff));
    printf("sizeof(0xffffffffL)    = %zd\n", sizeof(0xffffffffL));
    printf("sizeof(0x100000000)    = %zd\n", sizeof(0x100000000));
    printf("sizeof(0x100000000L)   = %zd\n", sizeof(0x100000000L));
    printf("sizeof(4000000000)     = %zd\n", sizeof(4000000000));

    printf("%x\n",  1);
    printf("%lx\n", 1L);
    printf("%x\n",  0xffffffff);
    printf("%lx\n", 0xffffffffL);
    printf("%lx\n", 0x100000000);
    printf("%lx\n", 0x100000000L);
    printf("%ld\n", 4000000000);

    return 0;
}
