$ErrorActionPreference = 'Stop'

$script = Join-Path $PSScriptRoot 'check-edits.ps1'
$repo = Join-Path ([System.IO.Path]::GetTempPath()) ('bs-check-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $repo | Out-Null

function G {
    git -C $repo @args | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "git $args failed" }
}

function Expect([int] $Code, [string] $Name) {
    powershell -NoProfile -ExecutionPolicy Bypass -File $script -RepoRoot $repo | Out-Null
    if ($LASTEXITCODE -ne $Code) { throw "${Name}: expected exit $Code, got $LASTEXITCODE" }
    Write-Host "ok - $Name"
}

$list = Join-Path $repo 'breadstudio/UPSTREAM-EDITS.md'

try {
    G init -q
    G config user.email test@example.invalid
    G config user.name test
    Set-Content (Join-Path $repo 'a.c') 'a'
    Set-Content (Join-Path $repo 'b.c') 'b'
    G add -A
    G commit -qm base
    G tag 1.0.0

    New-Item -ItemType Directory (Join-Path $repo 'breadstudio') | Out-Null
    Set-Content (Join-Path $repo 'breadstudio/UPSTREAM-TAG') '1.0.0'
    Set-Content $list '# Edits'
    G add -A
    G commit -qm ours
    Expect 0 'only breadstudio/ changed'

    Set-Content (Join-Path $repo 'a.c') 'a2'
    G commit -qam edit
    Expect 1 'edited upstream file is not listed'

    Add-Content $list '- `a.c` - test edit'
    G commit -qam list
    Expect 0 'edited upstream file is listed'

    Add-Content $list '- `b.c` - never edited'
    G commit -qam stale
    Expect 1 'listed file is not changed'

    G rm -q b.c
    G commit -qm delete
    Expect 0 'deleted upstream file is listed'

    Set-Content (Join-Path $repo 'c.c') 'c'
    G add -A
    G commit -qm add
    Expect 1 'file added outside breadstudio/ is not listed'

    Add-Content $list '- `c.c` - our new file in an upstream folder'
    G commit -qam list-add
    Expect 0 'added file is listed'

    # The check runs before a commit, so staged work must count.
    Set-Content (Join-Path $repo 'd.c') 'd'
    G add -A
    Expect 1 'staged, uncommitted file is not listed'
    G reset -q --hard

    # An entry may name a whole tree, so a vendored dependency is one line.
    New-Item -ItemType Directory -Force (Join-Path $repo 'e/f') | Out-Null
    Set-Content (Join-Path $repo 'e/f/g.c') 'g'
    G add -A
    G commit -qm tree
    Expect 1 'a file under an unlisted tree is not listed'

    Add-Content $list '- `e/f` - a vendored tree'
    G commit -qam list-tree
    Expect 0 'a file under a listed tree is listed'

    Set-Content (Join-Path $repo 'breadstudio/UPSTREAM-TAG') '9.9.9'
    G commit -qam badtag
    Expect 1 'pinned tag does not exist'
} finally {
    Remove-Item -Recurse -Force $repo
}

Write-Host 'check-edits: all tests passed'

# Every assertion above ran the check in a child process, so $LASTEXITCODE still
# holds the last one's code - and the last assertion expects a failure, so pwsh
# would leave this script exiting 1 with every test green. Say it explicitly.
exit 0
