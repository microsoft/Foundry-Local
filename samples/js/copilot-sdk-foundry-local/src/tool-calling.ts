// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/**
 * Tool Calling Example — Copilot SDK + Foundry Local
 *
 * Demonstrates multiple custom tools that the model can invoke:
 *   - calculate: Evaluate math expressions
 *   - get_system_info: Return local system details
 *   - lookup_definition: Look up programming term definitions
 *
 * Run:  npm run tools
 */

import { CopilotClient, defineTool } from "@github/copilot-sdk";
import { FoundryLocalManager } from "foundry-local-sdk";
import { z } from "zod";
import * as os from "os";

const alias = "phi-4-mini";

// Timeout for each model turn (ms).  Override with FOUNDRY_TIMEOUT_MS env var.
// Local models on CPU can be slow — increase this on less powerful hardware.
const TIMEOUT_MS = Number(process.env.FOUNDRY_TIMEOUT_MS) || 120_000;

// ---------------------------------------------------------------------------
// Helper: send a message and wait for the assistant's full reply.
// Foundry Local streaming sometimes omits finish_reason, which causes a
// session.error that can break sendAndWait(). This helper gates on
// assistant.turn_start so stale events from previous turns are ignored.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Tool definitions
// ---------------------------------------------------------------------------

function defineCalculateTool() {
    return defineTool("calculate", {
        description:
            "Evaluate a math expression and return the numeric result. " +
            "Supports +, -, *, /, parentheses, and Math.* functions like Math.sqrt, Math.pow.",
        parameters: z.object({
            expression: z.string().describe('Math expression to evaluate, e.g. "2 + 2" or "Math.sqrt(144)"'),
        }),
        handler: async (args) => {
            try {
                // Only allow safe math characters and Math.* calls
                const sanitized = args.expression.replace(/[^0-9+\-*/().,%\s]|Math\.\w+/g, (m) =>
                    m.startsWith("Math.") ? m : "",
                );
                const result = new Function(`"use strict"; return (${sanitized})`)();
                console.log(`\n    → calculate("${args.expression}") = ${result}`);
                return { expression: args.expression, result: Number(result) };
            } catch {
                return { expression: args.expression, error: "Could not evaluate expression" };
            }
        },
    });
}

function defineLookupTool() {
    const glossary: Record<string, string> = {
        "byok": "Bring Your Own Key — a pattern where you supply your own API credentials to route requests to a custom endpoint instead of the default provider.",
        "onnx": "Open Neural Network Exchange — an open format for representing machine learning models, enabling interoperability between frameworks.",
        "rag": "Retrieval-Augmented Generation — a technique that combines a retrieval system with a generative model so responses are grounded in external documents.",
        "json-rpc": "JSON Remote Procedure Call — a lightweight protocol for calling methods on a remote server using JSON-encoded messages.",
        "streaming": "A technique where the server sends response tokens incrementally as they are generated, rather than waiting for the full response.",
    };

    return defineTool("lookup_definition", {
        description:
            "Look up the definition of a programming or AI term. " +
            "Available terms: " + Object.keys(glossary).join(", "),
        parameters: z.object({
            term: z.string().describe("The term to look up (case-insensitive)"),
        }),
        handler: async (args) => {
            const key = args.term.toLowerCase().trim();
            const definition = glossary[key];
            console.log(`\n    → lookup_definition("${args.term}") → ${definition ? "found" : "not found"}`);
            if (definition) {
                return { term: args.term, definition };
            }
            return { term: args.term, error: `Term not found. Available: ${Object.keys(glossary).join(", ")}` };
        },
    });
}

function defineSystemInfoTool(modelId: string, endpoint: string) {
    return defineTool("get_system_info", {
        description: "Get information about the local system: OS, architecture, memory, CPU count, and the running model.",
        parameters: z.object({}),
        handler: async () => {
            const info = {
                platform: os.platform(),
                arch: os.arch(),
                cpus: os.cpus().length,
                totalMemory: `${Math.round(os.totalmem() / 1024 ** 3)} GB`,
                freeMemory: `${Math.round(os.freemem() / 1024 ** 3)} GB`,
                nodeVersion: process.version,
                model: modelId,
                endpoint,
            };
            console.log(`\n    → get_system_info() → ${JSON.stringify(info)}`);
            return info;
        },
    });
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

async function main() {
    console.log("Initializing Foundry Local...");
    const manager = new FoundryLocalManager();
    const modelInfo = await manager.init(alias);
    console.log(`Model: ${modelInfo.id}`);
    console.log(`Endpoint: ${manager.endpoint}\n`);

    const calculate = defineCalculateTool();
    const lookupDefinition = defineLookupTool();
    const getSystemInfo = defineSystemInfoTool(modelInfo.id, manager.endpoint);

    const client = new CopilotClient();

    const session = await client.createSession({
        model: modelInfo.id,
        provider: {
            type: "openai",
            baseUrl: manager.endpoint,
            apiKey: manager.apiKey,
            wireApi: "completions",
        },
        streaming: true,
        tools: [calculate, lookupDefinition, getSystemInfo],
        systemMessage: {
            content:
                "You are a helpful AI assistant running locally via Foundry Local. " +
                "You have access to tools. ALWAYS use the appropriate tool when the user asks you to " +
                "calculate something, look up a term, or get system information. " +
                "Do not guess — call the tool and report its result.",
        },
    });

    // Stream assistant text to stdout
    session.on("assistant.message_delta", (event) => {
        process.stdout.write(event.data.deltaContent);
    });
    session.on("tool.execution_start", (event) => {
        console.log(`\n  [Tool called: ${(event as any).data?.toolName ?? "unknown"}]`);
    });

    // --- Turn 1: Calculator tool ---
    console.log("=== Turn 1: Calculator ===\n");
    process.stdout.write("User: What is the square root of 144 plus 8 times 3?\n\nAssistant: ");
    await sendMessage(
        session,
        "Use the calculate tool to compute: Math.sqrt(144) + 8 * 3",
    );
    console.log("\n");

    // --- Turn 2: Glossary lookup tool ---
    console.log("=== Turn 2: Glossary Lookup ===\n");
    process.stdout.write("User: What does BYOK mean? And what about RAG?\n\nAssistant: ");
    await sendMessage(
        session,
        "Use the lookup_definition tool to look up 'byok' and 'rag', then explain both.",
    );
    console.log("\n");

    // --- Turn 3: System info tool ---
    console.log("=== Turn 3: System Info ===\n");
    process.stdout.write("User: What system am I running on?\n\nAssistant: ");
    await sendMessage(
        session,
        "Use the get_system_info tool to check what system this is running on, then summarize.",
    );
    console.log("\n");

    await session.destroy();
    await client.stop();
    console.log("Done!");
}

main().catch(console.error);
