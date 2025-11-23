param(
    [Parameter(Mandatory = $true)][string]$BootBin,
    [Parameter(Mandatory = $true)][string]$KernelBin,
    [Parameter(Mandatory = $true)][string]$AppElf,
    [Parameter(Mandatory = $true)][string]$OutputImg
)

if (-not (Test-Path $BootBin)) { throw "Boot bin not found: $BootBin" }
if (-not (Test-Path $KernelBin)) { throw "Kernel bin not found: $KernelBin" }
if (-not (Test-Path $AppElf))    { throw "App ELF not found: $AppElf" }

# repo root
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

# fat16 builder script
$scriptPath = Join-Path $repoRoot "fat16.py"
if (-not (Test-Path $scriptPath)) {
    throw "fat16.py not found at $scriptPath"
}

# locate help.txt
$helpPath = Join-Path $repoRoot "res/help.txt"
if (-not (Test-Path $helpPath)) {
    throw "help.txt not found at $helpPath"
}

Write-Host "Building FAT16 image via fat16.py"
Write-Host "  Boot     = $BootBin"
Write-Host "  Kernel   = $KernelBin"
Write-Host "  App ELF  = $AppElf"
Write-Host "  Help.txt = $helpPath"
Write-Host "  Output   = $OutputImg"

& python "$scriptPath" "$BootBin" "$KernelBin" "$helpPath" "$AppElf" "$OutputImg"

if ($LASTEXITCODE -ne 0) {
    throw "fat16.py failed with exit code $LASTEXITCODE"
}

Write-Host "Created FAT16 image: $OutputImg"
