# 🚀 Getting started with the Foundry Local SDK

There are two NuGet packages for the Foundry Local SDK - a WinML and a cross-platform package - that have *exactly* the same API surface but are optimised for different platforms: 

- **Windows**: Uses the `Microsoft.AI.Foundry.Local.WinML` package that is specific to Windows applications. The WinML package uses Windows Machine Learning to deliver optimal performance and user experience on Windows devices.
    - **Samples**: The [`FoundrySamplesWinML.sln`](windows/FoundrySamplesWinML.sln) solution provides sample projects that demonstrate how to use the Foundry Local SDK on Windows.
- **Cross-Platform**: Use the `Microsoft.AI.Foundry.Local` package that can be used for cross-platform applications (Windows, Linux, macOS).
    - **Samples**: The [`FoundrySamplesXPlatform.sln`](cross-platform/FoundrySamplesXPlatform.sln) solution provides sample projects that demonstrate how to use the Foundry Local SDK in cross-platform applications.

> [!TIP]
> Whilst you can use either package on Windows, we recommend using the WinML package for Windows applications to take advantage of the Windows ML framework for optimal performance and user experience. Your end users will benefit with:
> - a wider range of hardware acceleration options that are automatically managed by Windows ML. 
> - a smaller application package size because downloading hardware-specific libraries occurs at application runtime rather than bundled with your application.

Both the WinML and cross-platform packages provide the same APIs, so you can easily switch between the two packages if you need to target multiple platforms. The samples for *both* packages include the following projects:

- **HelloFoundryLocalSdk**: A simple console application that initializes the Foundry Local SDK and performs basic operations.
- **FoundryLocalWebServer**: A simple console application that shows how to set up a local OpenAI-compliant web server using the Foundry Local SDK.
- **AudioTranscriptionSample**: A simple console application that demonstrates how to use the Foundry Local SDK for audio transcription tasks.

## ⚙️ Windows: Project setup guide

For Windows applications, we recommend using the `Microsoft.AI.Foundry.Local.WinML` package to take advantage of the Windows ML framework for optimal performance and user experience.

Your `csproj` file should include the following settings:

- Set the `TargetFramework` to a Windows-specific target (e.g. `net8.0-windows10.0.26100`).
- Set the `WindowsAppSDKSelfContained` property to `true` to ensure that the Windows App SDK is bundled with your application.
- Include a `PackageReference` to the `Microsoft.AI.Foundry.Local.WinML` package.
- Optionally, include an `Import` statement to exclude superfluous ONNX Runtime and IHV libraries from your published application - For more details, read [Reduce application package size](#🤏-reduce-application-package-size). This step is recommended to reduce the size of your application package. In the samples, we include a [`ExcludeExtraLibs.props`](./ExcludeExtraLibs.props) file and the `Import` statement to demonstrate this. The `Import` statement should be conditioned to only apply during publish operations.

Here is an example `csproj` file for a Windows application using the Foundry Local WinML SDK:

```xml
<Project Sdk="Microsoft.NET.Sdk">
  
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <ImplicitUsings>enable</ImplicitUsings>
    <Nullable>enable</Nullable>
    <!-- For Windows use the following -->
    <TargetFramework>net8.0-windows10.0.26100</TargetFramework>
    <WindowsAppSDKSelfContained>true</WindowsAppSDKSelfContained>
    <Platforms>x64;ARM64</Platforms>
  </PropertyGroup>

  <!-- Use WinML package for local Foundry SDK on Windows -->
  <ItemGroup>
    <PackageReference Include="Microsoft.AI.Foundry.Local.WinML" />
  </ItemGroup>

  <!-- On Publish: Exclude superfluous ORT and IHV libs -->
  <Import Project="../../ExcludeExtraLibs.props" Condition="'$(PublishDir)' != ''" />

</Project>
```

## 🤏 Reduce application package size

The Foundry Local SDK will pull in `Microsoft.ML.OnnxRuntime.Foundry` NuGet package as a dependency. The `Microsoft.ML.OnnxRuntime.Foundry` package provides the *inference runtime bundle*, which is the set of libraries required to efficiently run inference on specific vendor hardware devices. The inference runtime bundle includes the following components:

- **ONNX Runtime library**: The core inference engine (`onnxruntime.dll`).
- **ONNX Runtime Execution Provider (EP) library**. A hardware-specific backend in ONNX Runtime that optimises and executes parts of a machine learning model a hardware accelerator. For example:
    - CUDA EP: `onnxruntime_providers_cuda.dll`
    - QNN EP: `onnxruntime_providers_qnn.dll`
- **Independent Hardware Vendor (IHV) libraries**. For example:
    - WebGPU: DirectX dependencies (`dxcompiler.dll`, `dxil.dll`)
    - QNN: Qualcomm QNN dependencies (`QnnSystem.dll`, etc.)

The following table summarises what EP and IHV libraries will be bundled with your application and what WinML will download/install at runtime:

![EP Bundle table](../../../media/ep-bundle.png)

In all platform/architecture, the CPU EPU is required. The WebGPU EP and IHV libraries are small in size (for example, WebGPU only adds ~7MB to your application package) and are required in Windows and MacOS. However, the CUDA and QNN EPs are large in size (for example, CUDA adds ~1GB to your application package) so we recommend *excluding* these EPs from your application package and allowing WinML to download/install them at runtime if the end user has compatible hardware.

> [!NOTE]
> This is the current state and we are working on removing the CUDA and QNN EPs from the `Microsoft.ML.OnnxRuntime.Foundry` package in future releases so that you do not need to remove them from your application package.

In the samples, we include a [`ExcludeExtraLibs.props`](./ExcludeExtraLibs.props) file and `Import` it into the project on publish that demonstrates how to exclude the CUDA and QNN EP and IHV libraries from your application package.

## Linux: CUDA dependencies

Whilst the CUDA EP will be pulled into your Linux application via `Microsoft.ML.OnnxRuntime.Foundry`, we do not include the IHV libraries. If you want to allow your end users with CUDA-enabled devices to benefit from higher performance, you will need *add* the following CUDA IHV libraries to your application:

- CUBLAS 12.8.4 ([download from NVIDIA Developer](https://developer.download.nvidia.com/compute/cuda/redist/libcublas/windows-x86_64/libcublas-windows-x86_64-12.8.4.1-archive.zip))
    - cublas64_12.dll
    - cublasLt64_12.dll
- CUDA RT 12.8.90 ([download from NVIDIA Developer](https://developer.download.nvidia.com/compute/cuda/redist/cuda_cudart/windows-x86_64/cuda_cudart-windows-x86_64-12.8.90-archive.zip))
    - cudart64_12.dll
- CUDNN 9.8.0 ([download from NVIDIA Developer](https://developer.download.nvidia.com/compute/cudnn/redist/cudnn/windows-x86_64/cudnn-windows-x86_64-9.8.0.87_cuda12-archive.zip))
    - cudnn_graph64_9.dll
    - cudnn_ops64_9.dll
    - cudnn64_9.dll
- CUDA FFT 11.3.3.83 ([download from NVIDIA Developer](https://developer.download.nvidia.com/compute/cuda/redist/libcufft/windows-x86_64/libcufft-windows-x86_64-11.3.3.83-archive.zip))
    - cufft64_11.dll

> [!WARNING]
> Adding the CUDA EP and IHV libraries to your application will increase the size of your application package by approximately 1GB.


