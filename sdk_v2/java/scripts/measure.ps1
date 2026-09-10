# Copyright (c) Microsoft Corporation. Licensed under the MIT License.
# Samples only the launched process and its descendants; never terminates unrelated processes.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Java,
    [Parameter(Mandatory)][string[]]$JavaArguments,
    [Parameter(Mandatory)][string]$OutputPrefix,
    [int]$TimeoutSeconds = 600
)
$ErrorActionPreference = 'Stop'
$OutputPrefix = [IO.Path]::GetFullPath($OutputPrefix)
New-Item -ItemType Directory -Force (Split-Path $OutputPrefix) | Out-Null
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $Java
$start.UseShellExecute = $false
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$start.Environment['ORT_TELEMETRY_DISABLED'] = '1'
foreach ($argument in $JavaArguments) { $start.ArgumentList.Add($argument) }
$process = [Diagnostics.Process]::new()
$process.StartInfo = $start
$watch = [Diagnostics.Stopwatch]::StartNew()
if (!$process.Start()) { throw 'Java did not start' }
$stdout = $process.StandardOutput.ReadToEndAsync()
$stderr = $process.StandardError.ReadToEndAsync()
$ids = [Collections.Generic.HashSet[int]]::new()
[void]$ids.Add($process.Id)
$peakTreeWorking = 0L
$peakTreePrivate = 0L
$peakRootWorking = 0L
$samples = 0
try {
    while (!$process.HasExited) {
        if ($watch.Elapsed.TotalSeconds -gt $TimeoutSeconds) { throw 'Java exceeded the measurement timeout' }
        $pending = [Collections.Generic.Queue[int]]::new()
        $pending.Enqueue($process.Id)
        $seen = [Collections.Generic.HashSet[int]]::new()
        $working = 0L
        $private = 0L
        while ($pending.Count) {
            $processId = $pending.Dequeue()
            if (!$seen.Add($processId)) { continue }
            [void]$ids.Add($processId)
            try { $current = [Diagnostics.Process]::GetProcessById($processId) }
            catch [ArgumentException] { continue } # Child exited between discovery and sampling.
            try {
                $current.Refresh()
                if (!$current.HasExited) {
                    $working += $current.WorkingSet64
                    $private += $current.PrivateMemorySize64
                    if ($processId -eq $process.Id) {
                        $peakRootWorking = [Math]::Max($peakRootWorking, $current.PeakWorkingSet64)
                    }
                }
            } finally { $current.Dispose() }
            Get-CimInstance Win32_Process -Filter "ParentProcessId=$processId" | ForEach-Object {
                $pending.Enqueue([int]$_.ProcessId)
            }
        }
        $peakTreeWorking = [Math]::Max($peakTreeWorking, $working)
        $peakTreePrivate = [Math]::Max($peakTreePrivate, $private)
        $samples++
        Start-Sleep -Milliseconds 100
    }
    $process.WaitForExit()
    $watch.Stop()
    [IO.File]::WriteAllText("$OutputPrefix.stdout.jsonl", $stdout.GetAwaiter().GetResult())
    [IO.File]::WriteAllText("$OutputPrefix.stderr.txt", $stderr.GetAwaiter().GetResult())
    $metrics = [ordered]@{
        elapsedMillis = $watch.ElapsedMilliseconds
        exitCode = $process.ExitCode
        processCount = $ids.Count
        sampleCount = $samples
        sampledPeakTreeWorkingSetBytes = $peakTreeWorking
        sampledPeakTreePrivateBytes = $peakTreePrivate
        observedOsPeakRootWorkingSetBytes = $peakRootWorking
        method = '100ms sleep plus process-tree enumeration; sampled values are lower bounds'
    }
    $metrics | ConvertTo-Json | Set-Content "$OutputPrefix.metrics.json"
    $metrics | ConvertTo-Json
    if ($process.ExitCode -ne 0) { throw "Java failed; inspect $OutputPrefix.stderr.txt and stdout.jsonl" }
} finally {
    if (!$process.HasExited) { $process.Kill($true); $process.WaitForExit() }
    $process.Dispose()
}
