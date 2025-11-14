using Microsoft.Extensions.Logging;
using System.Text;

internal static class Utils
{
    /// <summary>
    /// Get a dummy application logger.
    /// </summary>
    /// <returns>ILogger</returns>
    internal static ILogger GetAppLogger()
    {
        using var loggerFactory = LoggerFactory.Create(builder =>
        {
            builder.SetMinimumLevel(Microsoft.Extensions.Logging.LogLevel.Debug);
        });

        return loggerFactory.CreateLogger("FoundryLocalSamples");
    }

    internal static async Task RunWithSpinner<T>(string msg, T workTask) where T : Task
    {
        // Start the spinner
        using var cts = new CancellationTokenSource();
        var spinnerTask = ShowSpinner(msg, cts.Token);

        await workTask;     // wait for the real work to finish
        cts.Cancel();       // stop the spinner
        await spinnerTask;  // wait for spinner to exit
    }

    private static async Task ShowSpinner(string msg, CancellationToken token)
    {
        Console.OutputEncoding = Encoding.UTF8;

        var sequence = new[] { '◴','◷','◶','◵' };

        int counter = 0;

        while (!token.IsCancellationRequested)
        {
            Console.Write($"{msg}\t{sequence[counter % sequence.Length]}");
            Console.SetCursorPosition(0, Console.CursorTop);
            counter++;
            await Task.Delay(200, token).ContinueWith(_ => { });
        }

        Console.WriteLine($"\nDone.\n");
    }
}