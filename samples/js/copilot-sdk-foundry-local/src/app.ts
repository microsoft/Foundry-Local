// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import { CopilotClient, defineTool } from "@github/copilot-sdk";
import { FoundryLocalManager } from "foundry-local-sdk";

const alias = "gpt-oss-20b";

async function main() {
    // Step 1: Bootstrap Foundry Local
    // This starts the service if not running and downloads/loads the model
    console.log("Initializing Foundry Local...");
    const manager = new FoundryLocalManager();
    const modelInfo = await manager.init(alias);
    console.log(`Model: ${modelInfo.id}`);
    console.log(`Endpoint: ${manager.endpoint}`);
    console.log("");

    // Step 2: Create a Copilot SDK client
    // The SDK communicates with the Copilot CLI over JSON-RPC
    const client = new CopilotClient();

    // Step 3: Define a custom tool the model can call
    // This demonstrates agentic capabilities beyond simple chat completions
    const getSystemInfo = defineTool("get_system_info", {
        description: "Get information about the local AI system",
        parameters: {
            type: "object",
            properties: {
                query: {
                    type: "string",
                    description:
                        "What system information to retrieve: 'model', 'endpoint', or 'capabilities'",
                },
            },
            required: ["query"],
        },
        handler: async (args: { query: string }) => {
            switch (args.query) {
                case "model":
                    return {
                        modelId: modelInfo.id,
                        alias,
                        runtime: "ONNX Runtime (Foundry Local)",
                    };
                case "endpoint":
                    return {
                        url: manager.endpoint,
                        protocol: "OpenAI-compatible",
                        local: true,
                    };
                case "capabilities":
                    return {
                        chat: true,
                        streaming: true,
                        localInference: true,
                        noCloudRequired: true,
                    };
                default:
                    return { error: `Unknown query: ${args.query}` };
            }
        },
    });

    // Step 4: Create a session with BYOK pointing to Foundry Local
    // The provider config tells Copilot SDK to use Foundry Local's
    // OpenAI-compatible endpoint instead of GitHub Copilot's cloud service
    const session = await client.createSession({
        model: modelInfo.id,
        provider: {
            type: "openai",
            baseUrl: manager.endpoint, // e.g., "http://localhost:5272/v1"
            apiKey: manager.apiKey,
            wireApi: "completions", // Foundry Local uses Chat Completions API
        },
        streaming: true,
        tools: [getSystemInfo],
    });

    // Step 5: Subscribe to streaming events
    session.on("assistant.message_delta", (event) => {
        process.stdout.write(event.data.deltaContent);
    });

    // Step 6: Send a prompt that exercises tool calling
    console.log("Asking Copilot SDK about the local AI system...\n");
    process.stdout.write("Assistant: ");
    await session.sendAndWait({
        prompt:
            "What model am I running locally? Use the get_system_info tool to find out, then summarize the local AI setup.",
    });
    console.log("\n");

    // Step 7: Send a follow-up chat message (multi-turn conversation)
    console.log("Sending a follow-up question...\n");
    process.stdout.write("Assistant: ");
    await session.sendAndWait({
        prompt: "Explain the golden ratio in one paragraph.",
    });
    console.log("\n");

    // Step 8: Clean up
    await session.destroy();
    await client.stop();
    console.log("Done!");
}

main().catch(console.error);
