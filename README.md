# VisualOS

**VisualOS** 是一個從零開始打造的 **教學用 32-bit 作業系統**，  
目標是從最基礎的 bootloader → kernel → shell → 檔案系統，  
完整展示 OS 開發的核心原理。

目前系統已能啟動、進入自己的 shell、支援基本命令，  
並具有自製的 RAM 型虛擬檔案系統架構。

---

# 功能現況（已完成）

## Bootloader（FAT16 + Protected Mode）
- 512 bytes 可開機 FAT16 boot sector
- BIOS INT 13h 擴展讀取
- 從 FAT16 讀取 `KERNEL.BIN`
- 將 kernel 載入到 **0x10000**
- 啟用 A20
- 建立 GDT
- 進入 **32-bit Protected Mode**
- 跳入 kernel 入口點

---

## Kernel（自製核心）
- Text Mode VGA 輸出（80×25）
- `printf` / 字串函式 / 記憶體函式（無 libc）
- IRQ1 鍵盤中斷（可組合 ASCII 輸入）
- Basic PMM（簡易實體記憶體管理）
- Shell 入口：`shell_run()`

---

## Shell（基本使用者介面）

| 指令 | 功能 |
|------|------|
| `ls` | 列出目錄內容 |
| `cd <dir>` | 切換目錄 |
| `pwd` | 顯示當前路徑 |
| `cat <file>` | 顯示文件內容 |
| `touch <file>` | 建立空白文件 |
| `mkdir <dir>` | 建立資料夾 |
| `mkdir -p <dir/subdir>` | 遞迴建立資料夾 |
| `rm <file/dir>` | 刪除檔案或資料夾 |
| `echo` | 顯示訊息 |
| `clear` | 清除畫面 |
| `help` | 顯示所有指令 |

---

## VFS（虛擬檔案系統 / RAMFS）
目前的檔案系統為 **完全在記憶體中的 RAM FS**：

- directory / file nodes
- children 陣列（無動態配置 malloc）
- 支援 `.`, `..`
- 支援遞迴 mkdir
- 支援建立空檔案（touch）
- 支援刪除（rm）
- 支援路徑解析

**注意：目前所有檔案皆儲存在 RAM。重啟後會失去所有資料（尚未實作磁碟寫入）。**

---

# 編譯方式

### 需求工具
- **i686-elf-gcc toolchain**
- **NASM**
- **CMake**
- **QEMU**

建置：

```bash
cmake -G "MinGW Makefiles" -B build -S .
cmake --build build
```

執行：

```bash
cmake --build build --target run-qemu
```

# 系統架構概覽
```
+-------------------+
| BIOS / QEMU       |
+-------------------+
         |
         v
+-------------------+
| Bootloader (16-bit)
| - FAT16 loader
| - Protected Mode
+-------------------+
         |
         v
+---------------------------+
| Kernel (32-bit)          |
|  - VGA text output       |
|  - IRQ / keyboard        |
|  - PMM / device stubs    |
|  - Shell                 |
+---------------------------+
         |
         v
+---------------------------+
| RAMFS (Virtual FS)        |
|  - directories/files      |
|  - path resolving         |
|  - ls/cat/touch/mkdir/... |
+---------------------------+
```

# 未來開發

**下一階段開發目標是:**
- **FAT table 讀寫**
- **cluster 分配 / chain 釋放**
- **root directory entry 寫回**
- **data 區塊寫入磁碟**