#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

unsigned long long my_license_key = 0;
__attribute__((noinline)) void print_license_secret() {
    printf("Damn How u find these: [%s]\n", "TopSecret!");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <LicenseKey>\n", argv[0]);
        return 1;
    }
    my_license_key = strtoull(argv[1], NULL, 10);
    print_license_secret();
    return 0;
}