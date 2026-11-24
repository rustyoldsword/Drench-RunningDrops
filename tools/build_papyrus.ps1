# Compile papyrus\source\*.psc into papyrus\out and papyrus\compat\*.psc into papyrus\out_compat, then strip the
# build identity (source path, user, machine) from every .pex header.
#
# papyrus\stubs stands in for SexLab P+ (its sources do not parse with the Bethesda compiler); stubs are never
# shipped. A failed compile leaves the previous .pex in place, so every output is checked to be newer than
# its source.
#
# Run: powershell -ExecutionPolicy Bypass -File tools\build_papyrus.ps1 [-Game <folder with Papyrus Compiler>]

param([string]$Game = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\basegame')).Path)

$ErrorActionPreference = 'Stop'

$root     = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path $Game 'Papyrus Compiler\PapyrusCompiler.exe'

$failed = @()
$built  = @()
foreach ($set in @(@{ src = 'papyrus\source'; out = 'papyrus\out' }, @{ src = 'papyrus\compat'; out = 'papyrus\out_compat' })) {
    $src = Join-Path $root $set.src
    $out = Join-Path $root $set.out
    New-Item -ItemType Directory -Force $out | Out-Null
    $imports = @(
        $src
        (Join-Path $root 'papyrus\source')
        (Join-Path $root 'papyrus\stubs')
        (Join-Path $Game 'Data\Scripts\Source')
        (Join-Path $Game 'Data\Source\Scripts')
    ) -join ';'
    Push-Location $env:TEMP
    try {
        foreach ($file in Get-ChildItem $src -Filter *.psc) {
            $output = & $compiler $file.FullName -f='TESV_Papyrus_Flags.flg' -i="$imports" -o="$out" 2>&1
            $pex = Join-Path $out ($file.BaseName + '.pex')
            if (-not (Test-Path $pex) -or (Get-Item $pex).LastWriteTime -lt $file.LastWriteTime) {
                $failed += $file.Name
                $output | Where-Object { $_ -match '\(\d+,\d+\)|error' } | ForEach-Object { Write-Host $_ }
            } else {
                Write-Host "ok  $($file.Name)"
                $built += $pex
            }
        }
    } finally {
        Pop-Location
    }
}

if ($failed.Count) {
    Write-Host "FAILED: $($failed -join ', ')"
    exit 1
}
& python (Join-Path $PSScriptRoot 'pex_scrub.py') @built
