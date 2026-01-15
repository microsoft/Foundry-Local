// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;
using System;
using System.Diagnostics;

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
        Debug.Assert(logger != null);
        logger.LogError(message);
    }

    internal FoundryLocalException(string message, Exception innerException, ILogger logger)
        : base(message, innerException)
    {
        Debug.Assert(logger != null);
        logger.LogError(innerException, message);
    }
}
