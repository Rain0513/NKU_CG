# Run serial measurements so the two backends do not compete for CPU time.
param(
    [ValidateRange(1, 4096)][int]$Size = 128,
    [ValidateRange(1, 1024)][int]$Samples = 1,
    [ValidateRange(0, 12)][int]$Depth = 6,
    [ValidateRange(1, 100)][int]$Repeat = 5,
    [string[]]$Scenes = @('ray_tracing_lab3')
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot 'code\out\build\ray-tracing-release'
$check = Join-Path $buildRoot 'RayTracingCheck.exe'
if (!(Test-Path -LiteralPath $check)) { throw 'First run tools/build_ray_tracing.ps1 -WithChecks.' }
$resultRoot = Join-Path $projectRoot 'code\out\lab3-results'
New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null
function Get-Median($values) {
    $sorted = @($values | ForEach-Object { [double]$_ } | Sort-Object)
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2) { return $sorted[$middle] }
    return ($sorted[$middle - 1] + $sorted[$middle]) / 2
}
$summary = @()
Push-Location $buildRoot
try {
    & $check --self-test
    if ($LASTEXITCODE -ne 0) { throw 'Regression check failed.' }
    foreach ($sceneName in $Scenes) {
        $scene = Join-Path $projectRoot "resource\$sceneName.scn"
        # Record the input before loading it; reject a mixed report if a scene
        # is edited during timing or the separate diagnostic run.
        $sceneHash = (Get-FileHash -LiteralPath $scene -Algorithm SHA256).Hash.ToLowerInvariant()
        $destination = Join-Path $resultRoot $sceneName
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        & $check --scene $scene --output $destination --size $Size --samples $Samples --depth $Depth --repeat $Repeat > (Join-Path $destination 'render.log')
        if ($LASTEXITCODE -ne 0) { throw "Validation/benchmark failed for $sceneName" }
        # Counters alter hot-loop cost, so collect them separately from timing.
        $diagnostics = Join-Path $destination 'diagnostics'
        & $check --diagnostics --scene $scene --output $diagnostics --size $Size --samples $Samples --depth $Depth --repeat 1 > (Join-Path $destination 'diagnostics.log')
        if ($LASTEXITCODE -ne 0) { throw "Diagnostic validation failed for $sceneName" }
        if ((Get-FileHash -LiteralPath $scene -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sceneHash) {
            throw "Scene changed during measurement: $sceneName. Rerun with a fixed scene."
        }
        $rows = @(Import-Csv (Join-Path $destination 'measurements.csv'))
        $brute = @($rows | Where-Object backend -eq 'BruteForce')
        $kd = @($rows | Where-Object backend -eq 'KDTree')
        $bruteMs = Get-Median $brute.trace_ms
        $kdMs = Get-Median $kd.trace_ms
        $bruteExecutionMs = Get-Median $brute.execution_ms
        $kdExecutionMs = Get-Median $kd.execution_ms
        $diagnosticRows = @(Import-Csv (Join-Path $diagnostics 'measurements.csv'))
        $bruteCounters = $diagnosticRows | Where-Object backend -eq 'BruteForce'
        $kdCounters = $diagnosticRows | Where-Object backend -eq 'KDTree'
        $summary += [pscustomobject]@{
            Scene=$sceneName; Primitives=$brute[0].primitives; Width=$Size; Height=$Size; Samples=$Samples; Depth=$Depth; Repeats=$Repeat
            BruteTraceMs=$bruteMs; KDTraceMs=$kdMs; KDBuildMs=(Get-Median $kd.build_ms)
            BruteExecutionMs=$bruteExecutionMs; KDExecutionMs=$kdExecutionMs; TotalSpeedup=$bruteExecutionMs/$kdExecutionMs
            TraceSpeedup=$bruteMs/$kdMs; BrutePrimitiveTests=$bruteCounters.primitive_tests; KDPrimitiveTests=$kdCounters.primitive_tests; KDBoxTests=$kdCounters.box_tests
            MaxPixelError=($rows.max_pixel_error | ForEach-Object { [double]$_ } | Measure-Object -Maximum).Maximum
            SceneSHA256=$sceneHash
        }
        Write-Host "Completed $sceneName (total speedup: $([Math]::Round($bruteExecutionMs/$kdExecutionMs, 2))x)"
    }
} finally { Pop-Location }
$summaryPath = Join-Path $resultRoot 'summary.csv'
# A targeted rerun replaces those rows while retaining other measured scenes.
$retained = @()
if (Test-Path -LiteralPath $summaryPath) {
    $retained = @(Import-Csv $summaryPath | Where-Object {
        $_.Scene -notin $Scenes -and (Test-Path -LiteralPath (Join-Path $projectRoot ("resource\" + $_.Scene + '.scn')))
    })
}
@($retained) + @($summary) | Export-Csv -NoTypeInformation -Encoding UTF8 -Path $summaryPath
$summary | Format-Table Scene,Primitives,BruteExecutionMs,KDExecutionMs,TotalSpeedup,MaxPixelError
Write-Host "Results, BMP previews and raw timing data: $resultRoot"
