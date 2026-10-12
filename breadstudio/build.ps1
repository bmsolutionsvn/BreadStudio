# Configure and build. Run from a Visual Studio 2026 developer shell.
param([string] $Configuration = 'RelWithDebInfo')

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/..").Path
$tag = (Get-Content (Join-Path $root 'breadstudio/UPSTREAM-TAG') -TotalCount 1).Trim()

Push-Location $root
try {
    # OBS_VERSION_OVERRIDE: without it OBS reads its version from `git describe`,
    # which a BreadStudio release tag would change. Plugins check this version.
    cmake --preset windows-x64 "-DOBS_VERSION_OVERRIDE=$tag" -DENABLE_WHATSNEW=OFF
    if ($LASTEXITCODE -ne 0) { throw 'cmake configure failed' }

    cmake --build --preset windows-x64 --config $Configuration --parallel
    if ($LASTEXITCODE -ne 0) { throw 'cmake build failed' }
} finally {
    Pop-Location
}
