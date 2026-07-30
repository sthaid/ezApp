#include <stdio.h>

int main()
{
    int i, result;

    // PICOC_ISSUE: ternary ops are evaluated incorrectly

#if 0
    // demo - ternary ops evaluated incorrectly
    for (i = 0; i < 5; i++) {
        result = (i == 0 ? 0 : i == 1 ? 1 : i == 2 ? 2 : i == 3 ? 3 : 4);
        printf("i=%d result=%d\n", i, result);
    }
#endif

    // workaround: add parentheses
    printf("workaround: ...\n");
    for (i = 0; i < 5; i++) {
        result = (i == 0 ? 0 : 
                 (i == 1 ? 1 : 
                 (i == 2 ? 2 : 
                 (i == 3 ? 3 : 4))));
        printf("i=%d result=%d\n", i, result);
    }

    return 0;
}
