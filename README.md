# VisualOS

一個從零開始打造的小型教學用作業系統

## 功能列表（目前完成）

### Bootloader
- 512 bytes MBR 格式
- 進入 32-bit Protected Mode
- 載入 kernel（從 LBA 1 開始）

### Kernel
- 清屏、文字繪製、字元模式
- 自製 printf（k_snprintf）
- 自製 string/mem 函式
- Keyboard IRQ / 輸入緩衝處理
- Shell 進入點 `shell_run()`

### Shell

| 指令 | 功能 |
|------|------|
| `ls` | 列出目錄內容 |
| `cd` | 切換目錄 |
| `pwd` | 顯示目前路徑 |
| `cat` | 顯示文件內容 |
| `touch <file>` | 建立空白文件 |
| `mkdir <dir>` | 建立資料夾 |
| `mkdir -p <dir/subdir>` | 遞迴建立資料夾 |
| `rm <file/dir>` | 刪除項目 |
| `clear` | 清除螢幕 |
| `echo` | 輸出內容 |
| `debug on/off` | 開啟／關閉 debug mode |
| `help` | 顯示所有指令 |

### 虛擬檔案系統（VFS）

- 靜態節點池（無動態記憶體）
- 每個 directory 有 children[] 指標陣列
- 支援絕對/相對路徑
- 支援 `.`, `..`
- 支援 `mkdir -p`
- 支援 `touch`
- 支援 `rm`

---

## 建置方式

### 環境需求

需要 i686-elf 工具鏈。

**編譯**

```bash
cmake -G "MinGW Makefiles" -B build -S .
cmake --build build
```

**執行**

```bash
cmake --build build --target run-qemu
```