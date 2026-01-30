# LangChain integration example

This sample demonstrates how to integrate the Foundry Local SDK with LangChain.js to create a simple application that uses local language models for text generation.

## Prerequisites
- Ensure you have Node.js installed (version 20 or higher is recommended).

## Setup project

Navigate to the sample directory, setup the project, and install the Foundry Local and LangChain packages.

### Windows

1. Navigate to the sample directory and setup the project:
    ```bash
    cd samples/js/langchain-integration-example
    npm init -y
    npm pkg set type=module
    ```
1. Install the Foundry Local and LangChain packages:
    ```bash
    npm install --winml foundry-local-sdk
    npm install @langchain/openai @langchain/core
    ```
    
> [!NOTE]
> The `--winml` flag installs the Windows-specific package that uses Windows Machine Learning (WinML) for hardware acceleration on compatible devices.    

### MacOS and Linux

1. Navigate to the sample directory and setup the project:
    ```bash
    cd samples/js/langchain-integration-example
    npm init -y
    npm pkg set type=module
    ```
1. Install the Foundry Local and LangChain packages:
    ```bash
    npm install foundry-local-sdk
    npm install @langchain/openai @langchain/core
    ```

## Run the sample

Run the sample script using Node.js:

```bash
cd samples/js/langchain-integration-example
node app.js
```