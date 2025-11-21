param(
    [Parameter(Mandatory = $true)][string]$BootBin,
    [Parameter(Mandatory = $true)][string]$KernelBin,
    [Parameter(Mandatory = $true)][string]$OutputImg
)

function Assert-PathExists {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        throw "File not found: $Path"
    }
}

Assert-PathExists -Path $BootBin
Assert-PathExists -Path $KernelBin

$diskSize = 1474560      # 1.44MB floppy
$sectorSize = 512

# Create/overwrite the image with zeros
$bytes = New-Object byte[] ($diskSize)
[System.IO.File]::WriteAllBytes($OutputImg, $bytes)

$fs = [System.IO.File]::Open($OutputImg, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite)

# Write boot sector
$boot = [System.IO.File]::ReadAllBytes($BootBin)
if ($boot.Length -ne $sectorSize) {
    $fs.Dispose()
    throw "Boot sector must be exactly 512 bytes (got $($boot.Length))"
}
$fs.Write($boot, 0, $boot.Length)

# Write kernel starting at sector 2 (offset 512)
$kernel = [System.IO.File]::ReadAllBytes($KernelBin)
$fs.Position = $sectorSize
$fs.Write($kernel, 0, $kernel.Length)

$fs.Dispose()
Write-Host "Created image: $OutputImg (kernel size: $($kernel.Length) bytes)"
