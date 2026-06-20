## Test runtime speed
```bash=
hyperfine -N --warmup 50 -M 2000 './baseline_bin' './output_bin' './opt_O3_output_bin' './opt_Os_output_bin'
```
## 初始化
只有Cmakelist與.so就先這樣跑來創造build與cmakefiles
```cmd=
mkdir build && cd build
cmake ..
```
## 製作pass 以substitution為例
```cmd=
cd build
make
cd ..

clang-15 -S -emit-llvm -O1 test/input.c -o test/input.ll

opt-15 -load-pass-plugin=build/libObfPass.so -passes="substitution" -S test/input.ll -o test/output.ll

lli-15 test/output.ll 
```
## O3優化
```
opt-15 -O3 -S test/output.ll -o test/opt_output.ll
```
## 編譯成ELF

```cmd=
clang-15 test/output.ll -o Workoutput
```

```cmd=
clang-15 -S -emit-llvm -O1 test_license.c -o test_license.ll
opt-15 -load-pass-plugin=build/libObfPass.so -O3 -obf-string -str-magic-symbol "my_license_key" -str-magic-value "1234567890" -S test_license.ll -o test_license.ll
clang-15 -O3 -g -o test_license_bin test_license.ll
./test_license_bin

```