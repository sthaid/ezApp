#include <stdio.h>
#include <math.h>

void test_doubles(void) 
{
    printf("test_doubles ...\n");

    // variables
    double a = 12.34 + 56.78;
    printf("%f\n", a);

    // infix operators
    printf("%f\n", 12.34 + 56.78);
    printf("%f\n", 12.34 - 56.78);
    printf("%f\n", 12.34 * 56.78);
    printf("%f\n", 12.34 / 56.78);

    // comparison operators
    printf("%d %d %d %d %d %d\n", 12.34 < 56.78, 12.34 <= 56.78, 12.34 == 56.78, 12.34 >= 56.78, 12.34 > 56.78, 12.34 != 56.78);
    printf("%d %d %d %d %d %d\n", 12.34 < 12.34, 12.34 <= 12.34, 12.34 == 12.34, 12.34 >= 12.34, 12.34 > 12.34, 12.34 != 12.34);
    printf("%d %d %d %d %d %d\n", 56.78 < 12.34, 56.78 <= 12.34, 56.78 == 12.34, 56.78 >= 12.34, 56.78 > 12.34, 56.78 != 12.34);

    // assignment operators
    a = 12.34;
    a += 56.78;
    printf("%f\n", a);

    a = 12.34;
    a -= 56.78;
    printf("%f\n", a);

    a = 12.34;
    a *= 56.78;
    printf("%f\n", a);

    a = 12.34;
    a /= 56.78;
    printf("%f\n", a);

    // prefix operators
    printf("%f\n", +12.34);
    printf("%f\n", -12.34);

    // type coercion
    a = 2;
    printf("%f\n", a);
    printf("%f\n", sin(2));
}

void test_floats(void)
{
    printf("test_floats ...\n");

    // variables
    float a = 12.34 + 56.78;
    printf("%f\n", a);

    // infix operators
    printf("%f\n", 12.34f + 56.78f);
    printf("%f\n", 12.34f - 56.78f);
    printf("%f\n", 12.34f * 56.78f);
    printf("%f\n", 12.34f / 56.78f);

    printf("%f\n", 12.34 + 56.78f);
    printf("%f\n", 12.34 - 56.78f);
    printf("%f\n", 12.34 * 56.78f);
    printf("%f\n", 12.34 / 56.78f);

    printf("%f\n", 12.34f + 56.78);
    printf("%f\n", 12.34f - 56.78);
    printf("%f\n", 12.34f * 56.78);
    printf("%f\n", 12.34f / 56.78);

    // comparison operators
    printf("%d %d %d %d %d %d\n", 12.34f < 56.78f, 12.34f <= 56.78f, 12.34f == 56.78f, 12.34f >= 56.78f, 12.34f > 56.78f, 12.34f != 56.78f);
    printf("%d %d %d %d %d %d\n", 12.34f < 12.34f, 12.34f <= 12.34f, 12.34f == 12.34f, 12.34f >= 12.34f, 12.34f > 12.34f, 12.34f != 12.34f);
    printf("%d %d %d %d %d %d\n", 56.78f < 12.34f, 56.78f <= 12.34f, 56.78f == 12.34f, 56.78f >= 12.34f, 56.78f > 12.34f, 56.78f != 12.34f);

    // assignment operators
    a = 12.34;
    a += 56.78;
    printf("%f\n", a);

    a = 12.34;
    a -= 56.78;
    printf("%f\n", a);

    a = 12.34;
    a *= 56.78;
    printf("%f\n", a);

    a = 12.34;
    a /= 56.78;
    printf("%f\n", a);

    // prefix operators
    printf("%f\n", +12.34f);
    printf("%f\n", -12.34f);

    // type coercion
    a = 2;
    printf("%f\n", a);
    printf("%f\n", sin(2));
}

int main()
{
    // call test procs
    test_doubles();
    test_floats();

    // assignemnt
    printf("test assignment ...\n");
    float flt;
    double dbl;
    flt = dbl = 12.34f;
    printf("flt = %0.3f dbl = %0.3f\n", flt, dbl);
    dbl = flt = 12.34f;
    printf("dbl = %0.3f flt = %0.3f\n", dbl, flt);

    return 0;
}
