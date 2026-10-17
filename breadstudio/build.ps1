# Configure and build. Run from a developer shell for the toolchain you have.
#
# -Preset: windows-x64 names the generator OBS pins, which is what CI's runner
# carries. A machine with Visual Studio 2022 and not 2026 uses
# windows-x64-vs2022 instead.
param(
    [string] $Configuration = 'RelWithDebInfo',
    [string] $Preset = 'windows-x64'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/..").Path
$tag = (Get-Content (Join-Path $root 'breadstudio/UPSTREAM-TAG') -TotalCount 1).Trim()

Push-Location $root
try {
    # OBS_VERSION_OVERRIDE: without it OBS reads its version from `git describe`,
    # which a BreadStudio release tag would change. Plugins check this version.
    cmake --preset $Preset "-DOBS_VERSION_OVERRIDE=$tag" -DENABLE_WHATSNEW=OFF
    if ($LASTEXITCODE -ne 0) { throw 'cmake configure failed' }

    cmake --build --preset $Preset --config $Configuration --parallel
    if ($LASTEXITCODE -ne 0) { throw 'cmake build failed' }
} finally {
    Pop-Location
}
