# Fails when breadstudio/UPSTREAM-EDITS.md and the files changed since the pinned OBS tag disagree.
param([string] $RepoRoot = (Resolve-Path "$PSScriptRoot/..").Path)

$tag = (Get-Content (Join-Path $RepoRoot 'breadstudio/UPSTREAM-TAG') -TotalCount 1).Trim()

git -C $RepoRoot rev-parse --verify --quiet "refs/tags/$tag" | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "Tag '$tag' from breadstudio/UPSTREAM-TAG is not in this repo. Run: git fetch upstream --tags"
    exit 1
}

# Compared with the working tree, not HEAD, so staged work counts and the check can run before a commit.
# Run `git add -A` first: a new file that is not staged is invisible to git diff.
# --no-renames: a moved OBS file must show up as one deletion and one addition.
$changed = @(git -C $RepoRoot diff --name-only --no-renames $tag -- . ':(exclude)breadstudio')
if ($LASTEXITCODE -ne 0) {
    Write-Host 'git diff failed'
    exit 1
}

$listed = @(Get-Content (Join-Path $RepoRoot 'breadstudio/UPSTREAM-EDITS.md') |
        ForEach-Object { if ($_ -match '^- `([^`]+)`') { $Matches[1] } })

$unlisted = @($changed | Where-Object { $listed -notcontains $_ })
$stale = @($listed | Where-Object { $changed -notcontains $_ })

if ($unlisted.Count -gt 0) {
    Write-Host 'Changed since the pinned tag but not in breadstudio/UPSTREAM-EDITS.md:'
    $unlisted | ForEach-Object { Write-Host "  $_" }
}
if ($stale.Count -gt 0) {
    Write-Host 'In breadstudio/UPSTREAM-EDITS.md but not changed since the pinned tag:'
    $stale | ForEach-Object { Write-Host "  $_" }
}
if ($unlisted.Count -gt 0 -or $stale.Count -gt 0) { exit 1 }

Write-Host "UPSTREAM-EDITS.md matches the $($changed.Count) file(s) changed since $tag."
exit 0
