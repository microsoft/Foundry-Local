// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import { FoundryLocalManager } from "foundry-local-sdk";
import { OpenAI } from "openai";

const alias = "phi-3.5-mini";

async function main() {
    // Step 1: Bootstrap Foundry Local
    // This starts the service if not running and downloads/loads the model
    console.log("Initializing Foundry Local...");
    const manager = new FoundryLocalManager();
    const modelInfo = await manager.init(alias);
    console.log(`Model: ${modelInfo.id}`);
    console.log(`Endpoint: ${manager.endpoint}`);
    console.log("");

    // Step 2: Create an OpenAI-compatible client pointing to Foundry Local
    // This is the same pattern Copilot SDK uses internally with BYOK type "openai"
    const client = new OpenAI({
        baseURL: manager.endpoint,
        apiKey: manager.apiKey,
    });

    // Step 3: Send a chat completion request
    console.log("Sending prompt to local model...\n");
    const stream = await client.chat.completions.create({
        model: modelInfo.id,
        messages: [
            {
                role: "system",
                content: "You are a helpful assistant running locally via Foundry Local.",
            },
            {
                role: "user",
                content: "Explain the golden ratio in one paragraph.",
            },
        ],
        stream: true,
    });

    // Step 4: Stream the response
    process.stdout.write("Assistant: ");
    for await (const chunk of stream) {
        const content = chunk.choices[0]?.delta?.content;
        if (content) {
            process.stdout.write(content);
        }
    }
    console.log("\n\nDone!");
}

main().catch(console.error);
