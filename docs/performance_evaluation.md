# AmazeLLVM Benchmarking & Overhead Profiling

## 1. Test Environment & Baseline

Performance testing was conducted on two representative real-world libraries:

*   **AES-CBC Cryptography (tiny-AES-c [9])**: Key/IV initialization and a complete encryption/decryption loop on a 1 MB `big_file.bin`.
*   **SQLite Database (SQLite3 [10])**: In-memory database, batch insertion of 10,000 records, and SUM aggregation queries.

Flags legend: `string` = StringObfuscation, `const` = ConstantObfuscation (includes ConstTagger), `split` = SplitBasicBlocks, `bcf` = BogusControlFlow, `sub` = Substitution; `(+O3)` indicates re-optimizing with `opt-15 -O3` after obfuscation.

---

## 2. AES-CBC Full Execution Time Data

The table below lists the AES execution times (in ms) for all 65 obfuscation combinations. The standard deviation in the Mean±σ column is provided by the `hyperfine` [11] measurement tool.

| Pass Combination | Mean (ms) | Min | Max | Expected Time | Penalty | Overhead Ratio |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Baseline (O3)** | 20.6 | 19.6 | 21.6 | 45 | -24.4 | 0.46× |
| **None (Baseline)** | 45.0 | 43.1 | 53.6 | 45 | 0 | 1.00× |
| **None (Baseline) (+O3)** | 46.5 | 45.1 | 51.1 | 45 | +1.5 | 1.03× |
| **bcf** | 44.8 | 43.7 | 47.2 | 44.8 | 0 | 1.00× |
| **bcf (+O3)** | 46.0 | 44.5 | 48.5 | 44.8 | +1.2 | 1.02× |
| **const** | 47.6 | 44.9 | 55.9 | 47.6 | 0 | 1.06× |
| **const (+O3)** | 46.3 | 44.7 | 48.2 | 47.6 | -1.3 | 1.03× |
| **const+bcf** | 46.5 | 44.9 | 50.2 | 47.4 | -0.9 | 1.03× |
| **const+bcf (+O3)** | 46.1 | 44.0 | 47.9 | 47.4 | -1.3 | 1.02× |
| **const+split** | 54.8 | 52.0 | 58.4 | 57.6 | -2.8 | 1.22× |
| **const+split (+O3)** | 46.6 | 45.4 | 48.3 | 57.6 | -11.0 | 1.04× |
| **const+split+bcf ⚠** | 544.5 | 540.5 | 553.7 | 57.4 | +487.1 | 12.10× |
| **const+split+bcf (+O3)** | 62.8 | 60.7 | 66.4 | 57.4 | +5.4 | 1.40× |
| **const+sub** | 1018.1 | 998.9 | 1043.1 | 1039.9 | -21.8 | 22.62× |
| **const+sub (+O3)** | 887.3 | 880.5 | 906.4 | 1039.9 | -152.6 | 19.72× |
| **const+sub+bcf** | 1021.2 | 1010.3 | 1064.3 | 1039.7 | -18.5 | 22.69× |
| **const+sub+bcf (+O3)** | 842.9 | 832.2 | 869.3 | 1039.7 | -196.8 | 18.73× |
| **const+sub+split** | 1037.6 | 1015.4 | 1154.8 | 1049.9 | -12.3 | 23.06× |
| **const+sub+split (+O3)** | 890.4 | 881.9 | 934.2 | 1049.9 | -159.5 | 19.79× |
| **const+sub+split+bcf ⚠** | 1719.4 | 1707.0 | 1769.5 | 1049.7 | +669.7 | 38.21× |
| **const+sub+split+bcf (+O3)** | 874.2 | 865.1 | 897.7 | 1049.7 | -175.5 | 19.43× |
| **split** | 55.0 | 53.8 | 56.2 | 55.0 | 0 | 1.22× |
| **split (+O3)** | 46.5 | 44.4 | 49.8 | 55.0 | -8.5 | 1.03× |
| **split+bcf** | 69.4 | 67.1 | 75.3 | 54.8 | +14.6 | 1.54× |
| **split+bcf (+O3)** | 54.6 | 52.2 | 57.3 | 54.8 | -0.2 | 1.21× |
| **string** | 303.9 | 300.8 | 308.9 | 303.9 | 0 | 6.75× |
| **string (+O3)** | 309.8 | 304.4 | 325.4 | 303.9 | +5.9 | 6.88× |
| **string+bcf** | 310.9 | 305.7 | 329.9 | 303.7 | +7.2 | 6.91× |
| **string+bcf (+O3)** | 307.3 | 302.4 | 316.3 | 303.7 | +3.6 | 6.83× |
| **string+const** | 312.7 | 308.4 | 323.7 | 306.5 | +6.2 | 6.95× |
| **string+const (+O3)** | 308.1 | 303.6 | 314.4 | 306.5 | +1.6 | 6.85× |
| **string+const+bcf** | 314.3 | 311.2 | 322.2 | 306.3 | +8.0 | 6.98× |
| **string+const+bcf (+O3)** | 309.7 | 303.9 | 317.1 | 306.3 | +3.4 | 6.88× |
| **string+const+split** | 326.2 | 320.5 | 345.0 | 316.5 | +9.7 | 7.25× |
| **string+const+split (+O3)** | 309.3 | 303.7 | 328.2 | 316.5 | -7.2 | 6.87× |
| **string+const+split+bcf ⚠** | 1021.7 | 1010.9 | 1064.6 | 316.3 | +705.4 | 22.70× |
| **string+const+split+bcf (+O3)** | 334.5 | 328.1 | 343.7 | 316.3 | +18.2 | 7.43× |
| **string+const+sub** | 1338.6 | 1323.3 | 1383.7 | 1298.8 | +39.8 | 29.75× |
| **string+const+sub (+O3)** | 1094.9 | 1084.7 | 1128.9 | 1298.8 | -203.9 | 24.33× |
| **string+const+sub+bcf** | 1277.4 | 1265.9 | 1341.4 | 1298.6 | -21.2 | 28.39× |
| **string+const+sub+bcf (+O3)** | 1128.0 | 1109.4 | 1159.3 | 1298.6 | -170.6 | 25.07× |
| **string+const+sub+split** | 1346.8 | 1334.6 | 1396.2 | 1308.8 | +38.0 | 29.93× |
| **string+const+sub+split (+O3)** | 1131.2 | 1116.2 | 1170.3 | 1308.8 | -177.6 | 25.14× |
| **string+const+sub+split+bcf ⚠** | 1929.7 | 1911.3 | 1970.9 | 1308.6 | +621.1 | 42.88× |
| **string+const+sub+split+bcf (+O3)** | 1158.9 | 1145.6 | 1193.6 | 1308.6 | -149.7 | 25.75× |
| **string+split** | 324.8 | 320.7 | 329.6 | 313.9 | +10.9 | 7.22× |
| **string+split (+O3)** | 307.0 | 303.4 | 312.2 | 313.9 | -6.9 | 6.82× |
| **string+split+bcf** | 334.8 | 328.8 | 341.9 | 313.7 | +21.1 | 7.44× |
| **string+split+bcf (+O3)** | 314.5 | 310.5 | 320.2 | 313.7 | +0.8 | 6.99× |
| **string+sub** | 1239.9 | 1225.7 | 1281.6 | 1296.2 | -56.3 | 27.55× |
| **string+sub (+O3)** | 1141.4 | 1130.8 | 1182.0 | 1296.2 | -154.8 | 25.36× |
| **string+sub+bcf** | 1289.7 | 1278.9 | 1331.0 | 1296.0 | -6.3 | 28.66× |
| **string+sub+bcf (+O3)** | 1149.8 | 1135.4 | 1174.8 | 1296.0 | -146.2 | 25.55× |
| **string+sub+split** | 1329.9 | 1318.3 | 1367.8 | 1306.2 | +23.7 | 29.55× |
| **string+sub+split (+O3)** | 1157.5 | 1145.1 | 1185.9 | 1306.2 | -148.7 | 25.72× |
| **string+sub+split+bcf ⚠** | 1398.8 | 1384.6 | 1434.2 | 1306.0 | +92.8 | 31.08× |
| **string+sub+split+bcf (+O3)** | 1116.6 | 1107.9 | 1159.4 | 1306.0 | -189.4 | 24.81× |
| **sub** | 1037.3 | 1026.0 | 1067.8 | 1037.3 | 0 | 23.05× |
| **sub (+O3)** | 886.8 | 876.8 | 928.1 | 1037.3 | -150.5 | 19.71× |
| **sub+bcf** | 1009.4 | 999.1 | 1046.8 | 1037.1 | -27.7 | 22.43× |
| **sub+bcf (+O3)** | 873.1 | 863.8 | 916.4 | 1037.1 | -164.0 | 19.40× |
| **sub+split** | 962.9 | 945.9 | 1002.4 | 1047.3 | -84.4 | 21.40× |
| **sub+split (+O3)** | 850.7 | 839.9 | 898.0 | 1047.3 | -196.6 | 18.90× |
| **sub+split+bcf** | 1029.7 | 1013.0 | 1064.6 | 1047.1 | -17.4 | 22.88× |
| **sub+split+bcf (+O3)** | 880.1 | 868.2 | 901.7 | 1047.1 | -167.0 | 19.56× |

⚠ Marks combinations with significant cascade effects (see Section 5).

---

## 3. SQLite Full Execution Time Data

SQLite includes an `O1` baseline (Baseline O1, 2.8 ms) for reference; the non-obfuscated baseline (None Baseline) is 5.5 ms. Therefore, some combinations may show negative penalties (indicating the effectiveness of O3 optimization).

| Pass Combination | Mean (ms) | Min | Max | Expected Time | Penalty | Overhead Ratio |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Baseline (O1)** | 2.8 | 2.7 | 3.0 | 5.5 | -2.7 | 0.51× |
| **None (Baseline)** | 5.5 | 5.3 | 5.8 | 5.5 | 0 | 1.00× |
| **None (Baseline) (+O3)** | 5.0 | 4.8 | 5.2 | 5.5 | -0.5 | 0.91× |
| **bcf** | 5.3 | 5.1 | 5.7 | 5.3 | 0 | 0.96× |
| **bcf (+O3)** | 5.2 | 5.0 | 5.4 | 5.3 | -0.1 | 0.95× |
| **const** | 11.4 | 10.7 | 11.9 | 13.4 | -2.0 | 2.07× |
| **const (+O3) ⚠** | 38.1 | 37.0 | 39.1 | 13.4 | +24.7 | 6.93× |
| **const+bcf** | 14.3 | 13.5 | 14.9 | 13.2 | +1.1 | 2.60× |
| **const+bcf (+O3) ⚠** | 47.8 | 45.2 | 52.6 | 13.2 | +34.6 | 8.69× |
| **const+split** | 12.9 | 12.4 | 13.6 | 14.1 | -1.2 | 2.35× |
| **const+split (+O3) ⚠** | 38.9 | 37.4 | 42.7 | 14.1 | +24.8 | 7.07× |
| **const+split+bcf** | 47.6 | 46.2 | 48.9 | 13.9 | +33.7 | 8.65× |
| **const+split+bcf (+O3) ⚠** | 117.1 | 114.8 | 122.8 | 13.9 | +103.2 | 21.29× |
| **const+sub** | 26.4 | 25.4 | 27.6 | 25.5 | +0.9 | 4.80× |
| **const+sub (+O3) ⚠** | 50.9 | 49.6 | 53.2 | 25.5 | +25.4 | 9.25× |
| **const+sub+bcf** | 29.2 | 27.8 | 30.4 | 25.3 | +3.9 | 5.31× |
| **const+sub+bcf (+O3) ⚠** | 59.6 | 58.1 | 62.0 | 25.3 | +34.3 | 10.84× |
| **const+sub+split** | 27.6 | 26.5 | 30.1 | 26.2 | +1.4 | 5.02× |
| **const+sub+split (+O3) ⚠** | 52.0 | 50.3 | 54.7 | 26.2 | +25.8 | 9.45× |
| **const+sub+split+bcf** | 63.1 | 61.0 | 67.7 | 26.0 | +37.1 | 11.47× |
| **const+sub+split+bcf (+O3) ⚠** | 129.7 | 126.2 | 133.6 | 26.0 | +103.7 | 23.58× |
| **split** | 6.2 | 6.0 | 6.5 | 6.2 | 0 | 1.13× |
| **split (+O3)** | 5.3 | 5.0 | 5.7 | 6.2 | -0.9 | 0.96× |
| **split+bcf** | 9.2 | 9.0 | 9.8 | 6.0 | +3.2 | 1.67× |
| **split+bcf (+O3)** | 6.6 | 6.4 | 7.0 | 6.0 | +0.6 | 1.20× |
| **string** | 284.6 | 276.5 | 302.2 | 284.6 | 0 | 51.75× |
| **string (+O3)** | 274.2 | 269.3 | 286.0 | 284.6 | -10.4 | 49.85× |
| **string+bcf** | 278.9 | 275.5 | 286.7 | 284.4 | -5.5 | 50.71× |
| **string+bcf (+O3)** | 276.6 | 271.3 | 289.5 | 284.4 | -7.8 | 50.29× |
| **string+const** | 307.3 | 302.8 | 311.9 | 292.5 | +14.8 | 55.87× |
| **string+const (+O3) ⚠** | 334.3 | 329.6 | 342.9 | 292.5 | +41.8 | 60.78× |
| **string+const+bcf** | 312.8 | 307.4 | 331.1 | 292.3 | +20.5 | 56.87× |
| **string+const+bcf (+O3) ⚠** | 344.9 | 340.8 | 352.4 | 292.3 | +52.6 | 62.71× |
| **string+const+split** | 306.8 | 302.8 | 312.8 | 293.2 | +13.6 | 55.78× |
| **string+const+split (+O3) ⚠** | 334.1 | 328.1 | 353.0 | 293.2 | +40.9 | 60.75× |
| **string+const+split+bcf** | 350.5 | 345.1 | 359.5 | 293.0 | +57.5 | 63.73× |
| **string+const+split+bcf (+O3) ⚠** | 409.3 | 403.9 | 420.4 | 293.0 | +116.3 | 74.42× |
| **string+const+sub** | 332.3 | 324.6 | 355.7 | 304.6 | +27.7 | 60.42× |
| **string+const+sub (+O3) ⚠** | 356.3 | 349.4 | 370.9 | 304.6 | +51.7 | 64.78× |
| **string+const+sub+bcf** | 336.1 | 330.1 | 344.5 | 304.4 | +31.7 | 61.11× |
| **string+const+sub+bcf (+O3) ⚠** | 366.9 | 361.3 | 392.4 | 304.4 | +62.5 | 66.71× |
| **string+const+sub+split** | 337.4 | 332.5 | 346.4 | 305.3 | +32.1 | 61.35× |
| **string+const+sub+split (+O3) ⚠** | 355.2 | 349.8 | 368.2 | 305.3 | +49.9 | 64.58× |
| **string+const+sub+split+bcf ⚠** | 382.3 | 375.6 | 407.8 | 305.1 | +77.2 | 69.51× |
| **string+const+sub+split+bcf (+O3) ⚠** | 436.7 | 427.1 | 461.7 | 305.1 | +131.6 | 79.40× |
| **string+split** | 281.9 | 278.6 | 287.2 | 285.3 | -3.4 | 51.25× |
| **string+split (+O3)** | 273.9 | 268.0 | 284.0 | 285.3 | -11.4 | 49.80× |
| **string+split+bcf** | 283.2 | 279.8 | 288.3 | 285.1 | -1.9 | 51.49× |
| **string+split+bcf (+O3)** | 274.9 | 271.9 | 280.5 | 285.1 | -10.2 | 49.98× |
| **string+sub** | 304.1 | 297.2 | 324.5 | 296.7 | +7.4 | 55.29× |
| **string+sub (+O3)** | 292.8 | 288.4 | 310.7 | 296.7 | -3.9 | 53.24× |
| **string+sub+bcf** | 301.0 | 295.7 | 306.0 | 296.5 | +4.5 | 54.73× |
| **string+sub+bcf (+O3)** | 295.2 | 288.6 | 314.2 | 296.5 | -1.3 | 53.67× |
| **string+sub+split** | 301.8 | 297.9 | 308.4 | 297.4 | +4.4 | 54.87× |
| **string+sub+split (+O3)** | 292.3 | 288.5 | 296.8 | 297.4 | -5.1 | 53.15× |
| **string+sub+split+bcf** | 309.7 | 303.4 | 331.2 | 297.2 | +12.5 | 56.31× |
| **string+sub+split+bcf (+O3)** | 295.7 | 292.4 | 300.6 | 297.2 | -1.5 | 53.76× |
| **sub** | 17.6 | 16.9 | 18.5 | 17.6 | 0 | 3.20× |
| **sub (+O3)** | 12.9 | 12.6 | 14.0 | 17.6 | -4.7 | 2.35× |
| **sub+bcf** | 17.7 | 16.8 | 18.6 | 17.4 | +0.3 | 3.22× |
| **sub+bcf (+O3)** | 12.9 | 12.5 | 14.3 | 17.4 | -4.5 | 2.35× |
| **sub+split** | 19.0 | 18.2 | 20.4 | 18.3 | +0.7 | 3.45× |
| **sub+split (+O3)** | 13.0 | 12.6 | 13.8 | 18.3 | -5.3 | 2.36× |
| **sub+split+bcf** | 21.1 | 20.5 | 22.0 | 18.1 | +3.0 | 3.84× |
| **sub+split+bcf (+O3)** | 14.2 | 13.4 | 15.5 | 18.1 | -3.9 | 2.58× |

⚠ Marks combinations where O3 abnormally increases overhead on `const`-related combinations, caused by the interaction between O3 and MBA instructions (see Section 5.2).

---

## 4. Size Bloat and Optimization Savings

Obfuscation significantly increases the size of both LLVM IR (`.ll` files) and the final compiled ELF binaries. However, applying `-O3` optimization after obfuscation can effectively mitigate some of this bloat.

### 4.1 Detailed Size and Expansion Rates

The following table shows the exact byte sizes and expansion ratios (relative to the baseline) for both intermediate LLVM IR and final ELF binaries across various obfuscation configurations.

| Obfuscation Config | AES IR Size (Bytes) | IR Bloat | AES ELF Size (Bytes) | AES ELF Bloat | SQLite ELF Size (Bytes) | SQLite ELF Bloat |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **baseline (No Obf)** | 46,281 | 1.00× | 21,248 | 1.00× | 2,314,448 | 1.00× |
| **string** | 72,762 | 1.57× | 25,584 | 1.20× | 2,823,848 | 1.22× |
| **opt_string (+O3)** | 67,048 | 1.45× | 25,584 | 1.20× | 2,770,032 | 1.20× |
| **split_bcf** | 77,324 | 1.67× | 27,824 | 1.31× | 6,650,800 | 2.87× |
| **opt_split_bcf (+O3)** | 56,205 | 1.21× | 23,728 | 1.12× | 5,383,784 | 2.33× |
| **const_sub** | 516,618 | 11.16× | 111,360 | 5.24× | 10,210,120 | 4.41× |
| **opt_const_sub (+O3)** | 363,684 | 7.86× | 111,360 | 5.24× | 9,558,360 | 4.13× |
| **Obf-all** * | 1,187,687 | 25.66× | 169,016 | 7.95× | 36,490,776 | 15.77× |
| **opt_Obf-all (+O3)** * | 621,894 | 13.44× | 152,632 | 7.18× | 24,094,848 | 10.41× |
| **Average Overhead** | 335,492 | 7.25× | 74,535 | 3.51× | 11,485,150 | 4.96× |

*\* Note: `Obf-all` refers to `string+const+sub+split+bcf`.*

### 4.2 O3 Optimization Savings Analysis

Applying `-O3` after obfuscation invokes LLVM's Dead Code Elimination (DCE), Constant Folding, and Instruction Combining, which effectively strips away redundant obfuscation artifacts. 

| Comparison Pair (No O3 vs +O3) | AES IR (Unopt) | AES IR (Opt) | AES IR Saved % | AES ELF (Unopt) | AES ELF (Opt) | AES ELF Saved % | SQLite ELF (Unopt) | SQLite ELF (Opt) | SQLite ELF Saved % |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **String Encryption** | 72,762 | 67,048 | **7.9%** | 25,584 | 25,584 | **0.0%** | 2,823,848 | 2,770,032 | **1.9%** |
| **CFG Flattening (`split+bcf`)** | 77,324 | 56,205 | **27.3%** | 27,824 | 23,728 | **14.7%** | 6,650,800 | 5,383,784 | **19.1%** |
| **Constant Substitution** | 516,618 | 363,684 | **29.6%** | 111,360 | 111,360 | **0.0%** | 10,210,120 | 9,558,360 | **6.4%** |
| **Full Obfuscation** | 1,187,687 | 621,894 | **47.6%** | 169,016 | 152,632 | **9.7%** | 36,490,776 | 24,094,848 | **34.0%** |

**Key Takeaways:**
* When enabling full obfuscation without O3 optimization, the AES LLVM IR size inflates drastically by 25.66 times (from 46 KB to approximately 1.18 MB).
* O3 optimization excels at shrinking the Intermediate Representation (IR), saving nearly **47.6%** of the bloated IR size under full obfuscation.
* For the compiled ELF binaries, the larger SQLite project gains a much higher O3 reduction rate compared to the smaller AES project (e.g., 34.0% vs 9.7% on full obfuscation). This demonstrates that in large projects, O3's optimization passes can more effectively identify and eliminate obfuscation-induced redundancy across broader contexts.

---

## 5. Cascade Effect Analysis

### 5.1 AES Cascade Effect
For the `const+split+bcf` (without O3) combination, execution time explodes from the expected ~57 ms to 544.5 ms (a penalty of +487.1 ms, a 12.10× ratio). This phenomenon stems from the by-design pipelined communication mechanism between passes: `BogusControlFlow` actively injects dummy constants for its opaque predicates and tags them with `!amaze.target.constant`, which are subsequently read by `ConstantObfuscation` and fully fed into the MBA engine for expansion.

However, empirical testing reveals that the performance penalty resulting from this IR-level interaction exhibits a "super-linear growth" that far exceeds expected linear superposition, leading to rapid instruction bloat within loops and massive overhead. The exact root cause and underlying bottleneck of this non-linear surge—brought on by multi-pass collaborative defense (far higher than standard model estimates)—cannot be conclusively determined based solely on current data. This has been listed as future work and will require more advanced profiling tools to trace. Notably, `-O3` re-optimization effectively suppresses this cascade reaction, drastically reducing the execution time back down to 62.8 ms (a penalty of only +5.4 ms).

### 5.2 Abnormal Overhead Increase with O3 in SQLite
When O3 is applied to `const`-related combinations in SQLite, the execution time significantly increases instead of decreasing (e.g., `const (+O3)` jumps from 11.4 ms to 38.1 ms). This contradicts the "O3 effectively suppresses penalties" result seen in AES. The specific cause of this abnormal increase has not yet been confirmed. Preliminary hypotheses suggest it may be related to SQLite's constant structure (such as query plan codes, page sizes, and other high-frequency integers) and the hindering effect of `volatile` instructions on O3 optimization. However, this hypothesis cannot fully explain why `const` remains the primary source of the penalty even when `string` obfuscation is excluded. Further investigation using IR-level tracing or profiling tools is required and is listed as future work.

> **Recommendation**: For large-scale projects characterized by dense constants like SQLite, it is advisable to avoid enabling full constant obfuscation (`-obf-const`). We recommend using Selective Obfuscation Control or increasing the `-const-threshold` (e.g., setting it to 10) to reduce the number of tagged constants. This achieves a better balance between protection strength and performance overhead.
