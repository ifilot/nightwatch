# SPDX-License-Identifier: GPL-3.0-only
# Run generate.py first. This script must run on Windows, not a substitute writer.
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$inputDir = Join-Path $repo 'build/zip-fixture-input'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
foreach ($pair in @(@('WINFAST.ZIP','Fastest'), @('WINBEST.ZIP','Optimal'), @('WINSTORE.ZIP','NoCompression'))) {
    $path = Join-Path $PSScriptRoot $pair[0]
    if (Test-Path $path) { Remove-Item $path }
    $zip = [IO.Compression.ZipFile]::Open($path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in (Get-ChildItem $inputDir -Recurse -File | Sort-Object FullName)) {
            $name = $file.FullName.Substring($inputDir.Length + 1).Replace('\','/')
            $entry = $zip.CreateEntry($name, [IO.Compression.CompressionLevel]::$($pair[1]))
            $entry.LastWriteTime = [DateTimeOffset]::new(2026, 10, 8, 12, 34, 56, [TimeSpan]::Zero)
            $source = [IO.File]::OpenRead($file.FullName)
            $out = $entry.Open()
            try { $source.CopyTo($out) } finally { $out.Dispose(); $source.Dispose() }
        }
        $zip.CreateEntry('SUB/EMPTY/') | Out-Null
    } finally { $zip.Dispose() }
}
$archive = Join-Path $PSScriptRoot 'WINPS.ZIP'
if (Test-Path $archive) { Remove-Item $archive }
Compress-Archive -Path (Join-Path $inputDir '*') -DestinationPath $archive -CompressionLevel Optimal
@("Windows: $([Environment]::OSVersion.VersionString)", "PowerShell: $($PSVersionTable.PSVersion)", "CLR: $([Environment]::Version)", '.NET ZipArchive: Fastest, Optimal, NoCompression', 'PowerShell Compress-Archive: Optimal') | Set-Content (Join-Path $PSScriptRoot 'windows-provenance.txt')
