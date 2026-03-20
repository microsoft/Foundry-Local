//! Shared HuggingFace URL utilities.

/// Normalise a model identifier to a canonical HuggingFace URL, or return
/// `None` if it is a plain alias.
///
/// Strips `/tree/{revision}/` from full browser URLs so the result matches
/// the stored `ModelInfo.uri` format.
///
/// # Examples
///
/// ```text
/// "https://huggingface.co/org/repo/tree/main/sub" → Some("https://huggingface.co/org/repo/sub")
/// "https://huggingface.co/org/repo"               → Some("https://huggingface.co/org/repo")
/// "org/repo/sub"                                   → Some("https://huggingface.co/org/repo/sub")
/// "phi-3-mini" (plain alias)                       → None
/// ```
pub(crate) fn normalize_to_huggingface_url(input: &str) -> Option<String> {
    const HF_PREFIX: &str = "https://huggingface.co/";

    if input.to_lowercase().starts_with(&HF_PREFIX.to_lowercase()) {
        let path = &input[HF_PREFIX.len()..];
        let parts: Vec<&str> = path.split('/').collect();
        if parts.len() >= 4 && parts[2].eq_ignore_ascii_case("tree") {
            let org = parts[0];
            let repo = parts[1];
            let sub: Vec<&str> = parts[4..].to_vec();
            return if sub.is_empty() {
                Some(format!("{HF_PREFIX}{org}/{repo}"))
            } else {
                Some(format!("{HF_PREFIX}{org}/{repo}/{}", sub.join("/")))
            };
        }
        return Some(input.to_string());
    }

    if input.contains('/') && !input.to_lowercase().starts_with("azureml://") {
        let parts: Vec<&str> = input.split('/').collect();
        if parts.len() >= 4 && parts[2].eq_ignore_ascii_case("tree") {
            let org = parts[0];
            let repo = parts[1];
            let sub: Vec<&str> = parts[4..].to_vec();
            return if sub.is_empty() {
                Some(format!("{HF_PREFIX}{org}/{repo}"))
            } else {
                Some(format!("{HF_PREFIX}{org}/{repo}/{}", sub.join("/")))
            };
        }
        return Some(format!("{HF_PREFIX}{input}"));
    }

    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn plain_alias_returns_none() {
        assert!(normalize_to_huggingface_url("phi-3-mini").is_none());
    }

    #[test]
    fn org_repo_returns_hf_url() {
        assert_eq!(
            normalize_to_huggingface_url("microsoft/Phi-3-mini-4k-instruct-onnx"),
            Some("https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx".into())
        );
    }

    #[test]
    fn full_hf_url_passthrough() {
        let url = "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx";
        assert_eq!(normalize_to_huggingface_url(url), Some(url.into()));
    }

    #[test]
    fn browser_url_strips_tree_revision() {
        let input = "https://huggingface.co/org/repo/tree/main/sub/path";
        assert_eq!(
            normalize_to_huggingface_url(input),
            Some("https://huggingface.co/org/repo/sub/path".into())
        );
    }

    #[test]
    fn browser_url_no_subpath() {
        let input = "https://huggingface.co/org/repo/tree/main";
        assert_eq!(
            normalize_to_huggingface_url(input),
            Some("https://huggingface.co/org/repo".into())
        );
    }

    #[test]
    fn bare_identifier_with_tree_revision() {
        let input = "org/repo/tree/main/sub";
        assert_eq!(
            normalize_to_huggingface_url(input),
            Some("https://huggingface.co/org/repo/sub".into())
        );
    }

    #[test]
    fn org_repo_with_subpath() {
        let input = "org/repo/sub/path";
        assert_eq!(
            normalize_to_huggingface_url(input),
            Some("https://huggingface.co/org/repo/sub/path".into())
        );
    }

    #[test]
    fn azureml_url_returns_none() {
        assert!(normalize_to_huggingface_url("azureml://some/model").is_none());
    }
}
