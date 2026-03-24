//! HuggingFace model catalog — register, download, and look up HuggingFace models.
//!
//! Created via [`FoundryLocalManager::add_catalog`]. Provides the three-step
//! flow: register (config-only download) → download (ONNX files) → inference.

use std::collections::HashMap;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use serde_json::json;

use crate::catalog::CacheInvalidator;
use crate::detail::core_interop::CoreInterop;
use crate::detail::ModelLoadManager;
use crate::error::{FoundryLocalError, Result};
use crate::hf_utils::normalize_to_huggingface_url;
use crate::model::Model;
use crate::model_variant::ModelVariant;
use crate::types::ModelInfo;

/// Filename for the HuggingFace registration persistence file.
const REGISTRATIONS_FILENAME: &str = "huggingface.modelinfo.json";

/// Internal state protected by a Mutex.
struct HuggingFaceCatalogState {
    variants_by_id: HashMap<String, Arc<ModelVariant>>,
    models_by_id: HashMap<String, Arc<Model>>,
}

/// A catalog for HuggingFace models.
///
/// Created via [`FoundryLocalManager::add_catalog`]. Each call to `add_catalog`
/// creates a new instance with registrations loaded from disk.
///
/// # Three-step flow
///
/// ```text
/// let hf = manager.add_catalog("https://huggingface.co", None).await?;
/// let model = hf.register_model("org/repo").await?;   // config files only
/// model.download::<fn(&str)>(None).await?;             // ONNX files
/// ```
pub struct HuggingFaceCatalog {
    core: Arc<CoreInterop>,
    model_load_manager: Arc<ModelLoadManager>,
    token: Option<String>,
    state: Mutex<HuggingFaceCatalogState>,
    invalidator: CacheInvalidator,
}

impl HuggingFaceCatalog {
    pub(crate) async fn create(
        core: Arc<CoreInterop>,
        model_load_manager: Arc<ModelLoadManager>,
        token: Option<String>,
    ) -> Result<Self> {
        let invalidator = CacheInvalidator::new();
        let catalog = Self {
            core,
            model_load_manager,
            token,
            state: Mutex::new(HuggingFaceCatalogState {
                variants_by_id: HashMap::new(),
                models_by_id: HashMap::new(),
            }),
            invalidator,
        };
        catalog.load_registrations()?;
        Ok(catalog)
    }

    /// Catalog name.
    pub fn name(&self) -> &str {
        "HuggingFace"
    }

    /// Register a HuggingFace model by downloading its config files only (~50KB).
    ///
    /// Sends the `register_model` FFI command to the native core, which downloads
    /// config files (genai_config.json, config.json, tokenizer_config.json, etc.)
    /// and generates metadata. Returns a `Model` with `cached: false`.
    ///
    /// After registration, call [`Model::download`] to download the ONNX files.
    pub async fn register_model(&self, model_identifier: &str) -> Result<Arc<Model>> {
        if normalize_to_huggingface_url(model_identifier).is_none() {
            return Err(FoundryLocalError::Validation {
                reason: format!(
                    "'{model_identifier}' is not a valid HuggingFace URL or org/repo identifier."
                ),
            });
        }

        let params = json!({
            "Params": {
                "Model": model_identifier,
                "Token": self.token.as_deref().unwrap_or("")
            }
        });

        let result = self
            .core
            .execute_command_async("register_model".into(), Some(params))
            .await?;

        let model_info: ModelInfo = serde_json::from_str(&result)?;

        let model = {
            let mut s = self.lock_state()?;
            let variant = ModelVariant::new(
                model_info.clone(),
                Arc::clone(&self.core),
                Arc::clone(&self.model_load_manager),
                self.invalidator.clone(),
                self.token.clone(),
            );
            let variant_arc = Arc::new(variant.clone());
            s.variants_by_id
                .insert(model_info.id.clone(), variant_arc);

            let mut m = Model::new(model_info.alias.clone(), Arc::clone(&self.core));
            m.add_variant(variant);
            let model = Arc::new(m);
            s.models_by_id
                .insert(model_info.id.clone(), Arc::clone(&model));
            model
        };

        self.save_registrations()?;

        Ok(model)
    }

    /// Look up a model by its ID, alias, or HuggingFace URL.
    ///
    /// Uses three-tier lookup:
    /// 1. Direct ID match
    /// 2. Alias match (case-insensitive)
    /// 3. URI-based match (normalize to HuggingFace URL and compare)
    ///
    /// Returns an error if the model is not found.
    pub async fn get_model(&self, identifier: &str) -> Result<Arc<Model>> {
        if identifier.trim().is_empty() {
            return Err(FoundryLocalError::Validation {
                reason: "Model identifier must be a non-empty string".into(),
            });
        }

        let s = self.lock_state()?;

        // 1. Direct ID match
        if let Some(model) = s.models_by_id.get(identifier) {
            return Ok(Arc::clone(model));
        }

        // 2. Alias match (case-insensitive)
        for model in s.models_by_id.values() {
            if model.alias().eq_ignore_ascii_case(identifier) {
                return Ok(Arc::clone(model));
            }
        }

        // 3. URI-based match — prefer exact, fall back to prefix
        if let Some(normalized_url) = normalize_to_huggingface_url(identifier) {
            let normalized_lower = normalized_url.to_lowercase();
            let normalized_with_slash =
                format!("{}/", normalized_lower.trim_end_matches('/'));

            // Exact match first
            for variant in s.variants_by_id.values() {
                let uri_lower = variant.info().uri.to_lowercase();
                if uri_lower == normalized_lower {
                    if let Some(model) = s.models_by_id.get(variant.id()) {
                        return Ok(Arc::clone(model));
                    }
                }
            }
            // Prefix match fallback
            for variant in s.variants_by_id.values() {
                let uri_lower = variant.info().uri.to_lowercase();
                if uri_lower.starts_with(&normalized_with_slash) {
                    if let Some(model) = s.models_by_id.get(variant.id()) {
                        return Ok(Arc::clone(model));
                    }
                }
            }
        }

        Err(FoundryLocalError::ModelOperation {
            reason: format!("Model '{identifier}' not found in HuggingFace catalog."),
        })
    }

    /// Download a HuggingFace model's ONNX files.
    ///
    /// Sends the `download_model` FFI command. The model should have been
    /// previously registered via [`HuggingFaceCatalog::register_model`].
    ///
    /// If `progress` is provided, it receives human-readable progress strings
    /// as the download proceeds.
    pub async fn download_model<F>(
        &self,
        model_uri: &str,
        progress: Option<F>,
    ) -> Result<Arc<Model>>
    where
        F: FnMut(&str) + Send + 'static,
    {
        if normalize_to_huggingface_url(model_uri).is_none() {
            return Err(FoundryLocalError::Validation {
                reason: format!(
                    "'{model_uri}' is not a valid HuggingFace URL or org/repo identifier."
                ),
            });
        }

        let params = json!({
            "Params": {
                "Model": model_uri,
                "Token": self.token.as_deref().unwrap_or("")
            }
        });

        let result_data = match progress {
            Some(cb) => {
                self.core
                    .execute_command_streaming_async(
                        "download_model".into(),
                        Some(params),
                        cb,
                    )
                    .await?
            }
            None => {
                self.core
                    .execute_command_async("download_model".into(), Some(params))
                    .await?
            }
        };

        // Match result against registered models by URI
        let expected_uri = format!("https://huggingface.co/{result_data}");
        let expected_lower = expected_uri.to_lowercase();
        let expected_with_slash =
            format!("{}/", expected_lower.trim_end_matches('/'));

        let s = self.lock_state()?;

        // Exact match first
        for variant in s.variants_by_id.values() {
            let uri_lower = variant.info().uri.to_lowercase();
            if uri_lower == expected_lower {
                if let Some(model) = s.models_by_id.get(variant.id()) {
                    return Ok(Arc::clone(model));
                }
            }
        }
        // Prefix match fallback
        for variant in s.variants_by_id.values() {
            let uri_lower = variant.info().uri.to_lowercase();
            if uri_lower.starts_with(&expected_with_slash)
                || expected_lower.starts_with(
                    &format!("{}/", uri_lower.trim_end_matches('/')),
                )
            {
                if let Some(model) = s.models_by_id.get(variant.id()) {
                    return Ok(Arc::clone(model));
                }
            }
        }

        Err(FoundryLocalError::ModelOperation {
            reason: format!(
                "Model '{model_uri}' was downloaded but could not be found in the catalog."
            ),
        })
    }

    /// Return all registered models.
    pub async fn get_models(&self) -> Result<Vec<Arc<Model>>> {
        let s = self.lock_state()?;
        Ok(s.models_by_id.values().cloned().collect())
    }

    /// Look up a specific model variant by its unique id.
    pub async fn get_model_variant(
        &self,
        id: &str,
    ) -> Result<Option<Arc<ModelVariant>>> {
        let s = self.lock_state()?;
        Ok(s.variants_by_id.get(id).cloned())
    }

    /// Return only the model variants that are currently cached on disk.
    pub async fn get_cached_models(&self) -> Result<Vec<Arc<ModelVariant>>> {
        let raw = self
            .core
            .execute_command_async("get_cached_models".into(), None)
            .await?;
        if raw.trim().is_empty() {
            return Ok(Vec::new());
        }
        let cached_ids: Vec<String> = serde_json::from_str(&raw)?;
        let s = self.lock_state()?;
        Ok(cached_ids
            .iter()
            .filter_map(|id| s.variants_by_id.get(id).cloned())
            .collect())
    }

    /// Return model variants that are currently loaded into memory.
    pub async fn get_loaded_models(&self) -> Result<Vec<Arc<ModelVariant>>> {
        let loaded_ids = self.model_load_manager.list_loaded().await?;
        let s = self.lock_state()?;
        Ok(loaded_ids
            .iter()
            .filter_map(|id| s.variants_by_id.get(id).cloned())
            .collect())
    }

    // ── Persistence ──────────────────────────────────────────────────────

    fn registrations_path(&self) -> Result<PathBuf> {
        let cache_dir = self
            .core
            .execute_command("get_cache_directory".into(), None)?;
        Ok(PathBuf::from(cache_dir.trim().trim_matches('"'))
            .join("HuggingFace")
            .join(REGISTRATIONS_FILENAME))
    }

    fn load_registrations(&self) -> Result<()> {
        let path = match self.registrations_path() {
            Ok(p) => p,
            Err(_) => return Ok(()), // gracefully skip if home dir unknown
        };
        if !path.exists() {
            return Ok(());
        }

        let json = match std::fs::read_to_string(&path) {
            Ok(s) => s,
            Err(_) => return Ok(()), // gracefully skip on read error
        };
        if json.trim().is_empty() {
            return Ok(());
        }

        let infos: Vec<ModelInfo> = match serde_json::from_str(&json) {
            Ok(v) => v,
            Err(_) => return Ok(()), // gracefully skip on parse error
        };

        let mut s = self.lock_state()?;
        for info in infos {
            let variant = ModelVariant::new(
                info.clone(),
                Arc::clone(&self.core),
                Arc::clone(&self.model_load_manager),
                self.invalidator.clone(),
                self.token.clone(),
            );
            let variant_arc = Arc::new(variant.clone());
            s.variants_by_id.insert(info.id.clone(), variant_arc);

            let mut m = Model::new(info.alias.clone(), Arc::clone(&self.core));
            m.add_variant(variant);
            s.models_by_id.insert(info.id.clone(), Arc::new(m));
        }

        Ok(())
    }

    fn save_registrations(&self) -> Result<()> {
        let path = match self.registrations_path() {
            Ok(p) => p,
            Err(_) => return Ok(()), // gracefully skip
        };
        if let Some(dir) = path.parent() {
            let _ = std::fs::create_dir_all(dir);
        }

        // Snapshot data under the lock, then release before doing I/O
        let infos: Vec<ModelInfo> = {
            let s = self.lock_state()?;
            s.variants_by_id.values().map(|v| v.info().clone()).collect()
        };

        let json = serde_json::to_string_pretty(&infos)
            .map_err(|e| FoundryLocalError::Internal {
                reason: format!("Failed to serialize registrations: {e}"),
            })?;
        std::fs::write(&path, json).map_err(|e| FoundryLocalError::Internal {
            reason: format!("Failed to write registrations file: {e}"),
        })?;
        Ok(())
    }

    fn lock_state(
        &self,
    ) -> Result<std::sync::MutexGuard<'_, HuggingFaceCatalogState>> {
        self.state.lock().map_err(|_| FoundryLocalError::Internal {
            reason: "HuggingFace catalog state mutex poisoned".into(),
        })
    }
}
