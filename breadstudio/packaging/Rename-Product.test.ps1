$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Rename-Product.ps1')

function Assert-Equal($Expected, $Actual, [string] $Name) {
    if ($Expected -cne $Actual) { throw "${Name}`n  expected: $Expected`n  actual:   $Actual" }
    Write-Host "ok - $Name"
}

Assert-Equal 'A="BreadStudio is still currently active."' (Rename-ProductInText 'A="OBS is still currently active."') 'bare OBS'
Assert-Equal 'A="BreadStudio Crash Detected"' (Rename-ProductInText 'A="OBS Studio Crash Detected"') 'OBS Studio becomes one name'
Assert-Equal 'A="Select BreadStudio Recording..."' (Rename-ProductInText 'A="Select OBS Recording..."') 'OBS before another word'
Assert-Equal 'Remux.OBSRecording="BreadStudio Recording"' (Rename-ProductInText 'Remux.OBSRecording="OBS Recording"') 'the key is never changed'
Assert-Equal 'A="Import from OBS Classic"' (Rename-ProductInText 'A="Import from OBS Classic"') 'OBS Classic is kept'
Assert-Equal 'A="the OBS Project"' (Rename-ProductInText 'A="the OBS Project"') 'OBS Project is kept'
Assert-Equal 'A="report to the OBSProject."' (Rename-ProductInText 'A="report to the OBSProject."') 'OBSProject is kept'
Assert-Equal 'A="uses obs-websocket"' (Rename-ProductInText 'A="uses obs-websocket"') 'lower-case obs is kept'
Assert-Equal 'no equals sign OBS' (Rename-ProductInText 'no equals sign OBS') 'a line without = is kept'
Assert-Equal "A=`"BreadStudio`"`r`nB=`"x`"`r`n" (Rename-ProductInText "A=`"OBS`"`r`nB=`"x`"`r`n") 'CRLF line endings are kept'
Assert-Equal "A=`"BreadStudio`"`nB=`"BreadStudio`"`n" (Rename-ProductInText "A=`"OBS Studio`"`nB=`"OBS`"`n") 'every line is handled'

# The scene-collection importer lists the programs it can import from; that entry names the real OBS Studio.
Assert-Equal 'OBSStudio="OBS Studio"' (Rename-ProductInText 'OBSStudio="OBS Studio"') 'the importer program name is kept'
Assert-Equal "A=`"BreadStudio`"`nOBSStudio=`"OBS Studio`"`nB=`"BreadStudio`"" (Rename-ProductInText "A=`"OBS`"`nOBSStudio=`"OBS Studio`"`nB=`"OBS`"") 'lines around the importer name are still renamed'

# Japanese, Chinese and Korean write the name with no space around it.
$wo = [string][char]0x3092   # Japanese particle "wo"
Assert-Equal "A=`"BreadStudio${wo}`"" (Rename-ProductInText "A=`"OBS${wo}`"") 'OBS directly before a CJK character'
Assert-Equal "A=`"${wo}BreadStudio`"" (Rename-ProductInText "A=`"${wo}OBS Studio`"") 'OBS Studio directly after a CJK character'
Assert-Equal 'A="OBSBasic and MyOBS"' (Rename-ProductInText 'A="OBSBasic and MyOBS"') 'OBS inside a Latin word is kept'

# Files: a BOM, CRLF and Vietnamese text survive; an untouched file is not rewritten.
$utf8 = New-Object System.Text.UTF8Encoding $false
$dir = Join-Path ([System.IO.Path]::GetTempPath()) ('bs-rename-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $dir | Out-Null
try {
    $d = [string][char]0x0111   # Vietnamese d with stroke
    $bom = [byte[]](0xEF, 0xBB, 0xBF)

    $withBom = Join-Path $dir 'vi-VN.ini'
    [System.IO.File]::WriteAllBytes($withBom, $bom + $utf8.GetBytes("A=`"OBS ${d}ang ch`"`r`n"))
    Assert-Equal $true (Rename-ProductInFile $withBom) 'a changed file reports true'
    $expected = ($bom + $utf8.GetBytes("A=`"BreadStudio ${d}ang ch`"`r`n")) -join ','
    Assert-Equal $expected ([System.IO.File]::ReadAllBytes($withBom) -join ',') 'BOM, CRLF and non-ASCII bytes are kept'

    $clean = Join-Path $dir 'clean.ini'
    [System.IO.File]::WriteAllBytes($clean, $utf8.GetBytes("A=`"${d}`"`n"))
    $stamp = (Get-Item $clean).LastWriteTimeUtc
    Start-Sleep -Milliseconds 50
    Assert-Equal $false (Rename-ProductInFile $clean) 'an unchanged file reports false'
    Assert-Equal $stamp (Get-Item $clean).LastWriteTimeUtc 'an unchanged file is not rewritten'
} finally {
    Remove-Item -Recurse -Force $dir
}

Write-Host 'Rename-Product: all tests passed'

# Say the result explicitly: a script that ends without this exits with whatever
# $LASTEXITCODE a child command happened to leave behind.
exit 0
