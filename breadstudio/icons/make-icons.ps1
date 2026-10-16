# Draws the placeholder BreadStudio mark and writes the .ico and the three PNGs OBS's interface uses.
#
# PLACEHOLDER: this mark (a dark rounded square with "BM" in cyan) is not the BreadStudio
# logo. It reproduces the placeholder in bm-landing/public/favicon.svg so S0 can ship
# something that is not OBS's logo. The real mark is the owner's to supply; when it exists,
# change only this drawing and re-run.
param([string] $RepoRoot = (Resolve-Path "$PSScriptRoot/../..").Path)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function New-MarkBitmap([int] $Size, [string] $Badge) {
    $bmp = New-Object System.Drawing.Bitmap $Size, $Size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.TextRenderingHint = 'AntiAliasGridFit'
    $g.Clear([System.Drawing.Color]::Transparent)

    # Same proportions as the favicon: corner radius 6 and font size 15 on a 32 unit square.
    $d = [single]($Size * 12 / 32)
    $plate = New-Object System.Drawing.Drawing2D.GraphicsPath
    $plate.AddArc(0, 0, $d, $d, 180, 90)
    $plate.AddArc($Size - $d, 0, $d, $d, 270, 90)
    $plate.AddArc($Size - $d, $Size - $d, $d, $d, 0, 90)
    $plate.AddArc(0, $Size - $d, $d, $d, 90, 90)
    $plate.CloseFigure()
    $g.FillPath((New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml('#08090B'))), $plate)

    $style = [System.Drawing.FontStyle]::Bold
    $unit = [System.Drawing.GraphicsUnit]::Pixel
    $font = New-Object System.Drawing.Font 'Consolas', ([single]($Size * 15 / 32)), $style, $unit
    $format = New-Object System.Drawing.StringFormat
    $format.Alignment = 'Center'
    $format.LineAlignment = 'Center'
    $cyan = New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml('#22D3EE'))
    $g.DrawString('BM', $font, $cyan, (New-Object System.Drawing.RectangleF 0, 0, $Size, $Size), $format)

    if ($Badge) {
        # State dot, bottom right: amber when paused, red when live.
        $dot = [single]($Size * 0.34)
        $dotBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml($Badge))
        $x = $Size - $dot - 2
        $g.FillEllipse($dotBrush, $x, $x, $dot, $dot)
    }

    $g.Dispose()
    return $bmp
}

function Save-Png([string] $Path, [string] $Badge) {
    $bmp = New-MarkBitmap 256 $Badge
    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Save-Ico([string] $Path, [int[]] $Sizes) {
    $pngs = foreach ($size in $Sizes) {
        $bmp = New-MarkBitmap $size $null
        $stream = New-Object System.IO.MemoryStream
        $bmp.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        , $stream.ToArray()
    }

    $out = New-Object System.IO.MemoryStream
    $w = New-Object System.IO.BinaryWriter $out
    $w.Write([uint16]0)
    $w.Write([uint16]1)
    $w.Write([uint16]$Sizes.Count)

    $offset = 6 + 16 * $Sizes.Count
    for ($i = 0; $i -lt $Sizes.Count; $i++) {
        $dim = $Sizes[$i]
        if ($dim -ge 256) { $dim = 0 }   # 0 means 256 in the ICO directory
        $w.Write([byte]$dim)
        $w.Write([byte]$dim)
        $w.Write([byte]0)
        $w.Write([byte]0)
        $w.Write([uint16]1)
        $w.Write([uint16]32)
        $w.Write([uint32]$pngs[$i].Length)
        $w.Write([uint32]$offset)
        $offset += $pngs[$i].Length
    }
    foreach ($png in $pngs) { $w.Write([byte[]]$png) }
    $w.Flush()

    [System.IO.File]::WriteAllBytes($Path, $out.ToArray())
}

$images = Join-Path $RepoRoot 'frontend/forms/images'
Save-Png (Join-Path $images 'obs.png') $null
Save-Png (Join-Path $images 'obs_paused.png') '#F59E0B'
Save-Png (Join-Path $images 'tray_active.png') '#EF4444'

$ico = Join-Path $RepoRoot 'breadstudio/icons/breadstudio.ico'
Save-Ico $ico @(256, 64, 48, 32, 16)

# Self-check: the files are what the build expects.
foreach ($name in 'obs.png', 'obs_paused.png', 'tray_active.png') {
    $image = [System.Drawing.Image]::FromFile((Join-Path $images $name))
    $ok = ($image.Width -eq 256 -and $image.Height -eq 256)
    $image.Dispose()
    if (-not $ok) { throw "$name is not 256x256" }
}
$header = [System.IO.File]::ReadAllBytes($ico)[0..5]
if (($header -join ',') -ne '0,0,1,0,5,0') { throw "breadstudio.ico has a bad header: $($header -join ',')" }

Write-Host 'Wrote breadstudio.ico and 3 PNGs.'
