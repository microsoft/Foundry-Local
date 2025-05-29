//! # Foundry Local SDK
//!
//! A Rust SDK for interacting with the Microsoft Foundry Local service.
//! This SDK allows you to manage and use AI models locally on your device.
//!
//! ## Features
//! - Start and manage the Foundry Local service
//! - Download models from the Foundry catalog
//! - Load and unload models
//! - List available, cached, and loaded models
//! - Interact with loaded models using a simple API
//!
//! ## Example
//!
//! ```rust
//! use foundry_local::FoundryLocalManager;
//! use anyhow::Result;
//!
//! #[tokio::main]
//! async fn main() -> Result<()> {
//!     // Create a FoundryLocalManager instance for a model with default options
//!     let mut manager = FoundryLocalManager::builder()
//!         .alias_or_model_id("phi-4-mini")
//!         .build()
//!         .await?;
//!     
//!     // Use the OpenAI compatible API to interact with the model
//!     let client = reqwest::Client::new();
//!     let response = client.post(&format!("{}/chat/completions", manager.endpoint()?))
//!         .header("Content-Type", "application/json")
//!         .header("Authorization", format!("Bearer {}", manager.api_key()))
//!         .json(&serde_json::json!({
//!             "model": manager.get_model_info("phi-4-mini", true).await?.id,
//!             "messages": [{"role": "user", "content": "What is the golden ratio?"}],
//!         }))
//!         .send()
//!         .await?;
//!     
//!     let result = response.json::<serde_json::Value>().await?;
//!     println!("{}", result["choices"][0]["message"]["content"]);
//!     
//!     Ok(())
//! }
//! ```

pub mod api;
mod client;
pub mod models;
mod service;

pub use api::FoundryLocalManager;
