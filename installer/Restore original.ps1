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
    if (-not (Test-Path -LiteralPath $backup -PathType Leaf)) {
        throw "The backup created by the Lion King Windows Fix does not exist: $backup"
    }
    $backupHash = Get-Sha256 $backup
    if ($backupHash -ne $originalSha256) {
        throw 'The backup does not match the supported original and will not be used.'
    }
    if (Test-Path -LiteralPath $target -PathType Leaf) {
        $targetHash = Get-Sha256 $target
        if ($targetHash -ne $patchedSha256 -and $targetHash -ne $originalSha256) {
            throw 'LIONW.EXE has changed since installation and will not be overwritten.'
        }
    }
    [IO.File]::Copy($backup, $target, $true)
    if ((Get-Sha256 $target) -ne $originalSha256) {
        throw 'The restored executable could not be verified.'
    }
    Write-Host 'The original LIONW.EXE was restored successfully.' -ForegroundColor Green
    Write-Host 'Compatibility files were preserved to avoid deleting data without permission.'
    exit 0
} catch {
    Write-Error $_.Exception.Message
    exit 1
}
