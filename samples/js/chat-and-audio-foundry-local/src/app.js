// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import { FoundryLocalManager } from "foundry-local-sdk";
import path from "path";

// Model aliases
const CHAT_MODEL = "phi-3.5-mini";
const WHISPER_MODEL = "whisper-tiny";

async function main() {
  console.log("Initializing Foundry Local SDK...");
  const manager = FoundryLocalManager.create({
    appName: "foundry_local_samples",
    logLevel: "info",
  });

  const catalog = manager.catalog;

  // --- Load both models ---
  console.log("\n--- Loading models ---");

  const chatModel = await catalog.getModel(CHAT_MODEL);
  if (!chatModel) {
    throw new Error(
      `Chat model "${CHAT_MODEL}" not found. Run "foundry model list" to see available models.`
    );
  }

  const whisperModel = await catalog.getModel(WHISPER_MODEL);
  if (!whisperModel) {
    throw new Error(
      `Whisper model "${WHISPER_MODEL}" not found. Run "foundry model list" to see available models.`
    );
  }

  // Download models if not cached
  if (!chatModel.isCached) {
    console.log(`Downloading ${CHAT_MODEL}...`);
    await chatModel.download((progress) => {
      process.stdout.write(`\r  ${CHAT_MODEL}: ${progress.toFixed(1)}%`);
    });
    console.log();
  }

  if (!whisperModel.isCached) {
    console.log(`Downloading ${WHISPER_MODEL}...`);
    await whisperModel.download((progress) => {
      process.stdout.write(`\r  ${WHISPER_MODEL}: ${progress.toFixed(1)}%`);
    });
    console.log();
  }

  // Load both models into memory
  console.log(`Loading ${CHAT_MODEL}...`);
  await chatModel.load();
  console.log(`Loading ${WHISPER_MODEL}...`);
  await whisperModel.load();
  console.log("Both models loaded.\n");

  // --- Step 1: Transcribe audio ---
  console.log("=== Step 1: Audio Transcription ===");
  const audioClient = whisperModel.createAudioClient();
  audioClient.settings.language = "en";

  // Update this path to point to your audio file
  const audioFilePath = path.resolve("recording.mp3");
  const transcription = await audioClient.transcribe(audioFilePath);
  console.log("You said:", transcription.text);

  // --- Step 2: Analyze with chat model ---
  console.log("\n=== Step 2: AI Analysis ===");
  const chatClient = chatModel.createChatClient();
  chatClient.settings.temperature = 0.7;
  chatClient.settings.maxTokens = 500;

  // Summarize the transcription
  console.log("Generating summary...\n");
  for await (const chunk of chatClient.completeStreamingChat([
    {
      role: "system",
      content:
        "You are a helpful assistant. Summarize the following transcribed audio and extract key themes and action items.",
    },
    { role: "user", content: transcription.text },
  ])) {
    const content = chunk.choices?.[0]?.message?.content;
    if (content) {
      process.stdout.write(content);
    }
  }
  console.log("\n");

  // --- Clean up ---
  await chatModel.unload();
  await whisperModel.unload();
  console.log("Done.");
}

main().catch(console.error);
