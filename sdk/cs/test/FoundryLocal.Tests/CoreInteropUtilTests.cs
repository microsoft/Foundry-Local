// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System;
using System.Reflection;
using System.Runtime.InteropServices;

using Microsoft.AI.Foundry.Local.Detail;

internal sealed class CoreInteropUtilTests
{
    private static readonly Type CoreInteropType = typeof(CoreInterop);

    [Test]
    public async Task AddLibraryExtension_ReturnsCorrectExtensionForCurrentOS()
    {
        var method = CoreInteropType.GetMethod(
            "AddLibraryExtension",
            BindingFlags.NonPublic | BindingFlags.Static);

        await Assert.That(method).IsNotNull();

        var result = (string)method!.Invoke(null, new object[] { "TestLib" })!;

        if (Utils.IsWindows)
        {
            await Assert.That(result).IsEqualTo("TestLib.dll");
        }
        else if (Utils.IsLinux)
        {
            await Assert.That(result).IsEqualTo("TestLib.so");
        }
        else if (Utils.IsMacOS)
        {
            await Assert.That(result).IsEqualTo("TestLib.dylib");
        }
    }

    [Test]
    public async Task PtrToStringUtf8_RoundTripsUtf8String()
    {
        var method = CoreInteropType.GetMethod(
            "PtrToStringUtf8",
            BindingFlags.NonPublic | BindingFlags.Static);

        await Assert.That(method).IsNotNull();

        var testString = "Hello, 世界! 🌍";
        var bytes = System.Text.Encoding.UTF8.GetBytes(testString);
        var ptr = Marshal.AllocHGlobal(bytes.Length);

        try
        {
            Marshal.Copy(bytes, 0, ptr, bytes.Length);
            var result = (string)method!.Invoke(null, new object[] { ptr, bytes.Length })!;

            await Assert.That(result).IsEqualTo(testString);
        }
        finally
        {
            Marshal.FreeHGlobal(ptr);
        }
    }

    [Test]
    public async Task PtrToStringUtf8_EmptyString_ReturnsEmpty()
    {
        var method = CoreInteropType.GetMethod(
            "PtrToStringUtf8",
            BindingFlags.NonPublic | BindingFlags.Static);

        await Assert.That(method).IsNotNull();

        var bytes = System.Text.Encoding.UTF8.GetBytes(string.Empty);
        var ptr = Marshal.AllocHGlobal(1); // allocate at least 1 byte

        try
        {
            var result = (string)method!.Invoke(null, new object[] { ptr, 0 })!;

            await Assert.That(result).IsEqualTo(string.Empty);
        }
        finally
        {
            Marshal.FreeHGlobal(ptr);
        }
    }

    [Test]
    public async Task CallbackHelper_NullCallback_ThrowsArgumentNullException()
    {
        await Assert.That(() => new CoreInterop.CallbackHelper(null!))
            .Throws<ArgumentNullException>();
    }

    [Test]
    public async Task CallbackHelper_ValidCallback_StoresCallback()
    {
        ICoreInterop.CallbackFn fn = _ => { };
        var helper = new CoreInterop.CallbackHelper(fn);

        await Assert.That(helper.Callback).IsSameReferenceAs(fn);
        await Assert.That(helper.Exception).IsNull();
    }

    [Test]
    public async Task CallbackHelper_Exception_CanBeSetAndRetrieved()
    {
        ICoreInterop.CallbackFn fn = _ => { };
        var helper = new CoreInterop.CallbackHelper(fn);
        var ex = new InvalidOperationException("test");

        helper.Exception = ex;

        await Assert.That(helper.Exception).IsSameReferenceAs(ex);
    }

    [Test]
    public async Task OsPlatformHelpers_AtLeastOneIsTrue()
    {
        var isWindows = (bool)CoreInteropType
            .GetField("IsWindows", BindingFlags.NonPublic | BindingFlags.Static)!
            .GetValue(null)!;
        var isLinux = (bool)CoreInteropType
            .GetField("IsLinux", BindingFlags.NonPublic | BindingFlags.Static)!
            .GetValue(null)!;
        var isMacOS = (bool)CoreInteropType
            .GetField("IsMacOS", BindingFlags.NonPublic | BindingFlags.Static)!
            .GetValue(null)!;

        await Assert.That(isWindows || isLinux || isMacOS).IsTrue();
    }

    [Test]
    public async Task OsPlatformHelpers_ConsistentWithExpectedValues()
    {
        var isWindows = (bool)CoreInteropType
            .GetField("IsWindows", BindingFlags.NonPublic | BindingFlags.Static)!
            .GetValue(null)!;
        var isLinux = (bool)CoreInteropType
            .GetField("IsLinux", BindingFlags.NonPublic | BindingFlags.Static)!
            .GetValue(null)!;
        var isMacOS = (bool)CoreInteropType
            .GetField("IsMacOS", BindingFlags.NonPublic | BindingFlags.Static)!
            .GetValue(null)!;

        await Assert.That(isWindows).IsEqualTo(Utils.IsWindows);
        await Assert.That(isLinux).IsEqualTo(Utils.IsLinux);
        await Assert.That(isMacOS).IsEqualTo(Utils.IsMacOS);
    }

    [Test]
    public async Task LibraryName_IsExpectedValue()
    {
        var name = CoreInterop.LibraryName;
        await Assert.That(name).IsEqualTo("Microsoft.AI.Foundry.Local.Core");
    }
}
