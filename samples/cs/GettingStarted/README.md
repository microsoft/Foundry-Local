# 🚀 Getting started with the Foundry Local SDK

There are two NuGet packages for the Foundry Local SDK - a WinML and a cross-platform package - that have *exactly* the same API surface but are optimised for different platforms: 

- **Windows**: Uses the `Microsoft.AI.Foundry.Local.WinML` package that is specific to Windows applications. The WinML package uses Windows Machine Learning to deliver optimal performance and user experience on Windows devices.
- **Cross-Platform**: Use the `Microsoft.AI.Foundry.Local` package that can be used for cross-platform applications (Windows, Linux, macOS).

> [!TIP]
> Whilst you can use either package on Windows, we recommend using the WinML package for Windows applications to take advantage of the Windows ML framework for optimal performance and user experience. Your end users will benefit with:
> - a wider range of hardware acceleration options that are automatically managed by Windows ML. 
> - a smaller application package size because downloading hardware-specific libraries occurs at application runtime rather than bundled with your application.

Both the WinML and cross-platform packages provide the same APIs, so you can easily switch between the two packages if you need to target multiple platforms. The samples include the following projects:

- **HelloFoundryLocalSdk**: A simple console application that initializes the Foundry Local SDK and performs basic operations.
- **FoundryLocalWebServer**: A simple console application that shows how to set up a local OpenAI-compliant web server using the Foundry Local SDK.
- **AudioTranscriptionExample**: A simple console application that demonstrates how to use the Foundry Local SDK for audio transcription tasks.

## Running the samples

1. Clone the Foundry Local repository from GitHub.
   ```bash
    git clone https://github.com/microsoft/Foundry-Local.git
    ```
2. Open and run the samples.
        
    For Windows:
    1. Open the `Foundry-Local/samples/cs/GettingStarted/windows/FoundrySamplesWinML.sln` solution in Visual Studio or your preferred IDE.
    1. If you're using Visual Studio, run any of the sample projects (e.g., `HelloFoundryLocalSdk`, `FoundryLocalWebServer`, or `AudioTranscriptionExample`) by selecting the project in the Solution Explorer and clicking the "Start" button (or pressing F5).
    
        Alternatively, you can run the projects using the .NET CLI, for x64:
        ```bash
        cd Foundry-Local/samples/cs/GettingStarted/windows
        dotnet run --project <ProjectName>/<ProjectName>.csproj -r:win-x6
        ```
        or for ARM64:
        ```bash
        ```bash
        cd Foundry-Local/samples/cs/GettingStarted/windows
        dotnet run --project <ProjectName>/<ProjectName>.csproj -r:win-arm64
        ```
   
   
   For macOS or Linux:
   - Open the `Foundry-Local/samples/cs/GettingStarted/cross-platform/FoundrySamplesXPlatform.sln` solution in Visual Studio Code or your preferred IDE.
   - Run the project using the .NET CLI (update the `<ProjectName>` and `<runtime-identifier>` as needed):
        ```bash
        cd Foundry-Local/samples/cs/GettingStarted/cross-platform
        dotnet run --project <ProjectName>/<ProjectName>.csproj -r:<runtime-identifier>
        ```
     For example, to run the `HelloFoundryLocalSdk` project on macOS (Apple Silicon), use the following command:
        ```bash
        cd Foundry-Local/samples/cs/GettingStarted/cross-platform
        dotnet run --project HelloFoundryLocalSdk/HelloFoundryLocalSdk.csproj -r:osx-arm64
        ```


