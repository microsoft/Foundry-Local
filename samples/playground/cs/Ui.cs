// Terminal UI helpers for Foundry Local Playground.
// Handles all terminal drawing so the main app can focus on SDK calls.

using System.Text;
using System.Text.RegularExpressions;

namespace FoundryPlayground;

internal static partial class Ui
{
    private const string BlockFull = "█";
    private const string BlockEmpty = "░";
    private const int BarWidth = 30;

    [GeneratedRegex(@"\x1b\[[0-9;]*m")]
    private static partial Regex AnsiRegex();

    // ── Primitives ───────────────────────────────────────────────────────

    private static int VisibleLength(string s) => AnsiRegex().Replace(s, "").Length;

    private static string PadVisible(string s, int width)
    {
        var pad = Math.Max(0, width - VisibleLength(s));
        return s + new string(' ', pad);
    }

    public static string ProgressBar(double percent, int width = BarWidth)
    {
        int filled = (int)(width * percent / 100);
        return new string('█', filled) + new string('░', width - filled);
    }

    public static void Section(string title)
    {
        int cols = Console.BufferWidth > 0 ? Console.BufferWidth : 80;
        Console.WriteLine();
        Console.WriteLine(new string('─', cols));
        Console.WriteLine($"  {title}");
        Console.WriteLine(new string('─', cols));
    }

    private static string Seg(int w) => new string('─', w + 2);

    private static string TableHr(int[] widths, string pos)
    {
        var (l, m, r) = pos switch
        {
            "top" => ("┌", "┬", "┐"),
            "mid" => ("├", "┼", "┤"),
            _ => ("└", "┴", "┘"),
        };
        return "  " + l + string.Join(m, widths.Select(Seg)) + r;
    }

    private static string TableRow(int[] widths, string[] values)
    {
        var sb = new StringBuilder("  │");
        for (int i = 0; i < widths.Length; i++)
        {
            var val = i < values.Length ? values[i] : "";
            sb.Append($" {PadVisible(val, widths[i])} │");
        }
        return sb.ToString();
    }

    public static string[] WrapText(string text, int maxWidth)
    {
        var result = new List<string>();
        foreach (var para in text.Split('\n'))
        {
            if (string.IsNullOrEmpty(para)) { result.Add(""); continue; }
            var line = "";
            foreach (var word in para.Split(' '))
            {
                if (line.Length > 0 && line.Length + word.Length + 1 > maxWidth)
                {
                    result.Add(line);
                    line = word;
                }
                else
                {
                    line = line.Length > 0 ? $"{line} {word}" : word;
                }
            }
            if (line.Length > 0) result.Add(line);
        }
        return result.Count > 0 ? result.ToArray() : [""];
    }

    // ── EP Table ─────────────────────────────────────────────────────────

    public static (Action<string, double> onProgress, Action<string[]?> finalize)
        ShowEpTable(IReadOnlyList<(string Name, bool IsRegistered)> eps)
    {
        if (eps.Count == 0)
        {
            Console.WriteLine("  No execution providers found.");
            return ((_, _) => { }, _ => { });
        }

        int col1 = Math.Max(7, eps.Max(e => e.Name.Length));
        int col2 = BarWidth + 7;
        int[] w = [col1, col2];

        string Fmt(string name, string cell) => TableRow(w, [name, cell]);

        Console.WriteLine(TableHr(w, "top"));
        Console.WriteLine(Fmt("EP Name", "Status"));
        Console.WriteLine(TableHr(w, "mid"));

        var epIdx = new Dictionary<string, int>();
        for (int i = 0; i < eps.Count; i++)
        {
            epIdx[eps[i].Name] = i;
            var cell = eps[i].IsRegistered
                ? "\x1b[32m● registered\x1b[0m"
                : ProgressBar(0) + "  0.0%";
            Console.WriteLine(Fmt(eps[i].Name, cell));
        }
        Console.WriteLine(TableHr(w, "bot"));

        void OnProgress(string epName, double percent)
        {
            if (!epIdx.TryGetValue(epName, out int idx)) return;
            int up = (eps.Count - idx - 1) + 1;
            Console.Write($"\x1b[{up}A\r");
            Console.Write(Fmt(epName, ProgressBar(percent) + $" {percent,5:F1}%"));
            Console.Write($"\x1b[K\n\x1b[{up - 1}B");
        }

        void Finalize(string[]? failedEps)
        {
            var failedSet = new HashSet<string>(failedEps ?? []);
            int totalLines = eps.Count + 4;
            Console.Write($"\x1b[{totalLines}A\r");
            Console.WriteLine(TableHr(w, "top"));
            Console.WriteLine(Fmt("EP Name", "Status"));
            Console.WriteLine(TableHr(w, "mid"));
            foreach (var ep in eps)
            {
                bool ok = ep.IsRegistered || !failedSet.Contains(ep.Name);
                var dot = ok ? "\x1b[32m● registered\x1b[0m" : "\x1b[31m● failed\x1b[0m";
                Console.Write(Fmt(ep.Name, dot) + "\x1b[K\n");
            }
            Console.Write(TableHr(w, "bot") + "\x1b[K\n");
        }

        return (OnProgress, Finalize);
    }

    // ── Model Catalog ────────────────────────────────────────────────────

    public record CatalogRow(int ModelIdx, int VariantIdx, string Alias, string VariantId,
                             string SizeGb, string Task, bool IsCached);

    public static List<CatalogRow> ShowCatalog(IReadOnlyList<CatalogRow> rows, int modelCount)
    {
        int mcNum = Math.Max(2, rows.Count.ToString().Length);
        int mcAlias = Math.Max(5, rows.Max(r => r.Alias.Length));
        int mcVariant = Math.Max(7, rows.Max(r => r.VariantId.Length));
        int mcSize = 10;
        int mcTask = Math.Max(4, rows.Max(r => r.Task.Length));
        int mcCached = 6;
        int[] mc = [mcNum, mcAlias, mcVariant, mcSize, mcTask, mcCached];

        Console.WriteLine(TableHr(mc, "top"));
        Console.WriteLine(TableRow(mc, ["#", "Alias", "Variant", "Size (GB)", "Task", "Cached"]));
        Console.WriteLine(TableHr(mc, "mid"));

        int num = 1;
        int prevModelIdx = -1;
        foreach (var r in rows)
        {
            if (r.ModelIdx != prevModelIdx && prevModelIdx >= 0)
                Console.WriteLine(TableHr(mc, "mid"));
            prevModelIdx = r.ModelIdx;

            bool isFirst = r.VariantIdx == 0;
            string dot = r.IsCached ? "\x1b[32m●\x1b[0m" : "\x1b[31m●\x1b[0m";
            Console.WriteLine(TableRow(mc, [
                num.ToString(),
                isFirst ? r.Alias : "",
                r.VariantId,
                isFirst ? r.SizeGb : "",
                isFirst ? r.Task : "",
                dot,
            ]));
            num++;
        }
        Console.WriteLine(TableHr(mc, "bot"));
        return rows.ToList();
    }

    // ── Download Progress ────────────────────────────────────────────────

    public static (Action<float> onProgress, Action finalize) CreateDownloadBar(string modelAlias)
    {
        int col1 = Math.Max(5, modelAlias.Length);
        int col2 = BarWidth + 7;
        int[] w = [col1, col2];
        string Fmt(string n, string c) => TableRow(w, [n, c]);

        Console.WriteLine(TableHr(w, "top"));
        Console.WriteLine(Fmt("Model", "Progress"));
        Console.WriteLine(TableHr(w, "mid"));
        Console.WriteLine(Fmt(modelAlias, ProgressBar(0) + "  0.0%"));
        Console.WriteLine(TableHr(w, "bot"));

        void OnProgress(float percent)
        {
            Console.Write("\x1b[2A\r");
            Console.Write(Fmt(modelAlias, ProgressBar(percent) + $" {percent,5:F1}%"));
            Console.Write("\x1b[K\n\x1b[1B");
        }

        void Finalize()
        {
            Console.Write("\x1b[2A\r");
            Console.Write(Fmt(modelAlias, $"\x1b[32m{new string('█', BarWidth)} done \x1b[0m"));
            Console.Write("\x1b[K\n\x1b[1B");
        }

        return (OnProgress, Finalize);
    }

    // ── Chat / Audio Streaming ───────────────────────────────────────────

    public static void PrintUserMsg(string text)
    {
        int cols = Console.BufferWidth > 0 ? Console.BufferWidth : 80;
        var lines = WrapText(text, Math.Min(cols - 8, 60));
        foreach (var line in lines) Console.WriteLine($"  {line}");
        Console.WriteLine();
    }

    public sealed class StreamBox
    {
        private readonly int _boxWidth;
        private string _currentLine = "";
        private string _wordBuf = "";

        public StreamBox()
        {
            int cols = Console.BufferWidth > 0 ? Console.BufferWidth : 80;
            _boxWidth = Math.Min(cols - 8, 60);
            Console.WriteLine($"  ┌{new string('─', _boxWidth + 2)}┐");
            Console.Write(DrawRow("", true));
        }

        private string DrawRow(string text, bool cursor = false)
        {
            var display = cursor ? text + "▍" : text;
            return $"  │ {display.PadRight(_boxWidth)} │";
        }

        private void FlushLine()
        {
            Console.Write($"\r{DrawRow(_currentLine)}\x1b[K\n");
            _currentLine = "";
        }

        private void PushWord(string word)
        {
            if (_currentLine.Length == 0) _currentLine = word;
            else if (_currentLine.Length + 1 + word.Length <= _boxWidth) _currentLine += " " + word;
            else { FlushLine(); _currentLine = word; }

            while (_currentLine.Length > _boxWidth)
            {
                Console.Write($"\r{DrawRow(_currentLine[.._boxWidth])}\x1b[K\n");
                _currentLine = _currentLine[_boxWidth..];
            }
        }

        public void Write(char c)
        {
            if (c == '\n')
            {
                if (_wordBuf.Length > 0) { PushWord(_wordBuf); _wordBuf = ""; }
                FlushLine();
                Console.Write($"\r{DrawRow("", true)}\x1b[K");
            }
            else if (c == ' ')
            {
                if (_wordBuf.Length > 0) { PushWord(_wordBuf); _wordBuf = ""; }
                Console.Write($"\r{DrawRow(_currentLine, true)}\x1b[K");
            }
            else
            {
                _wordBuf += c;
                var preview = _currentLine.Length > 0
                    ? _currentLine + " " + _wordBuf
                    : _wordBuf;
                Console.Write($"\r{DrawRow(preview, true)}\x1b[K");
            }
        }

        public void Finish()
        {
            if (_wordBuf.Length > 0) { PushWord(_wordBuf); _wordBuf = ""; }
            Console.Write($"\r{DrawRow(_currentLine)}\x1b[K\n");
            Console.WriteLine($"  └{new string('─', _boxWidth + 2)}┘\n");
        }
    }

    public static string? AskUser(string prompt = "  \x1b[36m> \x1b[0m")
    {
        Console.Write(prompt);
        return Console.ReadLine();
    }
}
