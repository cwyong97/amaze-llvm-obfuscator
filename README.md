# 跑測試
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

---
# 要新增自訂 Pass 的標準流程

### 1. 建立 Pass 檔案
*   在 `Pass/` 目錄下建立 `<PassName>.h` 與 `<PassName>.cpp`。
*   在 `<PassName>.h` 中定義 Pass 類別，繼承 `PassInfoMixin<PassName>`。
*   在 `<PassName>.cpp` 中引入自己的標頭檔 `#include "<PassName>.h"`，並實作 `run` 函數。
*   **注意：** 如果有內部使用的輔助函數（例如 `applyOneRound`），請務必包在 `namespace { ... }` 匿名命名空間內，避免連結期（Linker）符號重複定義錯誤。

### 2. 更新CMake
*   打開 `CMakeLists.txt`。
*   在 `add_library(ObfPass MODULE ...)` 的列表中，手動加入新建立的 `Pass/<PassName>.cpp`。
*   **注意：** 如果這個 Pass 有自己專屬的內部數學或工具資料夾，記得要用 `include_directories` 把路徑加進去。

### 3. 去登記 (PipelineRegistration.cpp)
*   **引入標頭檔：** 在最頂部加上 `#include "<PassName>.h"`。
*   **新增參數開關：** 新增一個 `static cl::opt<bool> Enable<PassName>` 控制參數（可選，用於搭配自動優化管線）。
*   **註冊手動管線解析：** 在 `PB.registerPipelineParsingCallback` 裡面，加入對應的名字判斷：
    ```cpp
    if (Name.equals("新pass的小寫名稱")) {
        FPM.addPass(<PassName>());
        return true;
    }
    ```
    *(這樣你才能用 `opt-15 -passes=新pass名稱` 進行精準手動測試。)*
*   **註冊自動最佳化管線**： 在 PB.registerOptimizerLastEPCallback 裡面，加入開關判斷：
    ```cpp
    if (EnableAllObf || Enable<PassName>) {
        MPM.addPass(createModuleToFunctionPassAdaptor(<PassName>()));
    }
    ```

