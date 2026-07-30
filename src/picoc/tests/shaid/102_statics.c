#include <stdio.h>

// PICOC_ISSUE: array sizes must be provided if declared 'static'

static char *x1[2]  = {"hello", "world"};
static char *x2[10] = {"hello", "world"};

char *x3[2] = {"hello", "world"};
char *x3b[] = {"hello", "world", "more_world"};  // array size not needed here since not declared 'static'
int z1 = 10;
static int z2 = 11;

void proc(void);

int main()
{
    if (sizeof(x1) == 0 || sizeof(x2) == 0 || sizeof(x3) == 0 || sizeof(x3b) == 0) {
        printf("ERROR sizeof x1,x2,x3 is zero\n");
        return 0;
    }

    printf("x1:  n=%zd %s %s\n", sizeof(x1)/sizeof(x1[0]), x1[0], x1[1]);
    printf("x2:  n=%zd %s %s\n", sizeof(x2)/sizeof(x2[0]), x2[0], x2[1]);
    printf("x3:  n=%zd %s %s\n", sizeof(x3)/sizeof(x3[0]), x3[0], x3[1]);
    printf("x3b: n=%zd %s %s %s\n", sizeof(x3b)/sizeof(x3b[0]), x3b[0], x3b[1], x3b[2]);

    proc();
    proc();
    proc();

    return 0;
}

void proc(void)
{
    static int array[2] = {1, 2};
    static int x = 3;

    if (sizeof(array) == 0) {
        printf("ERROR sizeof array is 0\n");
        return;
    }

    printf("proc: x = %d  array  = %d %d  z1,z2  = %d %d\n", x++, array[0]++, array[1]++, z1++, z2++);
}
