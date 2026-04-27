// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

using System.Runtime.InteropServices;

public static class OperatingSystemConverter
{
    public static string ToJson(string s)
    {
        if (!IsWindows())
        {
            s = s.Replace("\r\n", "\n");
        }
        return s;
    }

    private static bool IsWindows() =>
#if NET5_0_OR_GREATER
        OperatingSystem.IsWindows();
#else
        RuntimeInformation.IsOSPlatform(OSPlatform.Windows);
#endif
}
