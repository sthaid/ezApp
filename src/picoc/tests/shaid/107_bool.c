#include <stdio.h>
#include <stdbool.h>

typedef struct {
    int i;
    bool b1;
    bool b2;
    int j;
} test_t;

int main()
{
    // picoc does not support offsetof,
    test_t t;
    printf("sizeof(test_t) = %zd\n", sizeof(test_t));
    printf("field offsets: %zd %zd %zd %zd\n",
        (long)&t.i - (long)&t, 
        (long)&t.b1- (long)&t, 
        (long)&t.b2- (long)&t, 
        (long)&t.j - (long)&t);
    printf("\n");

    // test boolean ops, both args bool
    bool b1=false;
    bool b2=true;

    printf("%d %s %d = %s\n", b1, "&&", b1, b1 && b1 ? "true" : "false");
    printf("%d %s %d = %s\n", b1, "&&", b2, b1 && b2 ? "true" : "false");
    printf("%d %s %d = %s\n", b2, "&&", b1, b2 && b1 ? "true" : "false");
    printf("%d %s %d = %s\n", b2, "&&", b2, b2 && b2 ? "true" : "false");
    printf("\n");

    printf("%d %s %d = %s\n", b1, "||", b1, b1 || b1 ? "true" : "false");
    printf("%d %s %d = %s\n", b1, "||", b2, b1 || b2 ? "true" : "false");
    printf("%d %s %d = %s\n", b2, "||", b1, b2 || b1 ? "true" : "false");
    printf("%d %s %d = %s\n", b2, "||", b2, b2 || b2 ? "true" : "false");
    printf("\n");

    printf("%d %s %d = %s\n", b1, "^", b1, b1 ^ b1 ? "true" : "false");
    printf("%d %s %d = %s\n", b1, "^", b2, b1 ^ b2 ? "true" : "false");
    printf("%d %s %d = %s\n", b2, "^", b1, b2 ^ b1 ? "true" : "false");
    printf("%d %s %d = %s\n", b2, "^", b2, b2 ^ b2 ? "true" : "false");
    printf("\n");

    // test boolean ops, one arg bool other arg int
    int int1=7;
    int int2=7;

    printf("%d %s %d = %s\n", int1, "&&", int1, int1 && int1 ? "true" : "false");
    printf("%d %s %d = %s\n", int1, "&&", b2, int1 && b2 ? "true" : "false");
    printf("%d %s %d = %s\n", b2, "&&", int1, b2 && int1 ? "true" : "false");
    printf("%d %s %d = %s\n", b2, "&&", b2, b2 && b2 ? "true" : "false");
    printf("\n");

    printf("%d %s %d = %s\n", b1, "&&", b1, b1 && b1 ? "true" : "false");
    printf("%d %s %d = %s\n", b1, "&&", int2, b1 && int2 ? "true" : "false");
    printf("%d %s %d = %s\n", int2, "&&", b1, int2 && b1 ? "true" : "false");
    printf("%d %s %d = %s\n", int2, "&&", int2, int2 && int2 ? "true" : "false");
    printf("\n");

    return 0;
}
