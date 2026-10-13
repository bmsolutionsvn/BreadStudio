# Writes a 10 s, 1 kHz, -12 dBFS, 48 kHz stereo 16-bit WAV for the audio bed check.
# 10 s is a whole number of cycles, so the file loops without a click.
param([string] $Path = "$env:TEMP\bread-tone.wav")

$ErrorActionPreference = 'Stop'
$rate = 48000
$frames = $rate * 10
$amplitude = [math]::Pow(10, -12 / 20) * 32767
$dataBytes = $frames * 4
$ascii = [Text.Encoding]::ASCII

$stream = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter($stream)
$writer.Write($ascii.GetBytes('RIFF'))
$writer.Write([int32](36 + $dataBytes))
$writer.Write($ascii.GetBytes('WAVEfmt '))
$writer.Write([int32]16)           # fmt chunk size
$writer.Write([int16]1)            # PCM
$writer.Write([int16]2)            # channels
$writer.Write([int32]$rate)
$writer.Write([int32]($rate * 4))  # bytes per second
$writer.Write([int16]4)            # bytes per frame
$writer.Write([int16]16)           # bits per sample
$writer.Write($ascii.GetBytes('data'))
$writer.Write([int32]$dataBytes)
for ($i = 0; $i -lt $frames; $i++) {
    $sample = [int16][math]::Round($amplitude * [math]::Sin(2 * [math]::PI * 1000 * $i / $rate))
    $writer.Write($sample)
    $writer.Write($sample)
}
$writer.Flush()
[IO.File]::WriteAllBytes($Path, $stream.ToArray())
Write-Output $Path
