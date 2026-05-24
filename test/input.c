// branch.c
#include <stdio.h>
int main() {
    volatile char cc[] = "hi";
    if (cc[0] == 'h' && cc[1] == 'i' && cc[2] == '\0') {
        printf("This string is gen by volatile char cc\n");
    }
    volatile char a[] = "This string is gen by char a[]";
    printf("%s\n", a);

    volatile char b[] = {84, 104, 105, 115, 32, 115, 116, 114, 105, 110, 103, 32, 105, 115, 32, 103, 101, 110, 32, 98, 121, 32, 110, 117, 109, 32, 116, 111, 32, 115, 116, 114, 0};
    printf("%s\n", b);


    printf("This string is hardcode in printf\n");
    volatile int x = 150;
    volatile int y = 200;
    printf("The result of x + y is: %d\n", x + y);
    printf("%d\n", x);

    printf("lower\n");
    printf("IamaveryLoooooooooooooooooongString\n");
    char xor[] = "wKJPPWQJMDJPDFMAZ[LQ";
    for (int i = 0; xor[i] != '\0'; i++) {
        xor[i] ^= 0x123;
    }
    printf("%s\n", xor);

    char inserttest[50] = "";
    int set[] = {84, 104, 105, 115, 32, 115, 116, 114, 105, 110, 103, 32, 105, 115, 32, 103, 101, 110, 32, 98, 121, 32, 110, 117, 109, 32, 116, 111, 32, 115, 116, 114, 0};
    for (int i = 0; set[i] != 0; i++) {
        inserttest[i] = set[i];
    }
    printf("%s\n", inserttest);
    return 0;

}