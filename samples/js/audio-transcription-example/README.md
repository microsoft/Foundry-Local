# Audio transcription example

This sample demonstrates how to use the audio transcription capabilities of the Foundry Local SDK with a local model. It initializes the SDK, selects an audio transcription model, and sends an audio file for transcription.

## Prerequisites
- Ensure you have Node.js installed (version 20 or higher is recommended).

## Setup project

Navigate to the sample directory, setup the project, and install the Foundry Local SDK package.

### Windows

1. Navigate to the sample directory and setup the project:
    ```bash
    cd samples/js/audio-transcription-example
    npm init -y
    npm pkg set type=module
    ```
1. Install the Foundry Local package:
    ```bash
    npm install --winml foundry-local-sdk
    ```
    
> [!NOTE]
> The `--winml` flag installs the Windows-specific package that uses Windows Machine Learning (WinML) for hardware acceleration on compatible devices.    

### MacOS and Linux

1. Navigate to the sample directory and set up the project:
    ```bash
    cd samples/js/audio-transcription-example
    npm init -y
    npm pkg set type=module
    ```
1. Install the Foundry Local package:
    ```bash
    npm install foundry-local-sdk
    ```

## Run the sample

Run the sample script using Node.js:

```bash
cd samples/js/audio-transcription-example
node app.js
```