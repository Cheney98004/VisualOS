param(
    [Parameter(Mandatory = $true)][string]$BootBin,
    [Parameter(Mandatory = $true)][string]$KernelBin,
    [Parameter(Mandatory = $true)][string]$OutputImg
)

if (-not (Test-Path $BootBin)) { throw "Boot bin not found: $BootBin" }
if (-not (Test-Path $KernelBin)) { throw "Kernel bin not found: $KernelBin" }

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$scriptPath = Join-Path $repoRoot "fat16.py"
if (-not (Test-Path $scriptPath)) {
    throw "fat16.py not found at $scriptPath"
}

Write-Host "Building FAT16 image via fat16.py"
& python "$scriptPath" "$BootBin" "$KernelBin" "$OutputImg"
if ($LASTEXITCODE -ne 0) {
    throw "fat16.py failed with exit code $LASTEXITCODE"
}

Write-Host "Created FAT16 image: $OutputImg"
