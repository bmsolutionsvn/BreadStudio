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
# --- Whether vMix's SnapshotInput PNG carries alpha: NOT yet confirmed. ---
# When this script was written, vMix's API did not answer on this machine, so
# no snapshot was taken and the alpha question is open. GtRenderTest.cpp
# composites a reference that has no alpha over black, so an opaque snapshot is
# handled; if vMix writes alpha, read GtRenderTest.cpp's composite step before
# trusting a diff.
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

# vMix numbers its inputs; the numbers present now, so a new one can be picked
# out after AddInput. The attribute is "number" in vMix's /api/ XML.
function Get-InputNumbers([string] $Xml) {
    $numbers = @()
    foreach ($m in [regex]::Matches($Xml, '<input\b[^>]*\bnumber="(\d+)"')) {
        $numbers += [int] $m.Groups[1].Value
    }
    return $numbers
}

function Add-GtInput([string] $path) {
    $before = Get-InputNumbers (Get-VmixXml)
    foreach ($type in @('GT', 'Title')) {
        $url = $Api + '?Function=AddInput&Value=' + $type + '|' + $path
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 15 -Uri $url | Out-Null
        $after = Get-InputNumbers (Get-VmixXml)
        $new = @($after | Where-Object { $before -notcontains $_ })
        if ($new.Count -ge 1) {
            Write-Host "  added as '$type' input $($new[0])"
            return $new[0]
        }
    }
    throw "vMix did not add $path as either 'GT' or 'Title'"
}

function Remove-VmixInput([int] $number) {
    Invoke-WebRequest -UseBasicParsing -TimeoutSec 15 -Uri ($Api + '?Function=RemoveInput&Input=' + $number) | Out-Null
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
    $number = Add-GtInput $file.FullName
    try {
        $url = $Api + '?Function=SnapshotInput&Input=' + $number + '&Value=' + $dest
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 30 -Uri $url | Out-Null
        $made++
    } finally {
        Remove-VmixInput $number
    }
}

Write-Host "Wrote $made snapshot(s) into $Out"
