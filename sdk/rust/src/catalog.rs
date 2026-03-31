//! Model catalog – discovers, caches, and looks up available models.

use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use crate::detail::core_interop::CoreInterop;
use crate::detail::ModelLoadManager;
use crate::error::{FoundryLocalError, Result};
use crate::imodel::IModel;
use crate::detail::model::Model;
use crate::detail::model_variant::ModelVariant;
use crate::types::ModelInfo;

/// How long the catalog cache remains valid before a refresh.
const CACHE_TTL: Duration = Duration::from_secs(6 * 60 * 60); // 6 hours

/// Shared flag allowing `ModelVariant` to signal that the catalog cache is
/// stale (e.g. after a download or removal).
#[derive(Clone, Debug)]
pub(crate) struct CacheInvalidator(Arc<AtomicBool>);

impl CacheInvalidator {
    fn new() -> Self {
        Self(Arc::new(AtomicBool::new(false)))
    }

    /// Mark the catalog cache as stale.
    pub fn invalidate(&self) {
        self.0.store(true, Ordering::Release);
    }

    /// Check and clear the invalidation flag.
    fn take(&self) -> bool {
        self.0.swap(false, Ordering::AcqRel)
    }
}

/// All mutable catalog data behind a single lock to prevent split-brain reads.
struct CatalogState {
    models_by_alias: HashMap<String, Arc<Model>>,
    variants_by_id: HashMap<String, Arc<ModelVariant>>,
    last_refresh: Option<Instant>,
}

/// The model catalog provides discovery and lookup for all available models.
pub struct Catalog {
    core: Arc<CoreInterop>,
    model_load_manager: Arc<ModelLoadManager>,
    name: String,
    state: Mutex<CatalogState>,
    /// Async gate ensuring only one refresh runs at a time.
    refresh_gate: tokio::sync::Mutex<()>,
    invalidator: CacheInvalidator,
}

impl Catalog {
    pub(crate) fn new(
        core: Arc<CoreInterop>,
        model_load_manager: Arc<ModelLoadManager>,
    ) -> Result<Self> {
        let name = core
            .execute_command("get_catalog_name", None)
            .unwrap_or_else(|_| "default".into());

        let invalidator = CacheInvalidator::new();
        let catalog = Self {
            core,
            model_load_manager,
            name,
            state: Mutex::new(CatalogState {
                models_by_alias: HashMap::new(),
                variants_by_id: HashMap::new(),
                last_refresh: None,
            }),
            refresh_gate: tokio::sync::Mutex::new(()),
            invalidator,
        };

        // Perform initial synchronous refresh during construction.
        catalog.force_refresh_sync()?;
        Ok(catalog)
    }

    /// Catalog name as reported by the native core.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Refresh the catalog from the native core if the cache has expired or
    /// has been explicitly invalidated (e.g. after a download or removal).
    pub async fn update_models(&self) -> Result<()> {
        let invalidated = self.invalidator.take();

        // Fast path: check under data lock (held briefly).
        if !invalidated {
            let s = self.lock_state()?;
            if let Some(ts) = s.last_refresh {
                if ts.elapsed() < CACHE_TTL {
                    return Ok(());
                }
            }
        }

        // Slow path: acquire refresh gate so only one thread refreshes.
        let _gate = self.refresh_gate.lock().await;

        // Re-check after acquiring the gate — another thread may have refreshed.
        if !invalidated {
            let s = self.lock_state()?;
            if let Some(ts) = s.last_refresh {
                if ts.elapsed() < CACHE_TTL {
                    return Ok(());
                }
            }
        }

        self.force_refresh().await
    }

    /// Return all known models keyed by alias.
    pub async fn get_models(&self) -> Result<Vec<Arc<dyn IModel>>> {
        self.update_models().await?;
        let s = self.lock_state()?;
        Ok(s.models_by_alias
            .values()
            .map(|m| {
                let model: Arc<dyn IModel> = m.clone();
                model
            })
            .collect())
    }

    /// Look up a model by its alias.
    pub async fn get_model(&self, alias: &str) -> Result<Arc<dyn IModel>> {
        if alias.trim().is_empty() {
            return Err(FoundryLocalError::Validation {
                reason: "Model alias must be a non-empty string".into(),
            });
        }
        self.update_models().await?;
        let s = self.lock_state()?;
        s.models_by_alias
            .get(alias)
            .map(|m| {
                let model: Arc<dyn IModel> = m.clone();
                model
            })
            .ok_or_else(|| {
                let available: Vec<&str> = s.models_by_alias.keys().map(|k| k.as_str()).collect();
                FoundryLocalError::ModelOperation {
                    reason: format!("Unknown model alias '{alias}'. Available: {available:?}"),
                }
            })
    }

    /// Look up a specific model variant by its unique id.
    ///
    /// NOTE: This will return an `IModel` representing a single variant. Use
    /// [`get_model`](Catalog::get_model) to obtain an `IModel` with all
    /// available variants.
    pub async fn get_model_variant(&self, id: &str) -> Result<Arc<dyn IModel>> {
        if id.trim().is_empty() {
            return Err(FoundryLocalError::Validation {
                reason: "Variant id must be a non-empty string".into(),
            });
        }
        self.update_models().await?;
        let s = self.lock_state()?;
        s.variants_by_id
            .get(id)
            .map(|v| {
                let variant: Arc<dyn IModel> = v.clone();
                variant
            })
            .ok_or_else(|| {
                let available: Vec<&str> = s.variants_by_id.keys().map(|k| k.as_str()).collect();
                FoundryLocalError::ModelOperation {
                    reason: format!("Unknown variant id '{id}'. Available: {available:?}"),
                }
            })
    }

    /// Return only the model variants that are currently cached on disk.
    pub async fn get_cached_models(&self) -> Result<Vec<Arc<dyn IModel>>> {
        self.update_models().await?;
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
            .filter_map(|id| {
                s.variants_by_id
                    .get(id)
                    .map(|v| {
                        let variant: Arc<dyn IModel> = v.clone();
                        variant
                    })
            })
            .collect())
    }

    /// Return model variants that are currently loaded into memory.
    pub async fn get_loaded_models(&self) -> Result<Vec<Arc<dyn IModel>>> {
        self.update_models().await?;
        let loaded_ids = self.model_load_manager.list_loaded().await?;
        let s = self.lock_state()?;
        Ok(loaded_ids
            .iter()
            .filter_map(|id| {
                s.variants_by_id
                    .get(id)
                    .map(|v| {
                        let variant: Arc<dyn IModel> = v.clone();
                        variant
                    })
            })
            .collect())
    }

    /// Resolve the latest catalog version for the provided model or variant.
    pub async fn get_latest_version(&self, model_or_model_variant: &Arc<dyn IModel>) -> Result<Arc<dyn IModel>> {
        self.update_models().await?;
        let s = self.lock_state()?;

        let model = s
            .models_by_alias
            .get(model_or_model_variant.alias())
            .ok_or_else(|| FoundryLocalError::ModelOperation {
                reason: format!(
                    "Model with alias '{}' not found in catalog.",
                    model_or_model_variant.alias()
                ),
            })?;

        let latest = model
            .variants()
            .into_iter()
            .find(|variant| variant.info().name == model_or_model_variant.info().name)
            .ok_or_else(|| FoundryLocalError::Internal {
                reason: format!(
                    "Mismatch between model (alias:{}) and model variant (alias:{}).",
                    model.alias(),
                    model_or_model_variant.alias()
                ),
            })?;

        if latest.id() == model_or_model_variant.id() {
            Ok(Arc::clone(model_or_model_variant))
        } else {
            Ok(latest)
        }
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

        let mut alias_map_build: HashMap<String, Model> = HashMap::new();
        let mut id_map: HashMap<String, Arc<ModelVariant>> = HashMap::new();

        for info in infos {
            let id = info.id.clone();
            let alias = info.alias.clone();
            let variant = Arc::new(ModelVariant::new(
                info,
                Arc::clone(&self.core),
                Arc::clone(&self.model_load_manager),
                self.invalidator.clone(),
            ));
            id_map.insert(id, Arc::clone(&variant));

            alias_map_build
                .entry(alias)
                .or_insert_with_key(|a| Model::new(a.clone(), Arc::clone(&self.core)))
                .add_variant(variant);
        }

        let alias_map: HashMap<String, Arc<Model>> = alias_map_build
            .into_iter()
            .map(|(k, v)| (k, Arc::new(v)))
            .collect();

        // Atomic swap under a single lock — no split-brain reads.
        let mut s = self.lock_state()?;
        s.models_by_alias = alias_map;
        s.variants_by_id = id_map;
        s.last_refresh = Some(Instant::now());

        Ok(())
    }

    fn lock_state(&self) -> Result<std::sync::MutexGuard<'_, CatalogState>> {
        self.state.lock().map_err(|_| FoundryLocalError::Internal {
            reason: "catalog state mutex poisoned".into(),
        })
    }
}
