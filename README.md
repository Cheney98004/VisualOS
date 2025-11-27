# VisualOS

**VisualOS** 是一個從零開始打造的 **32-bit x86 作業系統**。  
採用 FAT16 Bootloader → Protected Mode Kernel → Shell → FAT16 File System → User Apps (ELF) 的完整架構。

---

# 目前功能（已完成）

## Bootloader（FAT16 開機 + Protected Mode）
- 512 bytes FAT16 開機磁區
- BIOS INT 13h 擴展讀取
- 從 FAT16 載入 `KERNEL.BIN`
- 啟用 A20
- 設置 GDT、進入 32-bit Protected Mode
- 跳入 Kernel

---

## Kernel（32-bit）
- VGA 文字模式（80×25）
- 自製 `printf` / 字串 / 記憶體函式
- 鍵盤 IRQ1 中斷（支援 ASCII 輸入）
- Shell 執行主迴圈  
- FAT16 檔案系統核心（read/write/delete/mkdir/cluster allocation）
- ELF 執行檔載入器（User Programs）
- 初步系統呼叫（syscall framework）

---

# FAT16 檔案系統（真正寫入到 os.img）
VisualOS 已從 RAMFS 完全移除，改為 **FAT16 寫入磁碟**，支援：

### 目錄功能
- `.`、`..` 自動生成
- 子目錄建立（mkdir）
- 目錄切換 `cd`
- 完整 path 解析（支援 `/path/to/dir`）
- FAT 目錄 entry 正確建立/刪除

### 檔案功能
- 建立空檔案（touch）
- 刪除檔案（rm）
- 寫入檔案（write）
- 讀取檔案（cat）
- 真正寫入/修改 FAT table 與 data clusters

### FAT table 操作
- cluster 分配
- cluster chain 追蹤（fat_next）
- cluster 釋放（刪除檔案時釋放 FAT）
- Root directory + Subdirectory 完整支援

### Permission Flags（擴充 FAT16）
VisualOS 使用 FAT16 內的 `flags` byte 建立類似 Linux 的權限：

| Flag | 說明 |
|------|------|
| r | 可讀 |
| w | 可寫 |
| x | 可執行 |
| h | 隱藏 |
| s | 系統檔案（不可刪除） |
| i | immutable（完全不可更名/刪除/寫入） |

開機時自動將 `KERNEL.BIN` 設為 **system + immutable**。

---

# Shell 指令

| 指令 | 功能 |
|------|------|
| `ls` / `ls -l` / `ls -a` | 列出檔案 |
| `pwd` | 顯示目前路徑 |
| `cd <dir>` | 切換資料夾 |
| `cat <file>` | 顯示檔案內容 |
| `touch <file>` | 建立空檔案 |
| `rm <file>` | 刪除檔案 |
| `write <file> <text>` | 寫入文字 |
| `mkdir <dir>` | 建立目錄 |
| `chmod [+rwxhsi] <file>` | 修改 FAT16 flags |
| `rename <old> <new>` | 更名 |
| `exec <program>` | 執行 ELF 程式 |
| `mem` | 顯示記憶體資訊 |
| `clear` | 清除畫面 |
| `help` | 列出指令 |

---

## 🧩 User Applications（ELF）
VisualOS 支援執行獨立 ELF 程式：

- `snake.elf`  
- `test.elf`

ELF loader 功能包含：
- 讀取 ELF header  
- 載入每個 segment  
- 轉跳至程式 entry point 執行  

程式結束後返回 shell。

---

# 編譯與執行

需要工具：
- **i686-elf-gcc toolchain**
- **NASM**
- **CMake**
- **QEMU**

編譯：

```bash
cmake -G "MinGW Makefiles" -B build -S .
cmake --build build
```

執行：

```bash
cmake --build build --target run-qemu
```

或手動啟動：

```bash
qemu-system-i386 -drive format=raw,file=build/os.img
```

系統架構

```
+--------------------------------------------------+
| BIOS / QEMU                                      |
+--------------------------------------------------+
                |
                v
+--------------------------------------------------+
| Bootloader (boot.asm)                            |
|  - FAT16 root directory scan                      |
|  - 找 KERNEL.BIN                                  |
|  - EXT INT 13h 讀取 kernel                        |
|  - 啟用 A20                                       |
|  - GDT + Protected Mode                           |
|  - 跳到 32-bit KERNEL                             |
+--------------------------------------------------+
                |
                v
+--------------------------------------------------+
| Kernel (kernel.c)                                |
|  - .data / .bss 初始化                            |
|  - Terminal + Keyboard IRQ                        |
|  - Syscall table 註冊                             |
|  - FAT16 初始化 + 保護 KERNEL.BIN                 |
|  - Shell                                          |
+--------------------------------------------------+
                |
                v
+--------------------------------------------------+
| FAT16 Filesystem                                 |
|  - read / write / delete                          |
|  - mkdir / cd / pwd                               |
|  - 8.3 格式處理                                   |
|  - cluster allocation                             |
+--------------------------------------------------+
                |
                v
+--------------------------------------------------+
| User Programs (ELF)                               |
|  - snake.elf                                      |
|  - test.elf                                       |
|  - exec                                          |
+--------------------------------------------------+
```