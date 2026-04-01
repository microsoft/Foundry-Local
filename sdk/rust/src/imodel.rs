//! Public model type — re-exported from the internal `detail::model` module.
//!
//! All public APIs return [`Arc<Model>`] so that callers never need to
//! reference internal types directly.

pub use crate::detail::model::Model;
