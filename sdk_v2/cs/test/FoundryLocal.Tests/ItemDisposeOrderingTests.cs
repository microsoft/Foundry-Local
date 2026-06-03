// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System;
using System.Threading.Tasks;

/// <summary>
/// Tests for Item dispose-ordering (M10) and constructor leak-safety (M11/M12).
/// These do not require a model — they exercise the Item base class dispose hook
/// and the construction-failure paths via subclassing.
/// </summary>
internal sealed class ItemDisposeOrderingTests
{
    private sealed class HookProbeItem : Item
    {
        public IntPtr PtrSeenInOnDisposing { get; private set; } = (IntPtr)(-1);
        public bool OnDisposingCalled { get; private set; }

        public HookProbeItem() : base(ItemType.Text)
        {
        }

        protected override void OnDisposing()
        {
            OnDisposingCalled = true;
            PtrSeenInOnDisposing = Ptr;
        }
    }

    [Test]
    public async Task OnDisposing_RunsBeforeNativeRelease_AndSeesValidPtr()
    {
        var probe = new HookProbeItem();
        var nativePtr = probe.Ptr;
        await Assert.That(nativePtr).IsNotEqualTo(IntPtr.Zero);

        probe.Dispose();

        await Assert.That(probe.OnDisposingCalled).IsTrue();
        await Assert.That(probe.PtrSeenInOnDisposing).IsEqualTo(nativePtr);
        await Assert.That(probe.Ptr).IsEqualTo(IntPtr.Zero);
    }

    [Test]
    public async Task Dispose_IsIdempotent()
    {
        var probe = new HookProbeItem();
        probe.Dispose();
        probe.Dispose();
        await Assert.That(probe.Ptr).IsEqualTo(IntPtr.Zero);
    }

    // M11/M12 happy-path sanity: ctor try/catch must not change normal construction.
    [Test]
    public async Task MessageItem_HappyPath_StillConstructs()
    {
        using var msg = MessageItem.User("hello world");
        await Assert.That(msg.Ptr).IsNotEqualTo(IntPtr.Zero);
        await Assert.That(msg.IsSimpleText()).IsTrue();
        await Assert.That(msg.GetSimpleText()).IsEqualTo("hello world");
    }

    [Test]
    public async Task MessageItem_EmptyContent_ThrowsAndReleasesNative()
    {
        // The empty-content guard runs inside the new try/catch; native handle must still
        // be released (not leaked) before the ArgumentException reaches the caller.
        await Assert.That(() => new MessageItem(MessageRole.User, string.Empty))
            .Throws<ArgumentException>();
    }

    [Test]
    public async Task MessageItem_NullPart_ThrowsAndReleasesNative()
    {
        Item?[] parts = [null];
        await Assert.That(() => new MessageItem(MessageRole.User, parts!))
            .Throws<ArgumentException>();
    }

    [Test]
    public async Task BytesItem_CreateOwned_ReadOnly_DefensiveCopyDecouplesSource()
    {
        // M13 (BytesItem analog): CreateOwned(ReadOnlyMemory) must defensively copy so
        // the native deleter doesn't end up writing to caller-owned read-only memory.
        byte[] source = [1, 2, 3, 4];
        using var item = BytesItem.CreateOwned((ReadOnlyMemory<byte>)source);

        // Mutating the caller's source after creation must not affect the item's snapshot.
        source[0] = 99;

        await Assert.That(item.Data.Length).IsEqualTo(4);
        await Assert.That(item.Data[0]).IsEqualTo((byte)1);
    }
}
