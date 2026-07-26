[CmdletBinding()]
param(
    [Parameter()]
    [string]$GameDirectory = $PSScriptRoot
)

$ErrorActionPreference = 'Stop'
$originalSha256 = '3E99DC48A4B347833E3857A13E0635AB9C5A262BBCD2DBB5304A7BBE0E45DEF3'
$patchedSha256 = 'AD28370116F86CEAAB87D77B66533018716CACB6E3E010ED63E3A053BA327EC8'

try {
    $gameRoot = [IO.Path]::GetFullPath($GameDirectory)
    $target = Join-Path $gameRoot 'LIONW.EXE'
    $backup = Join-Path $gameRoot 'LIONW.EXE.gamevault-original'
    if (-not (Test-Path -LiteralPath $backup -PathType Leaf)) {
        throw "The backup created by the Lion King Windows Fix does not exist: $backup"
    }
    $backupHash = (Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($backupHash -ne $originalSha256) {
        throw 'The backup does not match the supported original and will not be used.'
    }
    if (Test-Path -LiteralPath $target -PathType Leaf) {
        $targetHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($targetHash -ne $patchedSha256 -and $targetHash -ne $originalSha256) {
            throw 'LIONW.EXE has changed since installation and will not be overwritten.'
        }
    }
    [IO.File]::Copy($backup, $target, $true)
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToUpperInvariant() -ne $originalSha256) {
        throw 'The restored executable could not be verified.'
    }
    Write-Host 'The original LIONW.EXE was restored successfully.' -ForegroundColor Green
    Write-Host 'Compatibility files were preserved to avoid deleting data without permission.'
    exit 0
} catch {
    Write-Error $_.Exception.Message
    exit 1
}
