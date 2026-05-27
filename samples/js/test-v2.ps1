<#
.SYNOPSIS
    Validate samples/js against the local sdk_v2/js build.

.DESCRIPTION
    The samples declare `"foundry-local-sdk": "latest"`, which resolves to the
    published v1 package on npmjs.com. This script:

      1. Builds sdk_v2/js (TS + native addon + prebuilds copy).
      2. Runs `npm pack` to produce a tarball.
      3. For each sample under samples/js/, runs
         `npm install --omit=optional <tarball>`, which installs the v2 tarball
         AS `foundry-local-sdk` (the package name inside the tarball wins,
         overriding the "latest" specifier) and skips the WinML optional dep
         (which has no v2 equivalent yet).
      4. Optionally runs each sample with `npm start` under a per-sample
         timeout and reports pass/fail.

    Nothing about the samples' package.json files is modified. Run the script
    again any time after a rebuild to re-pack and reinstall.

.PARAMETER Sample
    Run only the named sample (folder name under samples/js/). Default: all.

.PARAMETER SkipBuild
    Skip the sdk_v2/js build + pack step. Use when iterating on the script
    itself or when you've already produced a fresh tarball.

.PARAMETER Run
    After installing, actually run each sample (npm start) under a timeout.
    Default behaviour is install-only — you usually want to run a single
    sample interactively with `npm start` after install.

.PARAMETER TimeoutSec
    Per-sample timeout when -Run is supplied. Default: 120s.

.EXAMPLE
    pwsh ./test-v2.ps1
    # Build + pack v2, install into every sample. Does not run anything.

.EXAMPLE
    pwsh ./test-v2.ps1 -Sample native-chat-completions -Run
    # Build, pack, install just one sample, then `npm start` it.

.EXAMPLE
    pwsh ./test-v2.ps1 -SkipBuild -Run
    # Reuse the existing tarball; install + smoke-run every sample.
#>
[CmdletBinding()]
param(
    [string] $Sample,
    [switch] $SkipBuild,
    [switch] $Run,
    [int]    $TimeoutSec = 120
)

$ErrorActionPreference = 'Stop'
$samplesRoot = $PSScriptRoot
$repoRoot    = Resolve-Path (Join-Path $samplesRoot '..\..')
$sdkDir      = Join-Path $repoRoot 'sdk_v2\js'

if (-not (Test-Path $sdkDir)) {
    throw "Cannot find sdk_v2/js at $sdkDir"
}

# ---------- 1. build + pack ----------
if (-not $SkipBuild) {
    Write-Host "==> Building sdk_v2/js" -ForegroundColor Cyan
    Push-Location $sdkDir
    try {
        npm install
        if ($LASTEXITCODE -ne 0) { throw "npm install in sdk_v2/js failed" }
        npm run build
        if ($LASTEXITCODE -ne 0) { throw "npm run build in sdk_v2/js failed" }

        # Clean stale tarballs so we always pick up the freshest one.
        Get-ChildItem -Path $sdkDir -Filter 'foundry-local-sdk-*.tgz' |
            Remove-Item -Force -ErrorAction SilentlyContinue

        Write-Host "==> Packing sdk_v2/js" -ForegroundColor Cyan
        npm pack
        if ($LASTEXITCODE -ne 0) { throw "npm pack in sdk_v2/js failed" }
    }
    finally {
        Pop-Location
    }
}

$tarball = Get-ChildItem -Path $sdkDir -Filter 'foundry-local-sdk-*.tgz' |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $tarball) {
    throw "No tarball found in $sdkDir. Run without -SkipBuild first."
}
$tarballPath = $tarball.FullName
Write-Host "Using tarball: $tarballPath" -ForegroundColor DarkGray

# ---------- 2. discover samples ----------
$candidateDirs = Get-ChildItem -Path $samplesRoot -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName 'package.json') }

if ($Sample) {
    $candidateDirs = $candidateDirs | Where-Object { $_.Name -eq $Sample }
    if (-not $candidateDirs) {
        throw "Sample '$Sample' not found (or has no package.json)."
    }
}

# Only touch samples that actually depend on foundry-local-sdk.
$samples = foreach ($d in $candidateDirs) {
    $pkg = Get-Content (Join-Path $d.FullName 'package.json') -Raw |
        ConvertFrom-Json
    $hasDep =
        ($pkg.dependencies         -and $pkg.dependencies.'foundry-local-sdk') -or
        ($pkg.optionalDependencies -and $pkg.optionalDependencies.'foundry-local-sdk')
    if ($hasDep) { $d }
}

if (-not $samples) {
    Write-Host "No samples depend on foundry-local-sdk. Nothing to do." -ForegroundColor Yellow
    return
}

# ---------- 3. install + (optionally) run ----------
$results = New-Object System.Collections.Generic.List[object]

foreach ($sampleDir in $samples) {
    $name = $sampleDir.Name
    Write-Host ""
    Write-Host "==> [$name] install" -ForegroundColor Cyan
    Push-Location $sampleDir.FullName
    $installOk = $false
    $runOk     = $null
    $note      = ''
    try {
        # Remove old install so the tarball is the canonical source.
        Remove-Item -Recurse -Force node_modules, package-lock.json -ErrorAction SilentlyContinue

        # `npm install <tgz>` registers the tarball under its internal package
        # name (`foundry-local-sdk`), which satisfies the "latest" specifier
        # in the sample's package.json. --omit=optional skips the WinML pkg.
        # --registry forces public npmjs.org, overriding any sample-local
        # .npmrc pointing at the ORT-Nightly Azure feed (which doesn't proxy
        # transitive deps like node-addon-api).
        npm install --omit=optional --registry=https://registry.npmjs.org/ $tarballPath 2>&1 | Write-Host
        $installOk = ($LASTEXITCODE -eq 0)
        if (-not $installOk) { $note = "npm install exit $LASTEXITCODE" }

        if ($installOk -and $Run) {
            Write-Host "==> [$name] run (timeout ${TimeoutSec}s)" -ForegroundColor Cyan

            # NOTE: do NOT use Start-Job here. PowerShell jobs run in a child
            # PSHost with no real console — native code in foundry_local.dll
            # that touches console / stdout handles can crash with 0xC0000005
            # under Start-Job but works fine under a normal console.
            #
            # Let stdout inherit the parent console so the user sees live
            # output (samples write progress, prompts, model responses, etc.
            # — redirecting stdout to a file hides all of that until the run
            # finishes). Only stderr is captured to a file so we can surface
            # it on failure without interleaving it into the live output.
            $errFile = Join-Path $sampleDir.FullName 'sample-run.err.log'
            Remove-Item $errFile -Force -ErrorAction SilentlyContinue
            $proc = Start-Process -FilePath 'npm.cmd' -ArgumentList 'start' `
                -WorkingDirectory $sampleDir.FullName `
                -NoNewWindow -PassThru `
                -RedirectStandardError $errFile
            $exited = $proc.WaitForExit($TimeoutSec * 1000)
            if (-not $exited) {
                try { $proc.Kill($true) } catch { }
                $runOk = $false
                $note  = "timed out after ${TimeoutSec}s"
                Write-Host $note -ForegroundColor Yellow
            }
            else {
                $exit  = $proc.ExitCode
                $runOk = ($exit -eq 0)
                if (-not $runOk) { $note = "npm start exit $exit" }
            }
            # Always surface stderr if non-empty — npm warnings go to stderr
            # too, but on failure this is usually where the real error lives.
            if ((Test-Path $errFile) -and ((Get-Item $errFile).Length -gt 0)) {
                Write-Host "--- stderr ($errFile) ---" -ForegroundColor DarkGray
                Get-Content $errFile | Select-Object -First 40 | Write-Host
            }
        }
    }
    catch {
        $note = $_.Exception.Message
        Write-Host "ERROR: $note" -ForegroundColor Red
    }
    finally {
        Pop-Location
    }

    $results.Add([pscustomobject]@{
        Sample  = $name
        Install = if ($installOk) { 'OK' } else { 'FAIL' }
        Run     = if ($null -eq $runOk) { '-' } elseif ($runOk) { 'OK' } else { 'FAIL' }
        Note    = $note
    })
}

# ---------- 4. summary ----------
Write-Host ""
Write-Host "==> Summary" -ForegroundColor Cyan
$results | Format-Table -AutoSize

$anyFail = $results | Where-Object { $_.Install -eq 'FAIL' -or $_.Run -eq 'FAIL' }
if ($anyFail) { exit 1 }
