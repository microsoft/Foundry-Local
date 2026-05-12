// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

// WinML build variant: injects Bootstrap parameter for Windows App Runtime initialization.

#if IS_WINML

namespace Microsoft.AI.Foundry.Local.Detail;

internal partial class CoreInterop
{
    partial void PrepareWinMLBootstrap(CoreInteropRequest request)
    {
        if (!request.Params.ContainsKey("Bootstrap"))
        {
            request.Params["Bootstrap"] = "true";
        }
    }
}

#endif
