# make-references.ps1 - builds vMix reference snapshots for the GT render test.
#
# Runs on the owner's PC, where vMix is installed and open. It drives vMix's
# HTTP API non-destructively: AddInput, SnapshotInput, RemoveInput. It never
# saves the owner's preset, never changes vMix's settings and never writes into
# the -Source tree; snapshots go only under -Out.
#
# vMix's output must be 1920x1080 (the render test compares at that size). Each
# .gtzip under -Source becomes one PNG under -Out with the same relative path.
#
# --- vMix's SnapshotInput PNG carries alpha: CONFIRMED 2026-10-19. ---
# A snapshot is 1920x1080 32bppArgb with a transparent corner. The file is
# labelled straight alpha, but its colour is premultiplied: 2026-10-20, pixel
# (960,1060) at alpha 227 holds 212,110,9, its source 238,123,10 times 227/255.
# That matches the render test's own premultiplied readback, so the two are
# compared like for like. (GtRenderTest.cpp composites a reference that has no
# alpha over black; it does not take that path here.)
#
# Expected on the owner's PC: 25 PNGs (design section 9).
param(
    [Parameter(Mandatory = $true)][string] $Source,
    [Parameter(Mandatory = $true)][string] $Out,
    [string] $Api = 'http://127.0.0.1:8088/api/',
    [switch] $Force
)

$ErrorActionPreference = 'Stop'

function Get-VmixXml {
    return (Invoke-WebRequest -UseBasicParsing -TimeoutSec 15 -Uri $Api).Content
}

# vMix renumbers its inputs, so a new one is picked out by its unique "key", not
# by its number. Both attributes are in vMix's /api/ XML.
function Get-InputKeys([string] $Xml) {
    $keys = @()
    foreach ($m in [regex]::Matches($Xml, '<input\b[^>]*\bkey="([^"]+)"')) {
        $keys += $m.Groups[1].Value
    }
    return $keys
}

# Everything the function needs travels in the query string, so the value -- a
# Windows path, often with spaces -- is percent-encoded. vMix decodes it.
function Add-GtInput([string] $path) {
    $before = Get-InputKeys (Get-VmixXml)
    foreach ($type in @('GT', 'Title')) {
        $value = [uri]::EscapeDataString($type + '|' + $path)
        $url = $Api + '?Function=AddInput&Value=' + $value
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 15 -Uri $url | Out-Null
        $keys = Get-InputKeys (Get-VmixXml)
        $new = @($keys | Where-Object { $before -notcontains $_ })
        if ($new.Count -ge 1) {
            Write-Host "  added as '$type' input key $($new[0])"
            return $new[0]
        }
    }
    throw "vMix did not add $path as either 'GT' or 'Title'"
}

function Remove-VmixInput([string] $key) {
    Invoke-WebRequest -UseBasicParsing -TimeoutSec 15 -Uri ($Api + '?Function=RemoveInput&Input=' + [uri]::EscapeDataString($key)) | Out-Null
}

# Fail early and clearly when vMix is not there, rather than writing nothing.
try {
    $null = Get-VmixXml
} catch {
    $message = "vMix's API did not answer at $Api. Open vMix with its API enabled, then run again."
    Write-Error ($message + " ($($_.Exception.Message))")
    exit 1
}

$Source = (Resolve-Path -LiteralPath $Source).Path
$Out = (Resolve-Path -LiteralPath (New-Item -ItemType Directory -Force -Path $Out)).Path

$files = @(Get-ChildItem -LiteralPath $Source -Recurse -File -Filter *.gtzip | Sort-Object FullName)
if ($files.Count -eq 0) {
    Write-Error "No .gtzip under $Source"
    exit 1
}

$made = 0
foreach ($file in $files) {
    $relative = $file.FullName.Substring($Source.Length).TrimStart('\', '/')
    $dest = Join-Path $Out ([System.IO.Path]::ChangeExtension($relative, '.png'))
    if ((Test-Path -LiteralPath $dest) -and -not $Force) {
        Write-Host "skip (exists): $relative"
        continue
    }
    [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($dest)) | Out-Null
    Write-Host "render: $relative"
    $input = Add-GtInput $file.FullName
    try {
        $url = $Api + '?Function=SnapshotInput&Input=' + [uri]::EscapeDataString($input) +
               '&Value=' + [uri]::EscapeDataString($dest)
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 30 -Uri $url | Out-Null
        $made++
    } finally {
        Remove-VmixInput $input
    }
}

Write-Host "Wrote $made snapshot(s) into $Out"
