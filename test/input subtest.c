// branch.c
#include <stdio.h>
int main() {
    // int x,y;
    // printf("Enter two integers: ");
    // scanf("%d %d", &x, &y);
    // printf("The sum is: %d\n", x + y);
    // volatile __int64_t a = 123;
    // volatile __int64_t notA = ~a;
    // volatile __int64_t b = notA + 1;
    // printf("Test 1 -a: %ld\n", b);
    // if (a == 123) {
    //     printf("a is 123\n");
    // }
    // volatile char  m = 20;
    // volatile char  n = 20;
    // volatile char  res = m - n;
    // printf("Shorts test 100-20 = %hd\n", res);
    volatile __int64_t a = -100;
    volatile __int64_t b = -9223372036854775807;
    printf("Test 1 (a + b): %ld\n", a + b);

    // volatile int c = 42;
    // volatile int d = 2147483647;
    // volatile int d = 2;
    // printf("Test 2 (c - d): %d\n", c - d);
    // int result = c-d;
    // int i = 0;
    // for (i = 0; i < 100; i++) {
    //     result = result * -1;
    // }
    // printf("%d\n", i);
    // printf("result %d\n", result);

    // volatile int e = -45;
    // volatile int f = 2147483647;
    // printf("Test 3 (e * f): %d\n", e * f);

    // volatile int g = 20;
    // volatile int h = -4;
    // printf("Test 4 (g / h): %d\n", g / h);

    // volatile int i = 2147483647;
    // volatile int j = 3;
    // printf("Test 5 (i %% j): %d\n", i % j);

    // volatile int k = -2147483647;
    // volatile int l = -2;
    // printf("Test 6 (k + l * 3): %d\n", k + l * 3);

    // volatile int xora = 50;
    // volatile int xorb = 10;
    // printf("Test 7 (a xor b): %d\n", xora ^ xorb);

    // volatile int m = 8;
    // volatile int n = 2;
    // printf("Test 8 (m << n * 3): %d\n", m << n * 3);

    // printf("All correct? \n");
    // printf("%ld %ld %d %d %d %d %d %d\n", a + b == 9223372036854775709, c - d == -2147483605, e * f == -2147483603, g / h == -5, i % j == 1, k + l * 3 == 2147483643, (xora ^ xorb) == 56, m << n * 3 == 512);
    return 0;
}