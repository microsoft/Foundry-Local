use std::net::SocketAddr;
use std::sync::{Arc, Mutex};

use axum::{
    extract::State,
    response::IntoResponse,
    routing::{get, post},
    Json, Router,
};
use serde_json::{json, Value};
use tokio::net::TcpListener;
use tokio::sync::oneshot;

use foundry_local::models::{ExecutionProvider, FoundryModelInfo};

// MockState represents the state of the mock Foundry Local service
pub struct MockState {
    catalog_models: Vec<FoundryModelInfo>,
    cached_models: Vec<String>,
    loaded_models: Vec<String>,
    cache_location: String,
}

impl Default for MockState {
    fn default() -> Self {
        Self {
            catalog_models: vec![
                FoundryModelInfo {
                    id: "mock-model-1".to_string(),
                    alias: "mock-small".to_string(),
                    runtime: ExecutionProvider::CPU,
                    file_size_mb: 100,
                    uri: "https://mock-uri/model1".to_string(),
                    version: "1.0".to_string(),
                    prompt_template: serde_json::json!({}),
                    provider: "MockProvider".to_string(),
                    publisher: "MockPublisher".to_string(),
                    license: "MIT".to_string(),
                    task: "text-generation".to_string(),
                },
                FoundryModelInfo {
                    id: "mock-model-2".to_string(),
                    alias: "mock-medium".to_string(),
                    runtime: ExecutionProvider::CUDA,
                    file_size_mb: 500,
                    uri: "https://mock-uri/model2".to_string(),
                    version: "1.0".to_string(),
                    prompt_template: serde_json::json!({}),
                    provider: "MockProvider".to_string(),
                    publisher: "MockPublisher".to_string(),
                    license: "MIT".to_string(),
                    task: "text-generation".to_string(),
                },
            ],
            cached_models: vec!["mock-model-1".to_string()],
            loaded_models: vec![],
            cache_location: "/tmp/mock-cache".to_string(),
        }
    }
}

type AppState = Arc<Mutex<MockState>>;

// Handler for /foundry/list endpoint
async fn list_catalog(State(state): State<AppState>) -> impl IntoResponse {
    let state = state.lock().unwrap();
    let models = state.catalog_models.iter()
        .map(|model| {
            json!({
                "name": model.id,
                "displayName": model.id,
                "modelType": "LLM",
                "providerType": model.provider,
                "uri": model.uri,
                "version": model.version,
                "promptTemplate": model.prompt_template,
                "publisher": model.publisher,
                "task": model.task,
                "runtime": {
                    "deviceType": if model.runtime == ExecutionProvider::CPU { "CPU" } else { "GPU" },
                    "executionProvider": model.runtime.to_string()
                },
                "fileSizeMb": model.file_size_mb,
                "modelSettings": json!({}),
                "alias": model.alias,
                "supportsToolCalling": false,
                "license": model.license,
                "licenseDescription": "",
                "parentModelUri": ""
            })
        })
        .collect::<Vec<_>>();
    
    Json(models)
}

// Handler for /openai/models endpoint
async fn list_cached_models(State(state): State<AppState>) -> impl IntoResponse {
    let state = state.lock().unwrap();
    Json(json!({
        "models": state.cached_models
    }))
}

// Handler for /foundry/cache endpoint
async fn get_cache_location(State(state): State<AppState>) -> impl IntoResponse {
    let state = state.lock().unwrap();
    Json(json!({
        "location": state.cache_location
    }))
}

// Handler for /foundry/loaded endpoint
async fn list_loaded_models(State(state): State<AppState>) -> impl IntoResponse {
    let state = state.lock().unwrap();
    Json(json!({
        "models": state.loaded_models
    }))
}

// Handler for /openai/download endpoint
async fn download_model(State(state): State<AppState>, Json(payload): Json<Value>) -> impl IntoResponse {
    let model_id = payload["model"]["Name"].as_str().unwrap_or_default();
    let mut state = state.lock().unwrap();
    if !state.cached_models.contains(&model_id.to_string()) {
        state.cached_models.push(model_id.to_string());
    }
    
    Json(json!({
        "status": "success",
        "message": format!("Downloaded model {}", model_id)
    }))
}

// Handler for /openai/load/:model_id endpoint
async fn load_model(
    State(state): State<AppState>,
    axum::extract::Path(model_id): axum::extract::Path<String>
) -> impl IntoResponse {
    let mut state = state.lock().unwrap();
    if !state.loaded_models.contains(&model_id) {
        state.loaded_models.push(model_id.clone());
    }
    
    Json(json!({
        "status": "success",
        "message": format!("Loaded model {}", model_id)
    }))
}

// Handler for /openai/unload/:model_id endpoint
async fn unload_model(
    State(state): State<AppState>,
    axum::extract::Path(model_id): axum::extract::Path<String>,
) -> impl IntoResponse {
    let mut state = state.lock().unwrap();
    state.loaded_models.retain(|id| *id != model_id);
    
    Json(json!({
        "status": "success",
        "message": format!("Unloaded model {}", model_id)
    }))
}

// Create and start the mock server
pub async fn start_mock_server() -> (String, oneshot::Sender<()>) {
    let state = Arc::new(Mutex::new(MockState::default()));
    
    let app = Router::new()
        .route("/foundry/list", get(list_catalog))
        .route("/openai/models", get(list_cached_models))
        .route("/foundry/cache", get(get_cache_location))
        .route("/openai/loadedmodels", get(list_loaded_models))
        .route("/openai/download", post(download_model))
        .route("/openai/load/:model_id", get(load_model))
        .route("/openai/unload/:model_id", get(unload_model))
        .with_state(state);
    
    let addr = SocketAddr::from(([127, 0, 0, 1], 0));
    let listener = TcpListener::bind(addr).await.unwrap();
    let server_addr = listener.local_addr().unwrap();
    let server_uri = format!("http://{}", server_addr);
    
    let (tx, rx) = oneshot::channel();
    
    tokio::spawn(async move {
        axum::serve(listener, app)
            .with_graceful_shutdown(async {
                rx.await.ok();
            })
            .await
            .unwrap();
    });
    
    (server_uri, tx)
} 