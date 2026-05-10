// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using System.Runtime.InteropServices;

using Microsoft.AI.Foundry.Local.Detail.Interop;
using Microsoft.AI.Foundry.Local.Detail.Native;

namespace Microsoft.AI.Foundry.Local;

public class TextItem : Item
{
    public string Text { get; }

    public TextItemType Type { get; }

    public TextItem(string text) : this(text, TextItemType.Default)
    {
    }

    public TextItem(string text, TextItemType type) : base(ItemType.Text)
    {
        Text = text;
        Type = type;
        SetNative(text, type);
    }

    internal TextItem(IntPtr ptr, bool ownsHandle) : base(ptr, ownsHandle)
    {
        var data = new FlTextData { Version = NativeMethods.ApiVersion };
        Api.CheckStatus(Api.Item.GetText(Ptr, out data));
        Text = Api.Utf8(data.Text) ?? string.Empty;
        Type = (TextItemType)data.Type;
    }

    private void SetNative(string text, TextItemType type)
    {
        var textNative = Marshal.StringToCoTaskMemUTF8(text);

        try
        {
            var data = new FlTextData
            {
                Version = NativeMethods.ApiVersion,
                Text = textNative,
                Type = (FlTextItemType)type,
            };
            Api.CheckStatus(Api.Item.SetText(Ptr, ref data));
        }
        finally
        {
            Marshal.FreeCoTaskMem(textNative);
        }
    }
}
