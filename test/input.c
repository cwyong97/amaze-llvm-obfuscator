#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

void test_string_obfuscation() {
    printf("=== Test String Obfuscation Boundaries ===\n");

    // 1. Length exactly multiple of 8 (8, 16, 24, 32 bytes)
    volatile char s8[] = "12345678";
    volatile char s16[] = "1234567812345678";
    volatile char s24[] = "123456781234567812345678";
    volatile char s32[] = "12345678123456781234567812345678";

    printf("s8: [%s] (len: %zu)\n", s8, strlen((char*)s8));
    printf("s16: [%s] (len: %zu)\n", s16, strlen((char*)s16));
    printf("s24: [%s] (len: %zu)\n", s24, strlen((char*)s24));
    printf("s32: [%s] (len: %zu)\n", s32, strlen((char*)s32));

    // 2. Empty string
    volatile char empty[] = "";
    printf("empty: [%s] (len: %zu)\n", empty, strlen((char*)empty));

    // 3. Special characters & escaping
    volatile char special[] = "Hello\n\tWorld!\\";
    printf("special: [%s]\n", special);

    // 4. Deduplication: Reference the same string multiple times
    printf("Duplicate check 1: %s\n", "DeduplicateMe");
    printf("Duplicate check 2: %s\n", "DeduplicateMe");
    printf("Duplicate check 3: %s\n", "DeduplicateMe");
}

void test_substitution() {
    printf("=== Test Substitution / Binary Operators & Constant Obfuscation ===\n");

    // 1. Large constant integers (64-bit boundaries)
    volatile int64_t large_pos = 922337203685477580LL; // safe under 64-bit limit
    volatile int64_t large_neg = -922337203685477580LL;
    volatile int64_t zero = 0;
    volatile int64_t neg_one = -1;

    printf("large_pos: %lld\n", (long long)large_pos);
    printf("large_neg: %lld\n", (long long)large_neg);
    printf("zero: %lld\n", (long long)zero);
    printf("neg_one: %lld\n", (long long)neg_one);

    // 2. Constants of different types (i8, i16, i32, i64)
    volatile int8_t  c_i8  = 120;
    volatile int16_t c_i16 = 30000;
    volatile int32_t c_i32 = 2000000000;
    volatile int64_t c_i64 = 800000000000LL;

    printf("c_i8: %d, c_i16: %d, c_i32: %d, c_i64: %lld\n", c_i8, c_i16, c_i32, (long long)c_i64);

    // 3. Binary operators boundary testing
    volatile int a = 100;
    volatile int b = 50;

    printf("Add (100 + 50): %d\n", a + b);
    printf("Sub (100 - 50): %d\n", a - b);
    printf("And (100 & 50): %d\n", a & b);
    printf("Or  (100 | 50): %d\n", a | b);
    printf("Xor (100 ^ 50): %d\n", a ^ b);

    // 4. Binary operations with boundaries (0, -1)
    printf("Add zero: %d\n", a + 0);
    printf("Sub neg_one: %d\n", a - (-1));
    printf("And zero: %d\n", a & 0);
    printf("And all_ones: %d\n", a & -1);
    printf("Or zero: %d\n", a | 0);
    printf("Xor zero: %d\n", a ^ 0);
    printf("Xor all_ones: %d\n", a ^ -1);
}

int main() {
    test_string_obfuscation();
    test_substitution();
    return 0;
}