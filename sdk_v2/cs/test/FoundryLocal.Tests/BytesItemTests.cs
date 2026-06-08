// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System;
using System.Threading.Tasks;

#pragma warning disable CA2000 // Items are transferred to Queue via Push

/// <summary>
/// Tests for BytesItem lifetime management and data ownership.
/// These exercise the four ownership states:
///   1. Read-only borrowed (constructor with ReadOnlyMemory)
///   2. Mutable borrowed (constructor with Memory)
///   3. Read-only owned (CreateOwned with ReadOnlyMemory)
///   4. Mutable owned (CreateOwned with Memory)
/// </summary>
internal sealed class BytesItemTests
{
    // -- Borrowed read-only --------------------------------------------------

    [Test]
    public async Task Borrowed_ReadOnly_DataRoundtrips()
    {
        byte[] source = [1, 2, 3, 4, 5];

        using var item = new BytesItem((ReadOnlyMemory<byte>)source);

        await Assert.That(item.ItemType).IsEqualTo(ItemType.Bytes);
        await Assert.That(item.Data.Length).IsEqualTo(5);
        await Assert.That(item.Data.ToArray()).IsEquivalentTo(source);
    }

    [Test]
    public async Task Borrowed_ReadOnly_ByteArray_ImplicitConversion()
    {
        // byte[] converts implicitly to ReadOnlyMemory<byte>
        byte[] source = [10, 20, 30];

        using var item = new BytesItem(source);

        await Assert.That(item.Data.Length).IsEqualTo(3);
        await Assert.That(item.Data[0]).IsEqualTo((byte)10);
        await Assert.That(item.Data[2]).IsEqualTo((byte)30);
    }

    [Test]
    public async Task Borrowed_ReadOnly_EmptyData()
    {
        using var item = new BytesItem(ReadOnlyMemory<byte>.Empty);

        await Assert.That(item.Data.Length).IsEqualTo(0);
        await Assert.That(item.Data.IsEmpty).IsTrue();
    }

    [Test]
    public async Task Borrowed_ReadOnly_LargeBuffer()
    {
        var source = new byte[64 * 1024]; // 64 KB
        new Random(42).NextBytes(source);

        using var item = new BytesItem((ReadOnlyMemory<byte>)source);

        await Assert.That(item.Data.Length).IsEqualTo(source.Length);
        await Assert.That(item.Data[0]).IsEqualTo(source[0]);
        await Assert.That(item.Data[^1]).IsEqualTo(source[^1]);
    }

    // -- Borrowed mutable ----------------------------------------------------

    [Test]
    public async Task Borrowed_Mutable_DataRoundtrips()
    {
        byte[] source = [100, 101, 102];

        using var item = new BytesItem((Memory<byte>)source);

        await Assert.That(item.Data.Length).IsEqualTo(3);
        await Assert.That(item.Data.ToArray()).IsEquivalentTo(source);
    }

    // -- Owned via ItemQueue -------------------------------------------------

    [Test]
    public async Task Owned_ReadOnly_SurvivesQueuePushPop()
    {
        byte[] source = [7, 8, 9, 10];

        using var queue = new ItemQueue();

        // CreateOwned — the pin survives past the C# wrapper's dispose
        var item = BytesItem.CreateOwned((ReadOnlyMemory<byte>)source);
        queue.Push(item);
        // item's C# wrapper released ownership — do not use item.Data after this

        await Assert.That(queue.Count).IsEqualTo(1);

        // Pop returns a new wrapper around the native item
        using var popped = queue.TryPop();

        await Assert.That(popped).IsNotNull();
        await Assert.That(popped).IsTypeOf<BytesItem>();

        var poppedBytes = (BytesItem)popped!;
        await Assert.That(poppedBytes.Data.Length).IsEqualTo(4);
        await Assert.That(poppedBytes.Data.ToArray()).IsEquivalentTo(source);
    }

    [Test]
    public async Task Owned_Mutable_SurvivesQueuePushPop()
    {
        byte[] source = [11, 22, 33];

        using var queue = new ItemQueue();

        var item = BytesItem.CreateOwned((Memory<byte>)source);
        queue.Push(item);

        using var popped = queue.TryPop();

        await Assert.That(popped).IsNotNull();

        var poppedBytes = (BytesItem)popped!;
        await Assert.That(poppedBytes.Data.Length).IsEqualTo(3);
        await Assert.That(poppedBytes.Data[0]).IsEqualTo((byte)11);
        await Assert.That(poppedBytes.Data[2]).IsEqualTo((byte)33);
    }

    [Test]
    public async Task Owned_MultipleItems_QueueRoundtrip()
    {
        using var queue = new ItemQueue();

        for (int i = 0; i < 10; i++)
        {
            var data = new byte[] { (byte)i, (byte)(i * 2) };
            var item = BytesItem.CreateOwned((ReadOnlyMemory<byte>)data);
            queue.Push(item);
        }

        await Assert.That(queue.Count).IsEqualTo(10);

        for (int i = 0; i < 10; i++)
        {
            using var popped = queue.TryPop();

            await Assert.That(popped).IsNotNull();

            var poppedBytes = (BytesItem)popped!;
            await Assert.That(poppedBytes.Data[0]).IsEqualTo((byte)i);
            await Assert.That(poppedBytes.Data[1]).IsEqualTo((byte)(i * 2));
        }

        await Assert.That(queue.TryPop()).IsNull();
    }

    // -- Queue lifecycle -----------------------------------------------------

    [Test]
    public async Task Queue_MarkFinished_Works()
    {
        using var queue = new ItemQueue();

        await Assert.That(queue.IsFinished).IsFalse();

        queue.MarkFinished();

        await Assert.That(queue.IsFinished).IsTrue();
    }

    [Test]
    public async Task Queue_TryPop_EmptyReturnsNull()
    {
        using var queue = new ItemQueue();

        var result = queue.TryPop();

        await Assert.That(result).IsNull();
    }

    // -- Dispose behavior ----------------------------------------------------

    [Test]
    public Task Borrowed_Dispose_DoesNotCrash()
    {
        byte[] source = [1, 2, 3];
        var item = new BytesItem((ReadOnlyMemory<byte>)source);

        // Dispose should unpin and release native handle without error
        item.Dispose();

        // Double dispose should be safe
        item.Dispose();

        // If the deleter crashed, we wouldn't reach here
        return Task.CompletedTask;
    }

    [Test]
    public async Task Owned_QueueDestruction_ReleasesPin()
    {
        // When the queue is disposed, all items in it are destroyed by the native layer.
        // This should trigger our deleter which unpins the memory.
        byte[] source = [42, 43, 44];

        var queue = new ItemQueue();
        var item = BytesItem.CreateOwned((ReadOnlyMemory<byte>)source);
        queue.Push(item);

        await Assert.That(queue.Count).IsEqualTo(1);

        // Disposing the queue destroys the native items, which calls our deleter
        queue.Dispose();

        // If the deleter crashed, we wouldn't reach here
    }

    // -- FromNative factory --------------------------------------------------

    [Test]
    public async Task FromNative_ReturnsBytesItem()
    {
        byte[] source = [5, 6, 7];

        using var queue = new ItemQueue();

        var item = BytesItem.CreateOwned((ReadOnlyMemory<byte>)source);
        queue.Push(item);

        // TryPop uses Item.FromNative — verify it returns BytesItem, not base Item
        using var popped = queue.TryPop();

        await Assert.That(popped).IsNotNull();
        await Assert.That(popped).IsTypeOf<BytesItem>();
        await Assert.That(popped!.ItemType).IsEqualTo(ItemType.Bytes);
    }

    // -- Pattern matching (ergonomics) ---------------------------------------

    [Test]
    public async Task PatternMatch_BytesItem()
    {
        byte[] source = [99];

        using var queue = new ItemQueue();

        var item = BytesItem.CreateOwned((ReadOnlyMemory<byte>)source);
        queue.Push(item);

        using var popped = queue.TryPop();
        byte? value = null;

        if (popped is BytesItem bytes)
        {
            value = bytes.Data[0];
        }

        await Assert.That(value).IsEqualTo((byte)99);
    }
}
