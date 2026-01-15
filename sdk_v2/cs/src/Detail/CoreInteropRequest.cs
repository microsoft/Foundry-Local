// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Detail;
using System.Collections.Generic;
using System.Text.Json;

public class CoreInteropRequest
{
    public Dictionary<string, string> Params { get; set; } = new();
}

internal static class RequestExtensions
{
    public static string ToJson(this CoreInteropRequest request)
    {
        return JsonSerializer.Serialize(request, JsonSerializationContext.Default.CoreInteropRequest);
    }
}
