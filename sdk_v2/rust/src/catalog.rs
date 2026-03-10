//! Model catalog – discovers, caches, and looks up available models.

use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use crate::detail::core_interop::CoreInterop;
use crate::detail::ModelLoadManager;
use crate::error::{FoundryLocalError, Result};
use crate::model::Model;
use crate::model_variant::ModelVariant;
use crate::types::ModelInfo;

/// How long the catalog cache remains valid before a refresh.
const CACHE_TTL: Duration = Duration::from_secs(6 * 60 * 60); // 6 hours

/// The model catalog provides discovery and lookup for all available models.
pub struct Catalog {
    core: Arc<CoreInterop>,
    model_load_manager: Arc<ModelLoadManager>,
    name: String,
    models_by_alias: Mutex<HashMap<String, Model>>,
    variants_by_id: Mutex<HashMap<String, ModelVariant>>,
    last_refresh: Mutex<Option<Instant>>,
}

impl Catalog {
    pub(crate) fn new(
        core: Arc<CoreInterop>,
        model_load_manager: Arc<ModelLoadManager>,
    ) -> Result<Self> {
        let name = core
            .execute_command("get_catalog_name", None)
            .unwrap_or_else(|_| "default".into());

        let catalog = Self {
            core,
            model_load_manager,
            name,
            models_by_alias: Mutex::new(HashMap::new()),
            variants_by_id: Mutex::new(HashMap::new()),
            last_refresh: Mutex::new(None),
        };

        // Perform initial synchronous refresh during construction.
        catalog.force_refresh_sync()?;
        Ok(catalog)
    }

    /// Catalog name as reported by the native core.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Refresh the catalog from the native core if the cache has expired.
    pub async fn update_models(&self) -> Result<()> {
        {
            let last = self.last_refresh.lock().unwrap();
            if let Some(ts) = *last {
                if ts.elapsed() < CACHE_TTL {
                    return Ok(());
                }
            }
        }

        self.force_refresh().await
    }

    /// Return all known models keyed by alias.
    pub async fn get_models(&self) -> Result<Vec<Model>> {
        self.update_models().await?;
        let map = self.models_by_alias.lock().unwrap();
        Ok(map.values().cloned().collect())
    }

    /// Look up a model by its alias.
    pub async fn get_model(&self, alias: &str) -> Result<Model> {
        if alias.trim().is_empty() {
            return Err(FoundryLocalError::Validation {
                reason: "Model alias must be a non-empty string".into(),
            });
        }
        self.update_models().await?;
        let map = self.models_by_alias.lock().unwrap();
        map.get(alias).cloned().ok_or_else(|| {
            let available: Vec<&String> = map.keys().collect();
            FoundryLocalError::ModelOperation {
                reason: format!("Unknown model alias '{alias}'. Available: {available:?}"),
            }
        })
    }

    /// Look up a specific model variant by its unique id.
    pub async fn get_model_variant(&self, id: &str) -> Result<ModelVariant> {
        if id.trim().is_empty() {
            return Err(FoundryLocalError::Validation {
                reason: "Variant id must be a non-empty string".into(),
            });
        }
        self.update_models().await?;
        let map = self.variants_by_id.lock().unwrap();
        map.get(id).cloned().ok_or_else(|| {
            let available: Vec<&String> = map.keys().collect();
            FoundryLocalError::ModelOperation {
                reason: format!("Unknown variant id '{id}'. Available: {available:?}"),
            }
        })
    }

    /// Return only the model variants that are currently cached on disk.
    ///
    /// The native core returns a list of variant IDs. This method resolves
    /// them against the internal cache, matching the JS SDK behaviour.
    pub async fn get_cached_models(&self) -> Result<Vec<ModelVariant>> {
        self.update_models().await?;
        let raw = self
            .core
            .execute_command_async("get_cached_models".into(), None)
            .await?;
        if raw.trim().is_empty() {
            return Ok(Vec::new());
        }
        let cached_ids: Vec<String> = serde_json::from_str(&raw)?;
        let id_map = self.variants_by_id.lock().unwrap();
        Ok(cached_ids
            .iter()
            .filter_map(|id| id_map.get(id).cloned())
            .collect())
    }

    /// Return identifiers of models that are currently loaded into memory.
    pub async fn get_loaded_models(&self) -> Result<Vec<String>> {
        self.model_load_manager.list_loaded().await
    }

    async fn force_refresh(&self) -> Result<()> {
        let raw = self
            .core
            .execute_command_async("get_model_list".into(), None)
            .await?;
        self.apply_model_list(&raw)
    }

    /// Synchronous refresh used only during construction (before a tokio
    /// runtime may be available).
    fn force_refresh_sync(&self) -> Result<()> {
        let raw = self.core.execute_command("get_model_list", None)?;
        self.apply_model_list(&raw)
    }

    fn apply_model_list(&self, raw: &str) -> Result<()> {
        let infos: Vec<ModelInfo> = if raw.trim().is_empty() {
            Vec::new()
        } else {
            serde_json::from_str(raw)?
        };

        let mut alias_map: HashMap<String, Model> = HashMap::new();
        let mut id_map: HashMap<String, ModelVariant> = HashMap::new();

        for info in infos {
            let variant = ModelVariant::new(
                info.clone(),
                Arc::clone(&self.core),
                Arc::clone(&self.model_load_manager),
            );
            id_map.insert(info.id.clone(), variant.clone());

            alias_map
                .entry(info.alias.clone())
                .or_insert_with(|| {
                    Model::new(
                        info.alias.clone(),
                        Arc::clone(&self.core),
                        Arc::clone(&self.model_load_manager),
                    )
                })
                .add_variant(variant);
        }

        *self.models_by_alias.lock().unwrap() = alias_map;
        *self.variants_by_id.lock().unwrap() = id_map;
        *self.last_refresh.lock().unwrap() = Some(Instant::now());

        Ok(())
    }
}
