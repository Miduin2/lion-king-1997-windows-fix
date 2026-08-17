[CmdletBinding()]
param(
    [Parameter()]
    [string]$GameDirectory
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($GameDirectory)) {
    $GameDirectory = $PSScriptRoot
}
$originalSha256 = '3E99DC48A4B347833E3857A13E0635AB9C5A262BBCD2DBB5304A7BBE0E45DEF3'
$patchedSha256 = 'AD28370116F86CEAAB87D77B66533018716CACB6E3E010ED63E3A053BA327EC8'
$componentHashes = @{
    'ddraw.dll' = '519DBB0E20963BAC0A3C7BEBA874421D3B1FAF4CD70BB3AEADDEECA30AEC2C4F'
    'GameVaultDraw.ini' = '85CB3121713AD1AC81C390DDCACE335AF02B60939C30A60E3C653211F1307F62'
    'Play Lion King.exe' = '124A5168F95F00472B67D40FCE370E0D34E8CD20F8A655BC19F974C3DD18348F'
}
$edits = @(
    @{
        Offset = 0x2EA7
        Before = [byte[]](0x7D)
        After = [byte[]](0xEB)
        Purpose = 'colour-depth check'
    },
    @{
        Offset = 0x5A3F
        Before = [byte[]](0x7F)
        After = [byte[]](0x7D)
        Purpose = 'EPFS descriptor zero'
    },
    @{
        Offset = 0xC165
        Before = [byte[]](
            0xB4, 0x02, 0xCD, 0x1A, 0x66, 0x33, 0xD0, 0xC1,
            0xE2, 0x10, 0x66, 0x8B, 0xD1, 0x66, 0x33, 0xD3,
            0x89, 0x15, 0xD8, 0xEB, 0x44, 0x00, 0xC3
        )
        After = [byte[]](
            0xFF, 0x15, 0x74, 0x41, 0x45, 0x00, 0xA3, 0xD8,
            0xEB, 0x44, 0x00, 0xC3, 0x90, 0x90, 0x90, 0x90,
            0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
        )
        Purpose = 'obsolete BIOS clock call'
    },
    @{
        Offset = 0x3771
        Before = [byte[]](0xFA)
        After = [byte[]](0x90)
        Purpose = 'privileged CLI instruction'
    },
    @{
        Offset = 0x377F
        Before = [byte[]](0xFB)
        After = [byte[]](0x90)
        Purpose = 'privileged STI instruction'
    },
    @{
        Offset = 0x3D5B
        Before = [byte[]](0xFA)
        After = [byte[]](0x90)
        Purpose = 'privileged CLI instruction'
    },
    @{
        Offset = 0x3D75
        Before = [byte[]](0xFB)
        After = [byte[]](0x90)
        Purpose = 'privileged STI instruction'
    }
)

function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $sha256 = [Security.Cryptography.SHA256]::Create()
        try {
            ([BitConverter]::ToString($sha256.ComputeHash($stream))).Replace('-', '')
        } finally {
            $sha256.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

try {
    $gameRoot = [IO.Path]::GetFullPath($GameDirectory)
    $target = Join-Path $gameRoot 'LIONW.EXE'
    $backup = Join-Path $gameRoot 'LIONW.EXE.gamevault-original'
    $temporary = Join-Path $gameRoot 'LIONW.EXE.gamevault-new'

    if (-not (Test-Path -LiteralPath $target -PathType Leaf)) {
        throw "LIONW.EXE was not found in: $gameRoot"
    }
    foreach ($component in $componentHashes.GetEnumerator()) {
        $componentPath = Join-Path $gameRoot $component.Key
        if (-not (Test-Path -LiteralPath $componentPath -PathType Leaf)) {
            throw "A patch component is missing: $($component.Key)"
        }
        $actual = Get-Sha256 $componentPath
        if ($actual -ne $component.Value) {
            throw "Component $($component.Key) does not match the official version 1.0.0.`nExpected: $($component.Value)`nActual: $actual"
        }
    }

    $targetHash = Get-Sha256 $target
    if ($targetHash -eq $patchedSha256) {
        Write-Host 'The Lion King Windows Fix 1.0.0 is already installed.' -ForegroundColor Green
        Write-Host 'To play, open: Play Lion King.exe'
        exit 0
    }
    if ($targetHash -ne $originalSha256) {
        throw "This LIONW.EXE edition is not supported and will not be modified.`nExpected: $originalSha256`nActual: $targetHash"
    }

    if (Test-Path -LiteralPath $backup -PathType Leaf) {
        $backupHash = Get-Sha256 $backup
        if ($backupHash -ne $originalSha256) {
            throw "An unknown backup already exists: $backup"
        }
    } else {
        [IO.File]::Copy($target, $backup, $false)
    }

    $bytes = [IO.File]::ReadAllBytes($target)
    foreach ($edit in $edits) {
        if ($edit.Before.Length -ne $edit.After.Length) {
            throw "Internal patch definition error: $($edit.Purpose)"
        }
        for ($index = 0; $index -lt $edit.Before.Length; $index++) {
            $offset = $edit.Offset + $index
            if ($offset -ge $bytes.Length) {
                throw ('Patch offset 0x{0:X} is outside LIONW.EXE ({1}).' -f $offset, $edit.Purpose)
            }
            if ($bytes[$offset] -ne $edit.Before[$index]) {
                throw ('Unexpected byte at 0x{0:X}: expected 0x{1:X2}, found 0x{2:X2} ({3}).' -f $offset, $edit.Before[$index], $bytes[$offset], $edit.Purpose)
            }
        }
    }
    foreach ($edit in $edits) {
        [Array]::Copy($edit.After, 0, $bytes, $edit.Offset, $edit.After.Length)
    }

    [IO.File]::WriteAllBytes($temporary, $bytes)
    $resultHash = Get-Sha256 $temporary
    if ($resultHash -ne $patchedSha256) {
        Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
        throw "Final verification failed. The original LIONW.EXE remains unchanged.`nExpected: $patchedSha256`nActual: $resultHash"
    }

    Move-Item -LiteralPath $temporary -Destination $target -Force
    Write-Host 'Patch installed and verified successfully.' -ForegroundColor Green
    Write-Host "Original preserved at: $backup"
    Write-Host 'To play, open: Play Lion King.exe'
    exit 0
} catch {
    if ($temporary -and (Test-Path -LiteralPath $temporary)) {
        Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
    }
    Write-Error $_.Exception.Message
    exit 1
}
