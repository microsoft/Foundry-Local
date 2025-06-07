# Foundry Local .NET Samples

This folder contains sample projects demonstrating how to use [Foundry Local](https://github.com/microsoft/foundry-local) with .NET.

## Projects in this Solution

- **FoundryLocal-01-MEAI-Chat**
  - Demonstrates chat completion using Foundry Local and the MEAI SDK.
  - Project file: `FoundryLocal-01-MEAI-Chat/FoundryLocal-02-MEAI-Chat.csproj`

- **FoundryLocal-01-SK-Chat**
  - Demonstrates chat completion using Foundry Local and Semantic Kernel.
  - Project file: `FoundryLocal-01-SK-Chat/FoundryLocal-01-SK-Chat.csproj`

## Prerequisites

To run these samples, you need:

- [.NET 8 SDK or higher](https://dotnet.microsoft.com/en-us/download/dotnet)
- **Foundry Local** installed and running on your machine
- A downloaded local model, such as Phi-3.5

### Installing Foundry Local

Follow the [Foundry Local installation guide](../../README.md#installing) to set up Foundry Local on your machine. After installation, download a supported model (e.g., phi-3.5) and configure Foundry Local to use it.

## How to Run the Samples

1. Ensure Foundry Local is running and a model is loaded (e.g., phi-3.5).

1. Open one of the sample project's folder in your terminal.

1. Run a sample project. For example, to run the MEAI Chat sample:

   ```bash
   dotnet run --project FoundryLocal-01-MEAI-Chat/FoundryLocal-02-MEAI-Chat.csproj
   ```

   Or to run the Semantic Kernel Chat sample:

   ```bash
   dotnet run --project FoundryLocal-01-SK-Chat/FoundryLocal-01-SK-Chat.csproj
   ```

## References

- [Sample code using Foundry Local with .NET](https://aka.ms/genainet)
- [Foundry Local GitHub](https://github.com/microsoft/foundry-local)