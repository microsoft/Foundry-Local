//! Single integration test binary for the Foundry Local Rust SDK.
//!
//! All test modules are compiled into one binary so the native core is only
//! initialised once (via the `OnceLock` singleton in `FoundryLocalManager`).
//! Running them as separate binaries causes "already initialized" errors
//! because the .NET native runtime retains state across process-level
//! library loads.

mod common;

mod manager_tests {
    use super::common;
    use foundry_local_sdk::FoundryLocalManager;

    #[test]
    fn should_initialize_successfully() {
        let config = common::test_config();
        let manager = FoundryLocalManager::create(config);
        assert!(
            manager.is_ok(),
            "Manager creation failed: {:?}",
            manager.err()
        );
    }

    #[test]
    fn should_return_catalog_with_non_empty_name() {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();
        let name = catalog.name();
        assert!(!name.is_empty(), "Catalog name should not be empty");
    }
}

mod catalog_tests {
    use super::common;
    use foundry_local_sdk::Catalog;

    fn catalog() -> &'static Catalog {
        common::get_test_manager().catalog()
    }

    #[test]
    fn should_initialize_with_catalog_name() {
        let cat = catalog();
        let name = cat.name();
        assert!(!name.is_empty(), "Catalog name must not be empty");
    }

    #[tokio::test]
    async fn should_list_models() {
        let cat = catalog();
        let models = cat.get_models().await.expect("get_models failed");

        assert!(
            !models.is_empty(),
            "Expected at least one model in the catalog"
        );

        let found = models.iter().any(|m| m.alias() == common::TEST_MODEL_ALIAS);
        assert!(
            found,
            "Test model '{}' not found in catalog",
            common::TEST_MODEL_ALIAS
        );
    }

    #[tokio::test]
    async fn should_get_model_by_alias() {
        let cat = catalog();
        let model = cat
            .get_model(common::TEST_MODEL_ALIAS)
            .await
            .expect("get_model failed");

        assert_eq!(model.alias(), common::TEST_MODEL_ALIAS);
    }

    #[tokio::test]
    async fn should_throw_when_getting_model_with_empty_alias() {
        let cat = catalog();
        let result = cat.get_model("").await;
        assert!(result.is_err(), "Expected error for empty alias");

        let err_msg = result.unwrap_err().to_string();
        assert!(
            err_msg.contains("Model alias must be a non-empty string"),
            "Unexpected error message: {err_msg}"
        );
    }

    #[tokio::test]
    async fn should_throw_when_getting_model_with_unknown_alias() {
        let cat = catalog();
        let result = cat.get_model("unknown-nonexistent-model-alias").await;
        assert!(result.is_err(), "Expected error for unknown alias");

        let err_msg = result.unwrap_err().to_string();
        assert!(
            err_msg.contains("Unknown model alias"),
            "Error should mention unknown alias: {err_msg}"
        );
        assert!(
            err_msg.contains("Available"),
            "Error should list available models: {err_msg}"
        );
    }

    #[tokio::test]
    async fn should_get_cached_models() {
        let cat = catalog();
        let cached = cat
            .get_cached_models()
            .await
            .expect("get_cached_models failed");

        assert!(!cached.is_empty(), "Expected at least one cached model");

        let found = cached.iter().any(|m| m.alias() == common::TEST_MODEL_ALIAS);
        assert!(
            found,
            "Test model '{}' should be in the cached models list",
            common::TEST_MODEL_ALIAS
        );
    }

    #[tokio::test]
    async fn should_throw_when_getting_model_variant_with_empty_id() {
        let cat = catalog();
        let result = cat.get_model_variant("").await;
        assert!(result.is_err(), "Expected error for empty variant ID");
    }

    #[tokio::test]
    async fn should_throw_when_getting_model_variant_with_unknown_id() {
        let cat = catalog();
        let result = cat
            .get_model_variant("unknown-nonexistent-variant-id")
            .await;
        assert!(result.is_err(), "Expected error for unknown variant ID");
    }
}

mod model_tests {
    use super::common;

    #[tokio::test]
    async fn should_verify_cached_models_from_test_data_shared() {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();
        let cached = catalog
            .get_cached_models()
            .await
            .expect("get_cached_models failed");

        let has_qwen = cached.iter().any(|m| m.alias() == common::TEST_MODEL_ALIAS);
        assert!(
            has_qwen,
            "'{}' should be present in cached models",
            common::TEST_MODEL_ALIAS
        );

        let has_whisper = cached
            .iter()
            .any(|m| m.alias() == common::WHISPER_MODEL_ALIAS);
        assert!(
            has_whisper,
            "'{}' should be present in cached models",
            common::WHISPER_MODEL_ALIAS
        );
    }

    #[tokio::test]
    async fn should_load_and_unload_model() {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();
        let model = catalog
            .get_model(common::TEST_MODEL_ALIAS)
            .await
            .expect("get_model failed");

        model.load().await.expect("model.load() failed");
        assert!(
            model.is_loaded().await.expect("is_loaded check failed"),
            "Model should be loaded after load()"
        );

        model.unload().await.expect("model.unload() failed");
        assert!(
            !model.is_loaded().await.expect("is_loaded check failed"),
            "Model should not be loaded after unload()"
        );
    }
}

mod model_load_manager_tests {
    use super::common;

    async fn get_test_model() -> foundry_local_sdk::Model {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();
        catalog
            .get_model(common::TEST_MODEL_ALIAS)
            .await
            .expect("get_model failed")
    }

    #[tokio::test]
    async fn should_load_model_using_core_interop() {
        let model = get_test_model().await;
        model.load().await.expect("model.load() failed");
    }

    #[tokio::test]
    async fn should_unload_model_using_core_interop() {
        let model = get_test_model().await;
        model.load().await.expect("model.load() failed");
        model.unload().await.expect("model.unload() failed");
    }

    #[tokio::test]
    async fn should_list_loaded_models_using_core_interop() {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();

        let loaded = catalog
            .get_loaded_models()
            .await
            .expect("catalog.get_loaded_models() failed");

        let _ = loaded;
    }

    #[tokio::test]
    #[ignore = "requires running web service"]
    async fn should_load_and_unload_model_using_external_service() {
        if common::is_running_in_ci() {
            eprintln!("Skipping external-service test in CI");
            return;
        }

        let manager = common::get_test_manager();
        let model = get_test_model().await;

        let _urls = manager
            .start_web_service()
            .await
            .expect("start_web_service failed");

        model
            .load()
            .await
            .expect("load via external service failed");

        model
            .unload()
            .await
            .expect("unload via external service failed");
    }

    #[tokio::test]
    #[ignore = "requires running web service"]
    async fn should_list_loaded_models_using_external_service() {
        if common::is_running_in_ci() {
            eprintln!("Skipping external-service test in CI");
            return;
        }

        let manager = common::get_test_manager();

        let _urls = manager
            .start_web_service()
            .await
            .expect("start_web_service failed");

        let catalog = manager.catalog();
        let loaded = catalog
            .get_loaded_models()
            .await
            .expect("get_loaded_models via external service failed");

        let _ = loaded;
    }
}

mod chat_client_tests {
    use super::common;
    use foundry_local_sdk::openai::ChatClient;
    use foundry_local_sdk::{
        ChatCompletionMessageToolCalls, ChatCompletionRequestMessage,
        ChatCompletionRequestSystemMessage, ChatCompletionRequestToolMessage,
        ChatCompletionRequestUserMessage, ChatToolChoice,
    };
    use serde_json::json;
    use tokio_stream::StreamExt;

    async fn setup_chat_client() -> ChatClient {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();
        let model = catalog
            .get_model(common::TEST_MODEL_ALIAS)
            .await
            .expect("get_model failed");
        model.load().await.expect("model.load() failed");

        let mut client = model.create_chat_client();
        client.max_tokens(500).temperature(0.0);
        client
    }

    fn user_message(content: &str) -> ChatCompletionRequestMessage {
        ChatCompletionRequestUserMessage::from(content).into()
    }

    fn system_message(content: &str) -> ChatCompletionRequestMessage {
        ChatCompletionRequestSystemMessage::from(content).into()
    }

    fn assistant_message(content: &str) -> ChatCompletionRequestMessage {
        serde_json::from_value(json!({ "role": "assistant", "content": content }))
            .expect("failed to construct assistant message")
    }

    #[tokio::test]
    async fn should_perform_chat_completion() {
        let client = setup_chat_client().await;
        let messages = vec![
            system_message("You are a helpful math assistant. Respond with just the answer."),
            user_message("What is 7*6?"),
        ];

        let response = client
            .complete_chat(&messages, None)
            .await
            .expect("complete_chat failed");
        let content = response
            .choices
            .first()
            .and_then(|c| c.message.content.as_deref())
            .unwrap_or("");

        assert!(
            content.contains("42"),
            "Expected response to contain '42', got: {content}"
        );
    }

    #[tokio::test]
    async fn should_perform_streaming_chat_completion() {
        let client = setup_chat_client().await;
        let mut messages = vec![
            system_message("You are a helpful math assistant. Respond with just the answer."),
            user_message("What is 7*6?"),
        ];

        let mut first_result = String::new();
        let mut stream = client
            .complete_streaming_chat(&messages, None)
            .await
            .expect("streaming chat (first turn) setup failed");
        while let Some(chunk) = stream.next().await {
            let chunk = chunk.expect("stream chunk error");
            if let Some(choice) = chunk.choices.first() {
                if let Some(ref content) = choice.delta.content {
                    first_result.push_str(content);
                }
            }
        }
        stream.close().await.expect("stream close failed");

        assert!(
            first_result.contains("42"),
            "First turn should contain '42', got: {first_result}"
        );

        messages.push(assistant_message(&first_result));
        messages.push(user_message("Now add 25 to that result."));

        let mut second_result = String::new();
        let mut stream = client
            .complete_streaming_chat(&messages, None)
            .await
            .expect("streaming chat (follow-up) setup failed");
        while let Some(chunk) = stream.next().await {
            let chunk = chunk.expect("stream chunk error");
            if let Some(choice) = chunk.choices.first() {
                if let Some(ref content) = choice.delta.content {
                    second_result.push_str(content);
                }
            }
        }
        stream.close().await.expect("stream close failed");

        assert!(
            second_result.contains("67"),
            "Follow-up should contain '67', got: {second_result}"
        );
    }

    #[tokio::test]
    async fn should_throw_when_completing_chat_with_empty_messages() {
        let client = setup_chat_client().await;
        let messages: Vec<ChatCompletionRequestMessage> = vec![];

        let result = client.complete_chat(&messages, None).await;
        assert!(result.is_err(), "Expected error for empty messages");
    }

    #[tokio::test]
    async fn should_throw_when_completing_streaming_chat_with_empty_messages() {
        let client = setup_chat_client().await;
        let messages: Vec<ChatCompletionRequestMessage> = vec![];

        let result = client.complete_streaming_chat(&messages, None).await;
        assert!(
            result.is_err(),
            "Expected error for empty messages in streaming"
        );
    }

    #[tokio::test]
    async fn should_throw_when_completing_streaming_chat_with_invalid_callback() {
        let client = setup_chat_client().await;
        let messages: Vec<ChatCompletionRequestMessage> = vec![];

        let result = client.complete_streaming_chat(&messages, None).await;
        assert!(result.is_err(), "Expected error even with empty messages");
    }

    #[tokio::test]
    async fn should_perform_tool_calling_chat_completion_non_streaming() {
        let mut client = setup_chat_client().await;
        client.tool_choice(ChatToolChoice::Required);

        let tools = vec![common::get_multiply_tool()];
        let mut messages = vec![
            system_message("You are a math assistant. Use the multiply tool to answer."),
            user_message("What is 6 times 7?"),
        ];

        let response = client
            .complete_chat(&messages, Some(&tools))
            .await
            .expect("complete_chat with tools failed");

        let choice = response
            .choices
            .first()
            .expect("Expected at least one choice");
        let tool_calls = choice
            .message
            .tool_calls
            .as_ref()
            .expect("Expected tool_calls");
        assert!(
            !tool_calls.is_empty(),
            "Expected at least one tool call in the response"
        );

        let tool_call = match &tool_calls[0] {
            ChatCompletionMessageToolCalls::Function(tc) => tc,
            _ => panic!("Expected a function tool call"),
        };
        assert_eq!(
            tool_call.function.name, "multiply",
            "Expected tool call to 'multiply'"
        );

        let args: serde_json::Value = serde_json::from_str(&tool_call.function.arguments)
            .expect("Failed to parse tool call arguments");
        let a = args["a"].as_f64().unwrap_or(0.0);
        let b = args["b"].as_f64().unwrap_or(0.0);
        let product = (a * b) as i64;

        let tool_call_id = &tool_call.id;
        let assistant_msg: ChatCompletionRequestMessage = serde_json::from_value(json!({
            "role": "assistant",
            "content": null,
            "tool_calls": [{
                "id": tool_call_id,
                "type": "function",
                "function": {
                    "name": tool_call.function.name,
                    "arguments": tool_call.function.arguments,
                }
            }]
        }))
        .expect("failed to construct assistant message");
        messages.push(assistant_msg);
        messages.push(
            ChatCompletionRequestToolMessage {
                content: product.to_string().into(),
                tool_call_id: tool_call_id.clone(),
            }
            .into(),
        );

        client.tool_choice(ChatToolChoice::Auto);

        let final_response = client
            .complete_chat(&messages, Some(&tools))
            .await
            .expect("follow-up complete_chat with tools failed");
        let content = final_response
            .choices
            .first()
            .and_then(|c| c.message.content.as_deref())
            .unwrap_or("");

        assert!(
            content.contains("42"),
            "Final answer should contain '42', got: {content}"
        );
    }

    #[tokio::test]
    async fn should_perform_tool_calling_chat_completion_streaming() {
        let mut client = setup_chat_client().await;
        client.tool_choice(ChatToolChoice::Required);

        let tools = vec![common::get_multiply_tool()];
        let mut messages = vec![
            system_message("You are a math assistant. Use the multiply tool to answer."),
            user_message("What is 6 times 7?"),
        ];

        let mut tool_call_name = String::new();
        let mut tool_call_args = String::new();
        let mut tool_call_id = String::new();

        let mut stream = client
            .complete_streaming_chat(&messages, Some(&tools))
            .await
            .expect("streaming tool call setup failed");

        while let Some(chunk) = stream.next().await {
            let chunk = chunk.expect("stream chunk error");
            if let Some(choice) = chunk.choices.first() {
                if let Some(ref tool_calls) = choice.delta.tool_calls {
                    for call in tool_calls {
                        if let Some(ref func) = call.function {
                            if let Some(ref name) = func.name {
                                tool_call_name.push_str(name);
                            }
                            if let Some(ref args) = func.arguments {
                                tool_call_args.push_str(args);
                            }
                        }
                        if let Some(ref id) = call.id {
                            tool_call_id = id.clone();
                        }
                    }
                }
            }
        }
        stream.close().await.expect("stream close failed");

        assert_eq!(
            tool_call_name, "multiply",
            "Expected streamed tool call to 'multiply'"
        );

        let args: serde_json::Value =
            serde_json::from_str(&tool_call_args).unwrap_or_else(|_| json!({}));
        let a = args["a"].as_f64().unwrap_or(0.0);
        let b = args["b"].as_f64().unwrap_or(0.0);
        let product = (a * b) as i64;

        let assistant_msg: ChatCompletionRequestMessage = serde_json::from_value(json!({
            "role": "assistant",
            "tool_calls": [{
                "id": tool_call_id,
                "type": "function",
                "function": {
                    "name": tool_call_name,
                    "arguments": tool_call_args
                }
            }]
        }))
        .expect("failed to construct assistant message");
        messages.push(assistant_msg);
        messages.push(
            ChatCompletionRequestToolMessage {
                content: product.to_string().into(),
                tool_call_id: tool_call_id.clone(),
            }
            .into(),
        );

        client.tool_choice(ChatToolChoice::Auto);

        let mut final_result = String::new();
        let mut stream = client
            .complete_streaming_chat(&messages, Some(&tools))
            .await
            .expect("streaming follow-up setup failed");
        while let Some(chunk) = stream.next().await {
            let chunk = chunk.expect("stream chunk error");
            if let Some(choice) = chunk.choices.first() {
                if let Some(ref content) = choice.delta.content {
                    final_result.push_str(content);
                }
            }
        }
        stream.close().await.expect("stream close failed");

        assert!(
            final_result.contains("42"),
            "Streamed final answer should contain '42', got: {final_result}"
        );
    }
}

mod audio_client_tests {
    use super::common;
    use foundry_local_sdk::openai::AudioClient;
    use tokio_stream::StreamExt;

    async fn setup_audio_client() -> AudioClient {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();
        let model = catalog
            .get_model(common::WHISPER_MODEL_ALIAS)
            .await
            .expect("get_model(whisper-tiny) failed");
        model.load().await.expect("model.load() failed");
        model.create_audio_client()
    }

    fn audio_file() -> String {
        common::get_audio_file_path().to_string_lossy().into_owned()
    }

    #[tokio::test]
    async fn should_transcribe_audio_without_streaming() {
        let mut client = setup_audio_client().await;
        client.language("en").temperature(0.0);
        let response = client
            .transcribe(&audio_file())
            .await
            .expect("transcribe failed");

        assert!(
            response.text.contains(common::EXPECTED_TRANSCRIPTION_TEXT),
            "Transcription should contain expected text, got: {}",
            response.text
        );
    }

    #[tokio::test]
    async fn should_transcribe_audio_without_streaming_with_temperature() {
        let mut client = setup_audio_client().await;
        client.language("en").temperature(0.0);

        let response = client
            .transcribe(&audio_file())
            .await
            .expect("transcribe with temperature failed");

        assert!(
            response.text.contains(common::EXPECTED_TRANSCRIPTION_TEXT),
            "Transcription should contain expected text, got: {}",
            response.text
        );
    }

    #[tokio::test]
    async fn should_transcribe_audio_with_streaming() {
        let mut client = setup_audio_client().await;
        client.language("en").temperature(0.0);
        let mut full_text = String::new();

        let mut stream = client
            .transcribe_streaming(&audio_file())
            .await
            .expect("transcribe_streaming setup failed");

        while let Some(chunk) = stream.next().await {
            let chunk = chunk.expect("stream chunk error");
            full_text.push_str(&chunk.text);
        }
        stream.close().await.expect("stream close failed");

        assert!(
            full_text.contains(common::EXPECTED_TRANSCRIPTION_TEXT),
            "Streamed transcription should contain expected text, got: {full_text}"
        );
    }

    #[tokio::test]
    async fn should_transcribe_audio_with_streaming_with_temperature() {
        let mut client = setup_audio_client().await;
        client.language("en").temperature(0.0);

        let mut full_text = String::new();

        let mut stream = client
            .transcribe_streaming(&audio_file())
            .await
            .expect("transcribe_streaming with temperature setup failed");

        while let Some(chunk) = stream.next().await {
            let chunk = chunk.expect("stream chunk error");
            full_text.push_str(&chunk.text);
        }
        stream.close().await.expect("stream close failed");

        assert!(
            full_text.contains(common::EXPECTED_TRANSCRIPTION_TEXT),
            "Streamed transcription should contain expected text, got: {full_text}"
        );
    }

    #[tokio::test]
    async fn should_throw_when_transcribing_with_empty_audio_file_path() {
        let client = setup_audio_client().await;
        let result = client.transcribe("").await;
        assert!(result.is_err(), "Expected error for empty audio file path");
    }

    #[tokio::test]
    async fn should_throw_when_transcribing_streaming_with_empty_audio_file_path() {
        let client = setup_audio_client().await;
        let result = client.transcribe_streaming("").await;
        assert!(
            result.is_err(),
            "Expected error for empty audio file path in streaming"
        );
    }
}
