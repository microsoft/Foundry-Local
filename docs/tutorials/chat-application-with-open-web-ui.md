# Build a chat application with Open Web UI

This tutorial shows you how to create a chat application using Foundry Local and Open Web UI. When you finish, you'll have a working chat interface running entirely on your local device.

## Prerequisites

Before you start this tutorial, you need:

- **Foundry Local** [installed](../get-started.md) on your computer.
- **At least one model loaded** with the `foundry model load` command, like this:
  ```bash
  foundry model load Phi-4-mini-gpu-int4-rtn-block-32
  ```

## Set up Open Web UI for chat

1. **Install Open Web UI** by following the instructions from the [Open Web UI GitHub repository](https://github.com/open-webui/open-webui).

2. **Launch Open Web UI** with this command in your terminal:

   ```bash
   open-webui serve
   ```

3. Open your web browser and go to [http://localhost:8080](http://localhost:8080).

4. **Connect Open Web UI to Foundry Local**:

   1. Select **Settings** in the navigation menu
   2. Select **Connections**
   3. Select **Manage Direct Connections**
   4. Select the **+** icon to add a connection
   5. Enter `http://localhost:5272/v1` for the URL
   6. Type any value (like `test`) for the API Key, since it cannot be empty
   7. Save your connection

![image](https://github.com/user-attachments/assets/82437726-2b80-442a-b9bc-df46eb7f3d77)

5. **Start chatting with your model**:
   1. Your loaded models will appear in the dropdown at the top
   2. Select any model from the list
   3. Type your message in the input box at the bottom

That's it! You're now chatting with an AI model running entirely on your local device.

## Next steps

- [Build an application with LangChain](use-langchain-with-foundry-local.md)
- [How to compile Hugging Face models to run on Foundry Local](../how-to/how-to-compile-hugging-face-models.md)
