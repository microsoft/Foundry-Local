use super::common;

const HF_URL: &str = "https://huggingface.co/onnxruntime/Phi-3-mini-4k-instruct-onnx/tree/main/cpu_and_mobile/cpu-int4-rtn-block-32-acc-level-4";

#[tokio::test]
async fn should_create_huggingface_catalog() {
    let manager = common::get_test_manager();
    let hf_catalog = manager
        .add_catalog("https://huggingface.co", None)
        .await
        .expect("add_catalog failed");
    assert_eq!(hf_catalog.name(), "HuggingFace");
}

#[tokio::test]
async fn should_reject_non_huggingface_url() {
    let manager = common::get_test_manager();
    let result = manager.add_catalog("https://example.com", None).await;
    assert!(result.is_err(), "Non-HuggingFace URL should be rejected");
}

#[tokio::test]
async fn should_register_model() {
    let manager = common::get_test_manager();
    let hf_catalog = manager
        .add_catalog("https://huggingface.co", None)
        .await
        .expect("add_catalog failed");

    let model = hf_catalog
        .register_model(HF_URL)
        .await
        .expect("register_model failed");

    assert!(!model.alias().is_empty(), "Model alias should be non-empty");
    assert!(!model.id().is_empty(), "Model id should be non-empty");
}

#[tokio::test]
async fn should_find_registered_model_by_identifier() {
    let manager = common::get_test_manager();
    let hf_catalog = manager
        .add_catalog("https://huggingface.co", None)
        .await
        .expect("add_catalog failed");

    let _model = hf_catalog
        .register_model(HF_URL)
        .await
        .expect("register_model failed");

    let found = hf_catalog
        .get_model(HF_URL)
        .await
        .expect("get_model failed");

    assert!(!found.alias().is_empty(), "Should find model by HuggingFace URL");
}

#[tokio::test]
async fn should_register_then_download_model() {
    let manager = common::get_test_manager();
    let hf_catalog = manager
        .add_catalog("https://huggingface.co", None)
        .await
        .expect("add_catalog failed");

    let registered = hf_catalog
        .register_model(HF_URL)
        .await
        .expect("register_model failed");

    assert!(!registered.alias().is_empty());

    // Now download the ONNX files
    registered
        .download::<fn(&str)>(None)
        .await
        .expect("download failed");
}

#[tokio::test]
async fn should_reject_registration_of_plain_alias() {
    let manager = common::get_test_manager();
    let hf_catalog = manager
        .add_catalog("https://huggingface.co", None)
        .await
        .expect("add_catalog failed");

    let result = hf_catalog.register_model("phi-3-mini").await;
    assert!(result.is_err(), "Plain alias should be rejected");
}

#[tokio::test]
async fn should_list_registered_models() {
    let manager = common::get_test_manager();
    let hf_catalog = manager
        .add_catalog("https://huggingface.co", None)
        .await
        .expect("add_catalog failed");

    let _model = hf_catalog
        .register_model(HF_URL)
        .await
        .expect("register_model failed");

    let models = hf_catalog
        .get_models()
        .await
        .expect("get_models failed");

    assert!(
        !models.is_empty(),
        "Should have at least one registered model"
    );
}
