# Replaces the OBS product name with BreadStudio in the values of installed translation (.ini) files.
# Keys are never changed. "OBS Classic", "OBS Project", "OBSProject" and the importer's "OBS Studio"
# entry name other things and are kept.
# Dot-source the file to get the two functions; pass -Path to rewrite files.
param([string[]] $Path)

# Boundaries are ASCII letters and digits only: \b would treat Japanese, Chinese and Korean
# characters as part of the word and skip "OBS" written directly against them.
$script:ProductPattern = [regex]'(?<![A-Za-z0-9_])OBS(?: Studio)?(?![A-Za-z0-9_])(?! (?:Classic|Project)(?![A-Za-z0-9_]))'
$script:ValuePattern = [regex]'(?m)^([^=\r\n]*=)([^\r\n]*)'
$script:Utf8NoBom = New-Object System.Text.UTF8Encoding $false

function Rename-ProductInText([string] $Text) {
    return $script:ValuePattern.Replace($Text, [System.Text.RegularExpressions.MatchEvaluator] {
            param($match)
            # The scene-collection importer's entry for the real OBS Studio keeps its name.
            if ($match.Groups[1].Value -ceq 'OBSStudio=') { return $match.Value }
            $match.Groups[1].Value + $script:ProductPattern.Replace($match.Groups[2].Value, 'BreadStudio')
        })
}

function Rename-ProductInFile([string] $File) {
    $bytes = [System.IO.File]::ReadAllBytes($File)
    $skip = 0
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { $skip = 3 }

    $old = $script:Utf8NoBom.GetString($bytes, $skip, $bytes.Length - $skip)
    $new = Rename-ProductInText $old
    if ($new -ceq $old) { return $false }

    $out = New-Object System.IO.MemoryStream
    $out.Write($bytes, 0, $skip)
    $body = $script:Utf8NoBom.GetBytes($new)
    $out.Write($body, 0, $body.Length)
    [System.IO.File]::WriteAllBytes($File, $out.ToArray())
    return $true
}

if ($Path) {
    $count = 0
    foreach ($file in $Path) {
        if (Rename-ProductInFile $file) { $count++ }
    }
    Write-Host "Renamed the product in $count of $($Path.Count) file(s)."
}
