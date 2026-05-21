// branch.c
#include <stdio.h>
int main() {
    volatile int cc = 10;
    printf("%d\n", cc^20);
    
    char a[] = "This string is gen by char a[]";
    printf("%s\n", a);

    char b[] = {84, 104, 105, 115, 32, 115, 116, 114, 105, 110, 103, 32, 105, 115, 32, 103, 101, 110, 32, 98, 121, 32, 110, 117, 109, 32, 116, 111, 32, 115, 116, 114, 0};
    printf("%s\n", b);

    printf("This string is hardcode in printf\n");
    return 0;
}