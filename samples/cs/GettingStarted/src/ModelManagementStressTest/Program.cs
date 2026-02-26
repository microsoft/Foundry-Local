using Microsoft.AI.Foundry.Local;
using System.Diagnostics;
using System.Runtime.InteropServices;

// ============================================================
// Download Resume Stress Test
// ============================================================
// Tests whether a killed/crashed download can be resumed by a
// new process — multiple times in sequence.
// Each iteration deletes the model, then walks through all
// killAtPercents thresholds, killing and resuming at each one,
// before finally letting the download complete.
// Runs as two modes:
//   - Controller (default): orchestrates kill+resume cycles
//   - Child (--child): starts a download and writes progress
//     to stdout until killed by the controller
// ============================================================

const string ModelAlias = "qwen2.5-7b";
const int MaxIterations = 10;

// Kill the child at these progress thresholds (round-robin)
float[] killAtPercents = [5f, 10f, 25f, 50f, 75f, 90f];

// ============================================================
// Child mode: just download and report progress to stdout
// ============================================================
if (args.Length > 0 && args[0] == "--child")
{
    var childConfig = new Configuration
    {
        AppName = "foundry_local_download_stress_test",
        LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Warning
    };

    await FoundryLocalManager.CreateAsync(childConfig, Utils.GetAppLogger());
    var childMgr = FoundryLocalManager.Instance;
    await childMgr.EnsureEpsDownloadedAsync();

    var childCatalog = await childMgr.GetCatalogAsync();
    var childModel = await childCatalog.GetModelAsync(ModelAlias)
        ?? throw new Exception($"Model '{ModelAlias}' not found");
    var childVariant = childModel.Variants.First(v => v.Info.Runtime?.DeviceType == DeviceType.CPU);
    childModel.SelectVariant(childVariant);

    // Signal ready
    Console.WriteLine("READY");
    Console.Out.Flush();

    await childModel.DownloadAsync(progress =>
    {
        // Write progress so the controller can read it
        Console.WriteLine($"PROGRESS:{progress:F2}");
        Console.Out.Flush();
    });

    Console.WriteLine("DONE");
    Console.Out.Flush();
    return 0;
}

// ============================================================
// Controller mode
// ============================================================
Console.WriteLine("Download Resume Stress Test (process-kill based)");
Console.WriteLine($"Model:       {ModelAlias}");
Console.WriteLine($"Iterations:  {MaxIterations}");
Console.WriteLine($"Kill-at:     [{string.Join("%, ", killAtPercents)}%]");
Console.WriteLine();

// We need the SDK initialized to call RemoveFromCacheAsync and IsCachedAsync
var config = new Configuration
{
    AppName = "foundry_local_download_stress_test",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Warning
};

await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;
Console.WriteLine("Registering execution providers...");
await mgr.EnsureEpsDownloadedAsync();
Console.WriteLine("Done.");

var catalog = await mgr.GetCatalogAsync();
var model = await catalog.GetModelAsync(ModelAlias)
    ?? throw new Exception($"Model '{ModelAlias}' not found");
var modelVariant = model.Variants.First(v => v.Info.Runtime?.DeviceType == DeviceType.CPU);
model.SelectVariant(modelVariant);

// Find this executable's path
var exePath = Environment.ProcessPath
    ?? throw new Exception("Cannot determine current process path");

var overallSw = Stopwatch.StartNew();
int successfulResumes = 0;
int failedResumes = 0;
int killsDone = 0;

// Discover the model path once. We need to download it first if not cached.
string? knownModelPath = null;
double baselineDownloadSeconds = 0;
try
{
    knownModelPath = await model.GetPathAsync();
    Console.WriteLine($"Model path: {knownModelPath}");
}
catch
{
    // Model not cached yet — download it once to discover the path and measure baseline time
    Console.WriteLine("Model not cached yet, downloading once to discover path and measure baseline...");
    var baselineSw = Stopwatch.StartNew();
    await model.DownloadAsync(progress =>
    {
        Console.Write($"\rDownloading: {progress:F1}%");
        if (progress >= 100f) Console.WriteLine();
    });
    baselineSw.Stop();
    baselineDownloadSeconds = baselineSw.Elapsed.TotalSeconds;
    Console.WriteLine($"Baseline full download: {baselineDownloadSeconds:F1}s");
    try
    {
        knownModelPath = await model.GetPathAsync();
        Console.WriteLine($"Model path: {knownModelPath}");
    }
    catch (Exception ex)
    {
        Console.WriteLine($"[ERROR] Could not discover model path even after download: {ex.Message}");
        return 1;
    }
}

// If model was already cached, do a forced re-download to measure baseline
if (baselineDownloadSeconds == 0)
{
    Console.WriteLine("Measuring baseline download time...");
    try { await model.RemoveFromCacheAsync(); } catch { }
    if (knownModelPath != null && Directory.Exists(knownModelPath))
    {
        try { Directory.Delete(knownModelPath, recursive: true); } catch { }
    }
    var baselineSw = Stopwatch.StartNew();
    await model.DownloadAsync(progress =>
    {
        Console.Write($"\rDownloading: {progress:F1}%");
        if (progress >= 100f) Console.WriteLine();
    });
    baselineSw.Stop();
    baselineDownloadSeconds = baselineSw.Elapsed.TotalSeconds;
    Console.WriteLine($"Baseline full download: {baselineDownloadSeconds:F1}s");
}
Console.WriteLine();
int resumeVerifiedCount = 0;
int resumeNotVerifiedCount = 0;

for (int iteration = 1; iteration <= MaxIterations; iteration++)
{
    Console.WriteLine($"== Iteration {iteration}/{MaxIterations} ==========================================");

    // Step 1: Clear cache via SDK so the model is not considered "cached"
    try
    {
        await model.RemoveFromCacheAsync();
    }
    catch (Exception ex)
    {
        Console.WriteLine($"  [WARN] RemoveFromCacheAsync: {ex.Message}");
    }

    // Delete the model directory from disk to force a fresh download.
    if (knownModelPath != null && Directory.Exists(knownModelPath))
    {
        try
        {
            Directory.Delete(knownModelPath, recursive: true);
            Console.WriteLine($"  Deleted model dir: {knownModelPath}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"  [WARN] Failed to delete model dir: {ex.Message}");
        }
    }

    // Step 2: Walk through each kill threshold, killing and resuming at each one
    bool iterationFailed = false;
    var segmentTimes = new List<(float fromPct, float toPct, double seconds)>();
    float previousKillPct = 0;
    for (int ki = 0; ki < killAtPercents.Length; ki++)
    {
        float killAt = killAtPercents[ki];
        bool isLastThreshold = (ki == killAtPercents.Length - 1);
        Console.WriteLine($"  -- Kill phase {ki + 1}/{killAtPercents.Length} [kill@{killAt:F0}%] --");

        // Clear SDK cache index before each child, but DO NOT delete files on disk.
        // This lets the child discover and resume from partial files.
        try { await model.RemoveFromCacheAsync(); } catch { }

        // Launch child process
        Console.WriteLine($"    Launching child (will kill at ~{killAt:F0}%)...");
        var psi = new ProcessStartInfo
        {
            FileName = exePath,
            Arguments = "--child",
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };

        using var child = Process.Start(psi)
            ?? throw new Exception("Failed to start child process");

        float lastProgress = 0;
        bool childCompleted = false;
        bool childReady = false;
        var killSw = Stopwatch.StartNew();
        var lastProgressPrint = Stopwatch.StartNew();

        try
        {
            while (!child.HasExited)
            {
                var line = await child.StandardOutput.ReadLineAsync();
                if (line == null) break;

                if (line == "READY") { childReady = true; continue; }
                if (line == "DONE") { childCompleted = true; break; }

                if (line.StartsWith("PROGRESS:") &&
                    float.TryParse(line.AsSpan("PROGRESS:".Length), out var p))
                {
                    lastProgress = p;

                    if (lastProgressPrint.Elapsed.TotalSeconds >= 1.0)
                    {
                        Console.Write($"\r      download: {p:F1}%  ");
                        lastProgressPrint.Restart();
                    }

                    if (p >= killAt)
                    {
                        Console.Write($"\r                        \r");
                        child.Kill(entireProcessTree: true);
                        killSw.Stop();
                        killsDone++;
                        Console.WriteLine($"    Killed at {lastProgress:F1}% ({killSw.Elapsed.TotalSeconds:F1}s)");
                        break;
                    }
                }
            }
        }
        catch (InvalidOperationException) { }

        if (!child.HasExited)
        {
            try { child.Kill(entireProcessTree: true); } catch { }
        }
        await child.WaitForExitAsync();

        if (childCompleted)
        {
            // Child finished before we could kill it — that's fine, iteration is done
            Console.Write($"\r                        \r");
            Console.WriteLine($"    Child completed download before kill@{killAt:F0}% ({killSw.Elapsed.TotalSeconds:F1}s)");
            break; // no more kill phases needed for this iteration
        }

        if (!childReady)
        {
            Console.WriteLine($"    [FAIL] Child never signaled READY");
            iterationFailed = true;
            break;
        }

        // Record segment timing
        segmentTimes.Add((previousKillPct, lastProgress, killSw.Elapsed.TotalSeconds));
        previousKillPct = lastProgress;

        // Show partial file state
        if (knownModelPath != null && Directory.Exists(knownModelPath))
        {
            try
            {
                var files = Directory.GetFiles(knownModelPath, "*", SearchOption.AllDirectories);
                long logicalBytes = 0;
                long diskBytes = 0;
                foreach (var f in files)
                {
                    logicalBytes += new FileInfo(f).Length;
                    diskBytes += GetSizeOnDisk(f);
                }
                Console.WriteLine($"    Partial files: {files.Length} files, " +
                                  $"logical={logicalBytes / (1024.0 * 1024.0):F1} MB, " +
                                  $"on-disk={diskBytes / (1024.0 * 1024.0):F1} MB");
            }
            catch { }
        }
    }

    if (iterationFailed)
    {
        failedResumes++;
        PrintProgress(iteration);
        continue;
    }

    // Step 3: Final resume — let the download complete
    Console.WriteLine($"  -- Final resume --");
    try { await model.RemoveFromCacheAsync(); } catch { }

    Console.WriteLine($"    Resuming download to completion...");
    var resumeSw = Stopwatch.StartNew();

    var resumePsi = new ProcessStartInfo
    {
        FileName = exePath,
        Arguments = "--child",
        RedirectStandardOutput = true,
        RedirectStandardError = true,
        UseShellExecute = false,
        CreateNoWindow = true
    };

    using var resumeChild = Process.Start(resumePsi)
        ?? throw new Exception("Failed to start resume child process");

    bool resumeCompleted = false;
    float resumeLastProgress = 0;
    float resumeFirstProgress = -1;
    var resumeProgressPrint = Stopwatch.StartNew();

    try
    {
        while (!resumeChild.HasExited)
        {
            var line = await resumeChild.StandardOutput.ReadLineAsync();
            if (line == null) break;

            if (line == "DONE") { resumeCompleted = true; break; }

            if (line.StartsWith("PROGRESS:") &&
                float.TryParse(line.AsSpan("PROGRESS:".Length), out var p))
            {
                if (resumeFirstProgress < 0) resumeFirstProgress = p;
                resumeLastProgress = p;

                if (resumeProgressPrint.Elapsed.TotalSeconds >= 1.0)
                {
                    Console.Write($"\r      resume: {p:F1}%  ");
                    resumeProgressPrint.Restart();
                }
            }
        }
    }
    catch (InvalidOperationException) { }

    await resumeChild.WaitForExitAsync();
    resumeSw.Stop();
    Console.Write("\r                        \r");

    if (!resumeCompleted)
    {
        failedResumes++;
        Console.WriteLine($"    [FAIL] Resume child did not complete " +
                          $"(exit={resumeChild.ExitCode}, last={resumeLastProgress:F1}%)");
    }
    else if (resumeChild.ExitCode != 0)
    {
        failedResumes++;
        Console.WriteLine($"    [FAIL] Resume child exited with code {resumeChild.ExitCode}");
    }
    else
    {
        successfulResumes++;

        // Record final resume segment
        segmentTimes.Add((previousKillPct, 100f, resumeSw.Elapsed.TotalSeconds));

        // Analyze resume effectiveness
        double totalSegmentTime = segmentTimes.Sum(s => s.seconds);
        Console.WriteLine($"    Resume OK ({resumeSw.Elapsed.TotalSeconds:F1}s) " +
                          $"first_progress={resumeFirstProgress:F1}%");
        Console.WriteLine($"    Segment breakdown:");
        foreach (var (fromPct, toPct, secs) in segmentTimes)
        {
            double deltaPct = toPct - fromPct;
            double rate = deltaPct > 0 ? secs / deltaPct * 100 : 0;
            Console.WriteLine($"      {fromPct,5:F1}% -> {toPct,5:F1}%  " +
                              $"delta={deltaPct,5:F1}%  time={secs,5:F1}s  " +
                              $"(~{rate:F1}s per 100%)");
        }
        Console.WriteLine($"    Total segment time:  {totalSegmentTime:F1}s");
        Console.WriteLine($"    Baseline full time:  {baselineDownloadSeconds:F1}s");
        double ratio = baselineDownloadSeconds > 0 ? totalSegmentTime / baselineDownloadSeconds : 0;
        Console.WriteLine($"    Ratio (segment/baseline): {ratio:F2}x");

        // Resume is working if total segment time is within ~2x of baseline.
        // If it were re-downloading from scratch each time, the ratio would be
        // ~(N+1)x where N is the number of kill phases.
        bool resumeWorking = ratio < 2.5;
        if (resumeWorking)
        {
            resumeVerifiedCount++;
            Console.WriteLine($"    >> RESUME VERIFIED (ratio {ratio:F2}x < 2.5x baseline)");
        }
        else
        {
            resumeNotVerifiedCount++;
            Console.WriteLine($"    >> RESUME NOT VERIFIED (ratio {ratio:F2}x >= 2.5x baseline — " +
                              $"may be re-downloading from scratch)");
        }
    }

    PrintProgress(iteration);
}

// ============================================================
// Final Summary
// ============================================================
overallSw.Stop();
Console.WriteLine();
Console.WriteLine("============================================================");
Console.WriteLine("  DOWNLOAD RESUME STRESS TEST SUMMARY");
Console.WriteLine("============================================================");
Console.WriteLine($"  Total iterations:      {MaxIterations}");
Console.WriteLine($"  Kills performed:       {killsDone}");
Console.WriteLine($"  Successful resumes:    {successfulResumes}");
Console.WriteLine($"  Failed resumes:        {failedResumes}");
Console.WriteLine($"  Resume verified:       {resumeVerifiedCount}");
Console.WriteLine($"  Resume NOT verified:   {resumeNotVerifiedCount}");
Console.WriteLine($"  Baseline download:     {baselineDownloadSeconds:F1}s");
Console.WriteLine($"  Total elapsed:         {overallSw.Elapsed}");
string result = failedResumes > 0 ? "FAIL (download errors)" :
                resumeNotVerifiedCount > 0 ? "FAIL (resume not working)" : "PASS";
Console.WriteLine($"  Result:                {result}");
Console.WriteLine("============================================================");

return (failedResumes == 0 && resumeNotVerifiedCount == 0) ? 0 : 1;

void PrintProgress(int currentIteration)
{
    var elapsed = overallSw.Elapsed;
    var rate = currentIteration / Math.Max(elapsed.TotalMinutes, 0.001);
    Console.WriteLine($"  [{elapsed:hh\\:mm\\:ss}] iter={currentIteration} " +
                      $"ok={successfulResumes} failed={failedResumes} " +
                      $"kills={killsDone} ({rate:F1}/min)");
    Console.WriteLine();
}

// Get the actual size on disk (compressed/sparse-aware).
// Falls back to logical size on non-Windows or on error.
static long GetSizeOnDisk(string filePath)
{
    if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
    {
        uint high;
        uint low = GetCompressedFileSizeW(filePath, out high);
        if (low == 0xFFFFFFFF && Marshal.GetLastWin32Error() != 0)
            return new FileInfo(filePath).Length; // fallback
        return ((long)high << 32) | low;
    }
    return new FileInfo(filePath).Length;
}

[DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
static extern uint GetCompressedFileSizeW(string lpFileName, out uint lpFileSizeHigh);
