// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import { CopilotClient, defineTool } from "@github/copilot-sdk";
import { FoundryLocalManager } from "foundry-local-sdk";
import { z } from "zod";
import * as os from "os";

const alias = "phi-4-mini";

// Timeout for each model turn (ms).  Override with FOUNDRY_TIMEOUT_MS env var.
// Local models on CPU can be slow — increase this on less powerful hardware.
const TIMEOUT_MS = Number(process.env.FOUNDRY_TIMEOUT_MS) || 120_000;

async function sendMessage(
    session: Awaited<ReturnType<CopilotClient["createSession"]>>,
    prompt: string,
    timeoutMs = TIMEOUT_MS,
) {
    return new Promise<void>((resolve) => {
        let settled = false;
        let turnStarted = false;
        const finish = () => {
            if (!settled) {
                settled = true;
                unsub();
                resolve();
            }
        };

        const unsub = session.on((event: any) => {
            if (event.type === "assistant.turn_start") turnStarted = true;
            if (turnStarted && event.type === "session.idle") finish();
            if (turnStarted && event.type === "session.error") finish();
        });

        session.send({ prompt }).catch(() => finish());
        setTimeout(finish, timeoutMs);
    });
}

async function main() {
    console.log("Initializing Foundry Local...");
    const manager = new FoundryLocalManager();
    const modelInfo = await manager.init(alias);
    console.log(`Model: ${modelInfo.id}`);
    console.log(`Endpoint: ${manager.endpoint}\n`);

    const client = new CopilotClient();

    const getSystemInfo = defineTool("get_system_info", {
        description:
            "Get information about the current system including OS, architecture, memory, and CPU count",
        parameters: z.object({}),
        handler: async () => ({
            platform: os.platform(),
            arch: os.arch(),
            cpus: os.cpus().length,
            totalMemory: `${Math.round(os.totalmem() / (1024 ** 3))} GB`,
            freeMemory: `${Math.round(os.freemem() / (1024 ** 3))} GB`,
            nodeVersion: process.version,
            model: modelInfo.id,
            endpoint: manager.endpoint,
        }),
    });

    const session = await client.createSession({
        model: modelInfo.id,
        provider: {
            type: "openai",
            baseUrl: manager.endpoint,
            apiKey: manager.apiKey,
            wireApi: "completions",
        },
        streaming: true,
        tools: [getSystemInfo],
        systemMessage: {
            content:
                "You are a helpful AI assistant running locally via Foundry Local. " +
                "You have access to tools — use them when the user asks for system or runtime information.",
        },
    });

    session.on("assistant.message_delta", (event) => {
        process.stdout.write(event.data.deltaContent);
    });
    session.on("tool.execution_start", (event) => {
        console.log(`\n  [Tool called: ${(event as any).data?.toolName ?? "unknown"}]`);
    });

    console.log("--- Turn 1: Ask about the local AI setup ---\n");
    process.stdout.write("Assistant: ");
    await sendMessage(session, "What AI model am I running locally and what are its capabilities?");
    console.log("\n");

    console.log("--- Turn 2: Follow-up conversation ---\n");
    process.stdout.write("Assistant: ");
    await sendMessage(session, "What is the golden ratio? Explain in one paragraph.");
    console.log("\n");

    await session.destroy();
    await client.stop();
    console.log("Done!");
}

main().catch(console.error);
