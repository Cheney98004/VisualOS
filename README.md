# VisualOS

**VisualOS** 是一個從零開始打造的 **32-bit x86 作業系統**。  
系統從 **FAT16 bootloader → Protected Mode kernel → Shell → FAT16 檔案系統** 

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
- 自製 `printf` / 字串 / 記憶體函式（無 libc）
- 鍵盤 IRQ1 中斷（支援 ASCII 輸入）
- Shell 執行主迴圈  
- FAT16 檔案系統核心（read/write/delete/mkdir/cluster allocation）

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

---

# Shell 指令

| 指令 | 功能 |
|------|------|
| `ls` | 顯示目錄內容 |
| `cd <dir>` | 切換資料夾 |
| `pwd` | 顯示目前路徑 |
| `cat <file>` | 顯示檔案內容 |
| `touch <file>` | 建立空檔案 |
| `rm <file>` | 刪除檔案 |
| `write <file> <text>` | 寫文字到檔案 |
| `mkdir <dir>` | 建立資料夾 |
| `clear` | 清除畫面 |
| `help` | 所有指令說明 |

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
+-------------------+
| BIOS / QEMU       |
+-------------------+
         |
         v
+-------------------+
| Bootloader (FAT16)
| - Load KERNEL.BIN
| - Protected Mode
+-------------------+
         |
         v
+---------------------------+
| Kernel (32-bit)          |
|  - VGA Text Mode         |
|  - Keyboard IRQ          |
|  - Shell                 |
|  - FAT16 Filesystem      |
+---------------------------+
         |
         v
+---------------------------+
| FAT16 Disk (os.img)       |
|  - directories / files    |
|  - FAT table              |
|  - cluster allocation     |
+---------------------------+
```