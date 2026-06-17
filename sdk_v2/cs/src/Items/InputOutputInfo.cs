// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using System.Collections.Generic;

namespace Microsoft.AI.Foundry.Local;

public sealed class InputOutputInfo
{
    public IReadOnlyList<Item> Inputs { get; }
    public IReadOnlyList<Item> Outputs { get; }

    internal InputOutputInfo(List<Item> inputs, List<Item> outputs)
    {
        Inputs = inputs;
        Outputs = outputs;
    }
}
