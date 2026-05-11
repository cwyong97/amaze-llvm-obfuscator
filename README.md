# ==重點在這裡== 剩下都是測試指令
全套+跑程式看結果
```cmd=
./run.sh exec
```
全套但最後不跑程式
```cmd=
./run.sh run
```

sh有問題就:
```cmd=
./run.sh help
```

還有問題再看下方:
---

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

clang-15 -S -emit-llvm -O1 Test/input.c -o Test/input.ll

opt-15 -load-pass-plugin=build/libObfPass.so -passes="substitution" -S Test/input.ll -o Test/output.ll

lli-15 Test/output.ll 
```
## O3優化
```
opt-15 -O3 -S Test/output.ll -o Test/opt_output.ll
```
## 編譯成ELF

```cmd=
clang-15 Test/output.ll -o Workoutput
```