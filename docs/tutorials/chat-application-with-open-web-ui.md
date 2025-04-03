# Build a Chat application with Open Web UI

This tutorial guides you through setting up a chat application using AI Foundry Local and Open Web UI. By the end, you'll have a fully functional chat interface running locally on your device.

## Prerequisites

Before beginning this tutorial, make sure you have:

- **AI Foundry Local** [installed](../get-started.md) on your machine.
- **At least one model loaded** using the `foundry model load` command, for example:
  ```bash
  foundry model load Phi-4-mini-gpu-int4-rtn-block-32
  ```

## Set up Open Web UI for chat

1. **Install Open Web UI** by following the installation instructions from the [Open Web UI github](https://github.com/open-webui/open-webui).

2. **Start Open Web UI** by running the following command in your terminal:

   ```bash
   open-webui serve
   ```

   Then open your browser and navigate to [http://localhost:8080](http://localhost:8080).

3. **Connect Open Web UI to AI Foundry Local**:

   - Go to **Settings** in the navigation menu
   - Select **Connections**
   - Choose **Manage Direct Connections**
   - Click the **+** icon to add a new connection
   - For the URL, enter `http://localhost:5272/v1`
      - **⚠️** Presently, you must also add the model ids under the connection. Please ensure you've done so accurately (i.e. `deepseek-r1-1.5b-cpu`) 
   - For the API Key, it presently can't be blank, so you can enter any value (e.g. `test`)
   - Save the connection
     
![image](https://github.com/user-attachments/assets/82437726-2b80-442a-b9bc-df46eb7f3d77)

4. **Start chatting with your model**:
   - The model list should automatically populate at the top of the UI
   - Select one of your loaded models from the dropdown
   - Begin your chat in the input box at the bottom of the screen

That's it! You're now chatting with your AI model running completely locally on your device.

## Next steps

- Try [different models](../how-to/load-models.md) to compare performance and capabilities
