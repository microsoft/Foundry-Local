// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import { FoundryLocalManager } from "foundry-local-sdk";
import path from "path";

// The Whisper model alias for audio transcription
const alias = "whisper-tiny";

async function main() {
  console.log("Initializing Foundry Local SDK...");
  const manager = FoundryLocalManager.create({
    appName: "AudioTranscriptionSample",
    logLevel: "info",
  });

  // Get the Whisper model from the catalog
  const catalog = manager.catalog;
  const model = await catalog.getModel(alias);
  if (!model) {
    throw new Error(
      `Model "${alias}" not found. Run "foundry model list" to see available models.`
    );
  }

  // Download the model if not already cached
  if (!model.isCached) {
    console.log(`Downloading model "${alias}"...`);
    await model.download((progress) => {
      process.stdout.write(`\rDownload progress: ${progress.toFixed(1)}%`);
    });
    console.log("\nDownload complete.");
  }

  // Load the model into memory
  console.log(`Loading model "${model.id}"...`);
  await model.load();
  console.log("Model loaded.\n");

  // Create an audio client for transcription
  const audioClient = model.createAudioClient();
  audioClient.settings.language = "en";

  // Update this path to point to your audio file
  const audioFilePath = path.resolve("recording.mp3");

  // --- Standard transcription ---
  console.log("=== Standard Transcription ===");
  const result = await audioClient.transcribe(audioFilePath);
  console.log("Transcription:", result.text);

  // --- Streaming transcription ---
  console.log("\n=== Streaming Transcription ===");
  await audioClient.transcribeStreaming(audioFilePath, (chunk) => {
    process.stdout.write(chunk.text);
  });
  console.log("\n");

  // Clean up
  await model.unload();
  console.log("Done.");
}

main().catch(console.error);
