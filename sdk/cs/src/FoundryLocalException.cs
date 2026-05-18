// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;
using System;

using Microsoft.Extensions.Logging;

public class FoundryLocalException : Exception
{
    public FoundryLocalException(string message) : base(message)
    {
    }

    public FoundryLocalException(string message, Exception innerException) : base(message, innerException)
    {
    }

    internal FoundryLocalException(string message, ILogger logger) : base(message)
    {
        logger.LogError(message);
    }

    internal FoundryLocalException(string message, Exception innerException, ILogger logger)
        : base(message, innerException)
    {
        logger.LogError(innerException, message);
    }
}

/// <summary>
/// Thrown when a private catalog operation fails authentication (bad/expired
/// token, wrong <c>aud</c>, missing <c>registry_name</c>, etc.).
/// </summary>
public class CatalogAuthException : FoundryLocalException
{
    public string Reason { get; }
    public CatalogAuthException(string message, string reason) : base(message) { Reason = reason; }
}
