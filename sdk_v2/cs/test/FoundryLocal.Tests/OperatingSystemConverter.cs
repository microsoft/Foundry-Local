// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

public static class OperatingSystemConverter
{
    public static string ToJson(string s)
    {
        if (!OperatingSystem.IsWindows())
        {
            s = s.Replace("\r\n", "\n");
        }
        return s;
    }
}
