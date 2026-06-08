param([string]$CobFile = "build\Windows\Debug\coverage\coverage.xml")

[xml]$cob = Get-Content $CobFile
$fileMap = @{}
foreach ($pkg in $cob.coverage.packages.package) {
    foreach ($cls in $pkg.classes.class) {
        $filename = $cls.filename -replace '.*\\src\\', 'src\'
        if (-not $fileMap.ContainsKey($filename)) { $fileMap[$filename] = @{} }
        foreach ($line in $cls.lines.line) {
            $num = $line.number; $hits = [int]$line.hits
            if ($fileMap[$filename].ContainsKey($num)) {
                if ($hits -gt 0) { $fileMap[$filename][$num] = $true }
            } else {
                $fileMap[$filename][$num] = ($hits -gt 0)
            }
        }
    }
}

$results = @(); $totalCovered = 0; $totalLines = 0
foreach ($file in $fileMap.Keys) {
    $lines = $fileMap[$file]; $cov = @($lines.Values | Where-Object { $_ }).Count; $tot = $lines.Count
    $pct = if ($tot -gt 0) { [math]::Round(100 * $cov / $tot) } else { 0 }
    $results += [PSCustomObject]@{ File = $file; Covered = $cov; Total = $tot; Pct = $pct }
    $totalCovered += $cov; $totalLines += $tot
}
$totalPct = if ($totalLines -gt 0) { [math]::Round(100 * $totalCovered / $totalLines) } else { 0 }

Write-Host ""
Write-Host "=== Coverage: $totalCovered / $totalLines lines ($totalPct%) ===" -ForegroundColor Cyan
Write-Host ""
Write-Host ("{0,-70} {1,8} {2,8} {3,6}" -f "File", "Covered", "Total", "Rate")
Write-Host ("-" * 95)
$results | Sort-Object Pct | ForEach-Object {
    $color = if ($_.Pct -lt 50) { "Red" } elseif ($_.Pct -lt 80) { "Yellow" } else { "Green" }
    Write-Host ("{0,-70} {1,8} {2,8} {3,5}%" -f $_.File, $_.Covered, $_.Total, $_.Pct) -ForegroundColor $color
}
