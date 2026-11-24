# Assemble dist\Drench (Data layout) from the built pieces and check it.
# Run after: xmake build Drench, tools\build_papyrus.ps1, python tools\build_esp.py
#
# Run: powershell -ExecutionPolicy Bypass -File tools\stage.ps1
# Re-running rebuilds the folder from scratch and gives the same files.

$ErrorActionPreference = 'Stop'

$root  = Split-Path -Parent $PSScriptRoot
$stage = Join-Path $root 'dist\Drench'

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force "$stage\SKSE\Plugins", "$stage\SKSE\CustomConsole", "$stage\Scripts\Source" | Out-Null

Copy-Item (Join-Path $root 'plugin\Drench.esp') $stage
Copy-Item (Join-Path $root 'plugin\compat\WetFunctionNG.esp') $stage
Copy-Item (Join-Path $root 'build\windows\x64\releasedbg\Drench.dll') "$stage\SKSE\Plugins\"
Copy-Item (Join-Path $root 'dist_template\SKSE\CustomConsole\Drench.yaml') "$stage\SKSE\CustomConsole\"
# menu translation template (tools\strings_template.py); translations sit next to it as Strings_<LANGUAGE>.ini
New-Item -ItemType Directory -Force "$stage\SKSE\Plugins\Drench" | Out-Null
Copy-Item (Join-Path $root 'dist_template\SKSE\Plugins\Drench\*.ini') "$stage\SKSE\Plugins\Drench\"
Copy-Item (Join-Path $root 'assets\textures') $stage -Recurse
# the readme, under a name of its own (docs\<mod folder>__README.md), never a root README
Copy-Item (Join-Path $root 'dist_template\docs') $stage -Recurse

$scripts = @(
    @{ name = 'Drench'; out = 'papyrus\out'; src = 'papyrus\source' }
    @{ name = 'DrenchBridge'; out = 'papyrus\out'; src = 'papyrus\source' }
    @{ name = 'DrenchPlayerAlias'; out = 'papyrus\out'; src = 'papyrus\source' }
    @{ name = 'WetFunctionNG'; out = 'papyrus\out_compat'; src = 'papyrus\compat' }
)
foreach ($s in $scripts) {
    $pex = Join-Path $root "$($s.out)\$($s.name).pex"
    $psc = Join-Path $root "$($s.src)\$($s.name).psc"
    if ((Get-Item $pex).LastWriteTime -lt (Get-Item $psc).LastWriteTime) { throw "$($s.name).pex is older than its source - run tools\build_papyrus.ps1" }
    Copy-Item $pex "$stage\Scripts\"
    Copy-Item $psc "$stage\Scripts\Source\"
}

# every record of both plugins is form 44
foreach ($name in 'Drench.esp', 'WetFunctionNG.esp') {
    $esp = [IO.File]::ReadAllBytes("$stage\$name")
    $off = 0
    while ($off + 24 -le $esp.Length) {
        $type = [Text.Encoding]::ASCII.GetString($esp, $off, 4)
        $size = [BitConverter]::ToUInt32($esp, $off + 4)
        if ($type -eq 'GRUP') { $off += 24; continue }
        if ([BitConverter]::ToUInt16($esp, $off + 20) -ne 44) { throw "$name has a record that is not form 44" }
        $off += 24 + $size
    }
}

$textures = @(
    'wet_001_s', 'wet_010_s', 'wet_011_s', 'wet_100_s', 'wet_101_s', 'wet_110_s', 'wet_111_s', 'wethand_s', 'wethead_s', 'wetschlong_s',
    'male_wet_01_s', 'male_wet_10_s', 'male_wet_11_s', 'male_wethand_s', 'male_wethead_s', 'male_wetschlong_s',
    'wet_001_UBE_n', 'wet_010_UBE_n', 'wet_011_UBE_n', 'wet_100_UBE_n', 'wet_101_UBE_n', 'wet_110_UBE_n', 'wet_111_UBE_n', 'wethand_UBE_n', 'wethead_UBE_n',
    'male_wet_01_UBE_n', 'male_wet_10_UBE_n', 'male_wet_11_UBE_n', 'male_wethand_UBE_n', 'male_wethead_UBE_n', 'male_wetschlong_UBE_n',
    # head and hands per body state, matched to the body map at the neck and the wrists (tools\match_seams.py)
    'wethead_10_s', 'wethead_01_s', 'wethead_11_s', 'wethand_10_s', 'wethand_01_s', 'wethand_11_s',
    'wethead_10_UBE_n', 'wethead_01_UBE_n', 'wethead_11_UBE_n', 'wethand_10_UBE_n', 'wethand_01_UBE_n', 'wethand_11_UBE_n',
    # TNG genitals of the LDD racial meshes (tools\bake_tng.py + tools\make_tng_map.py)
    'male_wetschlong_LDD_s',
    # COTR faces: the head maps carried over to the COTR UV layout (tools\make_cotr_head.py)
    'wethead_COTR_s', 'wethead_COTR_10_s', 'wethead_COTR_01_s', 'wethead_COTR_11_s', 'male_wethead_COTR_s'
)
# 3BA drops in relief through Mu Dynamic NormalMap's detail map (tools\make_mdnm_detail.py)
$textures += 'wet_010_detail_n', 'wet_011_detail_n', 'wet_100_detail_n', 'wet_101_detail_n', 'wet_110_detail_n', 'wet_111_detail_n', 'wet_flat_detail_n'
# Goam Elven Ears - Redone, one ear map per head map and ear size (tools\bake_goam_ears.py + tools\make_ear_map.py)
foreach ($size in 'Sm', 'Lrg', 'XL') {
    foreach ($head in '_10', '_01', '_11', '') {
        $textures += "wetear_Goam$size${head}_s", "wetear_Goam$size${head}_UBE_s"
    }
    $textures += "male_wetear_Goam${size}_s", "male_wetear_Goam${size}_UBE_s"
}
$missing = $textures | Where-Object { -not (Test-Path "$stage\textures\actors\character\Drench\$_.dds") }
if ($missing) { throw "missing textures: $($missing -join ', ')" }

# the running drops (embedded DripSim): placeholder maps the engine loads into a free overlay slot, then replaced by Drench's own textures
$drips = foreach ($surface in 'body', 'hands', 'feet', 'face') { "surface_$surface", "surface_${surface}_n", "surface_${surface}_s" }
$missing = $drips | Where-Object { -not (Test-Path "$stage\textures\Drench\Drips\$_.dds") }
if ($missing) { throw "missing drop placeholders: $($missing -join ', ')" }
if (-not (Test-Path "$stage\docs\Drench__DripSim-LICENSE.txt")) { throw 'missing the DripSim license' }

$files = Get-ChildItem $stage -Recurse -File
# translations and the Drips*.ini settings
$translations = (Get-ChildItem (Join-Path $root 'dist_template\SKSE\Plugins\Drench') -Filter *.ini).Count
# esp x2, dll, console file, pex x4, psc x4, readme + DripSim license, ini, maps
$expected = 2 + 1 + 1 + 4 + 4 + 2 + $translations + $textures.Count + $drips.Count
if ($files.Count -ne $expected) { throw "expected $expected files, staged $($files.Count)" }

# nothing of the build machine may ship: user folders, drive paths, account names
# strings that must never ship (build machine names, local paths): one per line in tools\leak_patterns.txt
$patternFile = Join-Path $PSScriptRoot 'leak_patterns.txt'
$patterns = if (Test-Path $patternFile) { @(Get-Content $patternFile | Where-Object { $_ }) } else { @() }
$latin = [Text.Encoding]::GetEncoding(28591)
foreach ($file in $files | Where-Object { $_.Extension -ne '.dds' }) {
    $text = $latin.GetString([IO.File]::ReadAllBytes($file.FullName))
    foreach ($pattern in $patterns) {
        $hit = [regex]::Match($text, $pattern)
        if ($hit.Success -and $file.Extension -ne '.psc') {
            $from = [Math]::Max(0, $hit.Index - 20)
            throw "$($file.Name): build-machine text near '$($text.Substring($from, [Math]::Min(60, $text.Length - $from)))'"
        }
    }
}

$files | Where-Object { $_.Extension -ne '.dds' } | Select-Object @{ n = 'file'; e = { $_.FullName.Substring($stage.Length) } }, Length | Format-Table -AutoSize | Out-String -Width 200
'{0} textures, {1:N0} MB total, staged in {2}' -f $textures.Count, (($files | Measure-Object Length -Sum).Sum / 1MB), $stage
