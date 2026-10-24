[CmdletBinding()]
param(
    [ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version = '1.0.3',
    [string]$BuildDirectory = '',
    [string]$DestinationDirectory = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (!$BuildDirectory) { $BuildDirectory = Join-Path $projectRoot 'bin/x64/Release' }
if (!$DestinationDirectory) { $DestinationDirectory = Join-Path $projectRoot 'dist' }
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$packageName = "Layrix-$Version-windows-x64"
$stage = Join-Path $DestinationDirectory $packageName
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage -Force | Out-Null
# Only release payloads: never copy personal settings, debug symbols, old EXEs, or shortcuts.
Get-ChildItem -LiteralPath $BuildDirectory -File | Where-Object {
    $_.Name -eq 'Layrix.exe' -or $_.Extension -in '.dll', '.pri'
} | Copy-Item -Destination $stage
Get-ChildItem -LiteralPath $BuildDirectory -Directory | ForEach-Object {
    if ($_.Name -match '^[a-z]{2,3}(-[A-Za-z0-9]+)+$') { Copy-Item -LiteralPath $_.FullName -Destination $stage -Recurse }
}
foreach ($required in 'Layrix.exe', 'LayrixEditor.dll', 'resources.pri', 'Microsoft.ui.xaml.dll') {
    if (!(Test-Path -LiteralPath (Join-Path $stage $required))) { throw "Missing runtime payload: $required" }
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
$crt = Get-ChildItem -LiteralPath (Join-Path $vs 'VC/Redist/MSVC') -Directory | Where-Object Name -match '^\d' | Sort-Object Name -Descending | ForEach-Object {
    Get-ChildItem -LiteralPath (Join-Path $_.FullName 'x64') -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like 'Microsoft.VC*.CRT' }
} | Select-Object -ExpandProperty FullName -First 1
if (!$crt) { throw 'The app-local Visual C++ x64 runtime was not found.' }
Get-ChildItem -LiteralPath $crt -Filter '*.dll' | Copy-Item -Destination $stage
Copy-Item -LiteralPath (Join-Path $projectRoot 'licenses') -Destination $stage -Recurse
foreach ($document in 'LICENSE', 'README.md', 'README.ru.md', 'THIRD_PARTY_NOTICES.md') {
    Copy-Item -LiteralPath (Join-Path $projectRoot $document) -Destination $stage
}
$assets = Get-Content -LiteralPath (Join-Path $projectRoot 'obj/LayrixEditor/project.assets.json') -Raw | ConvertFrom-Json
$sdkRoot = $assets.packageFolders.PSObject.Properties.Name | ForEach-Object { Join-Path $_ 'microsoft.windowsappsdk/1.6.250602001' } | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (!$sdkRoot) { throw 'Windows App SDK package licenses were not found. Restore and build first.' }
Copy-Item -LiteralPath (Join-Path $sdkRoot 'license.txt') -Destination (Join-Path $stage 'licenses/WindowsAppSDK-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $sdkRoot 'NOTICE.txt') -Destination (Join-Path $stage 'licenses/WindowsAppSDK-NOTICE.txt')
$badFiles = Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object { $_.Name -like 'layouts.json*' -or $_.Extension -in '.pdb', '.lnk', '.lib', '.exp' }
if ($badFiles) { throw 'The archive unexpectedly contains local or build-only files.' }
$archive = Join-Path $DestinationDirectory "$packageName.zip"
if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -CompressionLevel Optimal
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $packageName.zip" | Set-Content -LiteralPath (Join-Path $DestinationDirectory 'SHA256SUMS.txt') -Encoding ascii
Write-Host "Release archive: $archive"
Write-Host "SHA256: $hash"
