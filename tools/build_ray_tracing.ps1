# Build with the installed Visual Studio toolchain; no machine-specific path is
# stored in CMake. Run in PowerShell from any working directory.
param(
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [switch]$WithChecks,
    [switch]$Clean
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer/vswhere.exe was not found.' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Install the Visual Studio Desktop development with C++ workload.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
$cmake = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
foreach ($required in @($vcvars, $cmake, $ninja)) {
    if (!(Test-Path -LiteralPath $required)) { throw "Missing build tool: $required" }
}
$buildRoot = Join-Path $projectRoot ('code\out\build\ray-tracing-' + $Configuration.ToLowerInvariant())
New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null
$buildLog = Join-Path $buildRoot 'build.log'
$checkOption = if ($WithChecks) { 'ON' } else { 'OFF' }
$targets = 'NRenderer RayCast SimplePathTracing RenderExample'
$cleanOption = if ($Clean) { '--clean-first' } else { '' }
if ($WithChecks) { $targets += ' RayTracingCheck' }
# cmd initializes MSVC only for this child process. All paths are quoted.
$command = 'chcp 65001 >nul && call "{0}" >nul && "{1}" -S "{2}\code" -B "{3}" -G Ninja -DCMAKE_BUILD_TYPE={4} "-DCMAKE_MAKE_PROGRAM={5}" -DNR_BUILD_RAY_TRACING_CHECK={6} && "{1}" --build "{3}" {8} --target {7} -j 8' -f $vcvars,$cmake,$projectRoot,$buildRoot,$Configuration,$ninja,$checkOption,$targets,$cleanOption
& $env:ComSpec /d /s /c $command *> $buildLog
if ($LASTEXITCODE -ne 0) {
    Get-Content -LiteralPath $buildLog -Tail 50
    throw "Build failed. Full log: $buildLog"
}
Write-Host "Built: $buildRoot\NRenderer.exe"
Write-Host "Log: $buildLog"
