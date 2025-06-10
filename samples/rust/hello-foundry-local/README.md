# Hello Foundry Local (Rust)

A simple example that demonstrates using the Foundry Local Rust SDK to interact with AI models locally.

## Prerequisites

- Rust 1.70.0 or later
- Foundry Local installed and available on PATH

## Running the Sample

1. Make sure Foundry Local is installed
2. Run the sample:

```bash
cargo run
```

## What This Sample Does

1. Creates a FoundryLocalManager instance
2. Starts the Foundry Local service if it's not already running
3. Downloads and loads the phi-3-mini-4k model
4. Sends a prompt to the model using the OpenAI-compatible API
5. Displays the response from the model

## Code Structure

- `src/main.rs` - The main application code
- `Cargo.toml` - Project configuration and dependencies 