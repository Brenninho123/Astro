# Generates assets/astro.ico (used by the Windows simulator executable) from the
# 16x16 pixel grid in src/apps/launcher_icon.c, so there is a single source of truth.
#
# Usage (from the repository root):  pwsh tools/make-icon.ps1

$ErrorActionPreference = 'Stop'

$root   = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'src/apps/launcher_icon.c'
$output = Join-Path $root 'assets/astro.ico'

# VGA palette colors (RGB) for each character of the grid; '.' is transparent.
# (case-sensitive: 'M' and 'm' are different colors)
$palette = New-Object System.Collections.Hashtable([StringComparer]::Ordinal)
$palette['M'] = @(0xFF, 0x55, 0xFF)   # light magenta
$palette['m'] = @(0xAA, 0x00, 0xAA)   # magenta
$palette['1'] = @(0x00, 0x00, 0xAA)   # blue
$palette['y'] = @(0xFF, 0xFF, 0x55)   # yellow
$palette['w'] = @(0xFF, 0xFF, 0xFF)   # white
$palette['c'] = @(0x55, 0xFF, 0xFF)   # light cyan

$rows = Get-Content $source | ForEach-Object {
    if ($_ -cmatch '^\s+"([.Mmywc1]{16})",\s*$') { $Matches[1] }
}
if ($rows.Count -ne 16) { throw "Expected 16 icon rows in $source, found $($rows.Count)" }

# One BMP-in-ICO image (32-bit BGRA, bottom-up) scaled by an integer factor.
function New-IconImage([int]$scale) {
    $size = 16 * $scale
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)

    # BITMAPINFOHEADER (height is doubled: XOR bitmap + AND mask)
    $bw.Write([uint32]40); $bw.Write([int32]$size); $bw.Write([int32]($size * 2))
    $bw.Write([uint16]1);  $bw.Write([uint16]32);   $bw.Write([uint32]0)
    $bw.Write([uint32]0);  $bw.Write([int32]0);     $bw.Write([int32]0)
    $bw.Write([uint32]0);  $bw.Write([uint32]0)

    for ($y = $size - 1; $y -ge 0; $y--) {
        $line = $rows[[int][math]::Floor($y / $scale)]
        for ($x = 0; $x -lt $size; $x++) {
            $ch = $line[[int][math]::Floor($x / $scale)]
            if ($palette.ContainsKey([string]$ch)) {
                $rgb = $palette[[string]$ch]
                $bw.Write([byte]$rgb[2]); $bw.Write([byte]$rgb[1]); $bw.Write([byte]$rgb[0]); $bw.Write([byte]255)
            } else {
                $bw.Write([uint32]0)
            }
        }
    }

    # AND mask: all zero (transparency comes from the alpha channel)
    $maskBytes = [int][math]::Ceiling($size / 32) * 4 * $size
    $bw.Write((New-Object byte[] $maskBytes))

    $bw.Flush()
    return ,$ms.ToArray()
}

$scales = 1, 2, 3, 4          # 16, 32, 48 and 64 pixels
$images = $scales | ForEach-Object { , (New-IconImage $_) }

$out = New-Object System.IO.MemoryStream
$w = New-Object System.IO.BinaryWriter($out)
$w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$images.Count)   # ICONDIR

$offset = 6 + 16 * $images.Count
for ($i = 0; $i -lt $images.Count; $i++) {
    $px = 16 * $scales[$i]
    $w.Write([byte]$px); $w.Write([byte]$px); $w.Write([byte]0); $w.Write([byte]0)
    $w.Write([uint16]1); $w.Write([uint16]32)
    $w.Write([uint32]$images[$i].Length); $w.Write([uint32]$offset)
    $offset += $images[$i].Length
}
foreach ($img in $images) { $w.Write($img) }
$w.Flush()

New-Item -ItemType Directory -Force (Split-Path $output) | Out-Null
[System.IO.File]::WriteAllBytes($output, $out.ToArray())
Write-Host "Wrote $output ($($out.Length) bytes)"
