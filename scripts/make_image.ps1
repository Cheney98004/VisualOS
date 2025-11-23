# ================================================
#   VisualOS - FAT16 image builder (No parameters)
# ================================================

$repoRoot  = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildDir  = Join-Path $repoRoot "build"

$BootBin   = Join-Path $buildDir "boot.bin"
$KernelBin = Join-Path $buildDir "kernel.bin"
$SnakeElf  = Join-Path $buildDir "snake.elf"
$TestElf   = Join-Path $buildDir "test.elf"
$OutputImg = Join-Path $buildDir "os.img"
$HelpTxt   = Join-Path $repoRoot "res/help.txt"
$PythonScript = Join-Path $repoRoot "fat16.py"

Write-Host "===== VisualOS Image Builder ====="
Write-Host "Boot     = $BootBin"
Write-Host "Kernel   = $KernelBin"
Write-Host "Snake    = $SnakeElf"
Write-Host "Test     = $TestElf"
Write-Host "Help.txt = $HelpTxt"
Write-Host "Output   = $OutputImg"

# Remove old image
if (Test-Path $OutputImg) {
    Remove-Item $OutputImg -Force
}

# ⚠ 不能在 --% 之後使用變數！
# 因此組成完整的命令字串，給 PowerShell Invoke-Expression 執行
$cmd = "python --% ""$PythonScript"" ""$BootBin"" ""$KernelBin"" ""$HelpTxt"" ""$SnakeElf"" ""$TestElf"" ""$OutputImg"""

Write-Host "Running:"
Write-Host "  $cmd"
Write-Host ""

Invoke-Expression $cmd

if ($LASTEXITCODE -ne 0) {
    throw "fat16.py failed: exit code $LASTEXITCODE"
}

Write-Host "===== Done: FAT16 image created successfully ====="
