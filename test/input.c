#include <stdio.h>
#include <string.h>
#include <stdint.h>

// 1. 測試 Switch-Case 結構安全過濾器 (驗證 SwitchInst 常數防禦是否 100% 成功阻止 Crash)
void test_switch_case(int value) {
    printf("=== Test Switch-Case Safety Filter ===\n");
    switch (value) {
        case 12345:
            printf("Matched sensitive magic case 12345!\n");
            break;
        case 67890:
            printf("Matched sensitive magic case 67890!\n");
            break;
        default:
            printf("Fallback default case: %d\n", value);
            break;
    }
}

// 2. 測試局部變數堆疊配置 (驗證 mem2reg 在 Entry Block 優化 Alloca 的保留度)
int test_local_alloca_preservation(int val) {
    printf("=== Test Local Alloca Preservation ===\n");
    volatile int x = val + 10;
    volatile int y = val * 5;
    volatile int array[4] = {1, 2, 3, 4};
    return x + y + array[2];
}

// 3. 測試字串解密邊界與去重 (String Obfuscation Boundaries & Deduplication)
void test_string_obfuscation() {
    printf("=== Test String Decryption ===\n");
    volatile char short_str[] = "Short";
    volatile char exact_8[] = "12345678";
    volatile char empty_str[] = "";
    volatile char escape_chars[] = "Tab:\t, Newline:\n, Backslash:\\";

    printf("short: [%s]\n", short_str);
    printf("exact_8: [%s]\n", exact_8);
    printf("empty: [%s]\n", empty_str);
    printf("escaped: [%s]\n", escape_chars);

    // 去重複化測試 (多處引用同一個字串常數)
    printf("Deduplication test 1: %s\n", "DeduplicateMe");
    printf("Deduplication test 2: %s\n", "DeduplicateMe");
}

// 4. 測試算術代換與高頻常數標記混淆 (Substitution & Constant Obfuscation)
void test_arithmetic_and_constants() {
    printf("=== Test MBA Substitution & Constant Obfuscation ===\n");
    volatile int a = 100;
    volatile int b = 50;

    // 這些算術指令將被替換為隨機線性 MBA 表達式
    int add_res = a + b;
    int sub_res = a - b;
    int and_res = a & b;
    int or_res  = a | b;
    int xor_res = a ^ b;

    printf("Add: %d, Sub: %d, And: %d, Or: %d, Xor: %d\n", add_res, sub_res, and_res, or_res, xor_res);

    // 定向常數標籤混淆 (高頻出現的敏感魔術數)
    volatile int64_t key1 = 0x123456789ABCDEF0LL;
    volatile int64_t key2 = 0x123456789ABCDEF0LL;
    printf("Key 1: %lld, Key 2: %lld\n", (long long)key1, (long long)key2);
}

// 5. 測試複雜控制流、結構體與有號/無號整數邊界 (Nested Loops, Structs & Signed/Unsigned Boundaries)
struct Point {
    int32_t x;
    int32_t y;
};

void test_complex_loop_and_signedness() {
    printf("=== Test Complex Control Flow & Boundaries ===\n");

    // 測試有號與無號極限常數 (防範 sign-extension 異常)
    volatile int32_t max_signed = 2147483647; // INT_MAX
    volatile uint32_t max_unsigned = 4294967295U; // UINT_MAX
    
    int64_t sum_bounds = (int64_t)max_signed + (int64_t)max_unsigned;
    printf("Signed/Unsigned Bounds Sum: %lld\n", (long long)sum_bounds);

    // 複雜控制流：雙重巢狀迴圈與雜湊值運算
    volatile uint32_t hash = 0x811C9DC5;
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 3; ++j) {
            hash ^= (uint32_t)(i * 3 + j);
            hash *= 16777619; // FNV-1a 32-bit prime
        }
    }
    printf("FNV hash result: 0x%X\n", hash);

    // 結構體堆疊操作
    struct Point p = {1000, 2000};
    volatile int res = p.x + p.y;
    printf("Struct Point result: %d\n", res);
}

int main() {
    test_switch_case(12345);
    test_switch_case(999);
    
    int alloca_res = test_local_alloca_preservation(10);
    printf("Alloca preservation result: %d\n", alloca_res);

    test_string_obfuscation();
    test_arithmetic_and_constants();
    test_complex_loop_and_signedness();

    return 0;
}
