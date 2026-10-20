[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$PackagesDirectory = '',
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2022 or Build Tools with Desktop development with C++.' }
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild/Current/Bin/MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild with the C++ toolchain was not found.' }
$common = @('/nologo', '/v:quiet', "/p:Configuration=$Configuration", '/p:Platform=x64', "/p:SolutionDir=$projectRoot\")
if ($PackagesDirectory) { $common += "/p:RestorePackagesPath=$PackagesDirectory" }
Push-Location $projectRoot
try {
    Write-Host 'Restoring NuGet packages...'
    & $msbuild LayrixEditor.vcxproj /t:Restore @common
    if ($LASTEXITCODE) { throw "NuGet restore failed: $LASTEXITCODE" }
    # Generate XAML metadata before the first C++/WinRT compilation on a clean checkout.
    & $msbuild LayrixEditor.vcxproj /t:MarkupCompilePass1 @common
    if ($LASTEXITCODE) { throw "XAML pass 1 failed: $LASTEXITCODE" }
    Write-Host 'Building the WinUI editor...'
    & $msbuild LayrixEditor.vcxproj @common
    if ($LASTEXITCODE) { throw "Editor build failed: $LASTEXITCODE" }
    Write-Host 'Building the window engine...'
    & $msbuild Layrix.vcxproj @common
    if ($LASTEXITCODE) { throw "Engine build failed: $LASTEXITCODE" }
    if (!$SkipTests) {
        $testExecutable = Join-Path $projectRoot "bin/x64/$Configuration/Layrix.exe"
        $testProcess = Start-Process -FilePath $testExecutable -ArgumentList '--self-test' -WindowStyle Hidden -Wait -PassThru
        if ($testProcess.ExitCode) { throw "Self-test failed: $($testProcess.ExitCode)" }
        Write-Host 'SELFTEST OK'
    }
} finally { Pop-Location }
