#include <unistd.h>
#include <stdio.h>

int authenticate(int input) {
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += (input ^ i);
    }
    if (sum == 417) {
        return 1;
    } else {
        return 0;
    }
}

int main() {
    int input;
    read(0, &input, sizeof(input));
    if (authenticate(input)) {
        printf("SUCCESS\n");
    } else {
        printf("FAIL\n");
    }
    return 0;
}
