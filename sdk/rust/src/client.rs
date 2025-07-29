use std::time::Duration;

use anyhow::Result;
use indicatif::{ProgressBar, ProgressStyle};
use log::{debug, error};
use regex::Regex;
use reqwest::{Client, Response, StatusCode};
use serde::de::DeserializeOwned;
use serde_json::Value;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum ClientError {
    #[error("HTTP response error: {0} - {1}")]
    HttpResponseError(StatusCode, String),

    #[error("Connection error: {0}")]
    ConnectionError(String),

    #[error("JSON parsing error: {0}")]
    JsonError(#[from] serde_json::Error),

    #[error("Request error: {0}")]
    RequestError(#[from] reqwest::Error),

    #[error("{0}")]
    Other(String),
}

/// Client for Foundry Local SDK.
pub struct HttpClient {
    client: Client,
    base_url: String,
}

impl HttpClient {
    /// Create a new HTTP client for the Foundry Local SDK.
    ///
    /// # Arguments
    ///
    /// * `host` - Base URL of the host.
    /// * `timeout` - Optional timeout for the HTTP client in seconds.
    ///
    /// # Returns
    ///
    /// A new HttpClient instance.
    pub fn new(host: &str, timeout_secs: Option<u64>) -> Self {
        let timeout = timeout_secs.map(Duration::from_secs);
        let mut client_builder = Client::builder().user_agent("foundry-local-rust-sdk/0.2.0");

        if let Some(timeout) = timeout {
            client_builder = client_builder.timeout(timeout);
        }

        let client = client_builder.build().expect("Failed to build HTTP client");

        Self {
            client,
            base_url: host.to_string(),
        }
    }

    /// Send a GET request to the specified path with optional query parameters.
    ///
    /// # Arguments
    ///
    /// * `path` - Path for the GET request.
    /// * `query_params` - Optional query parameters for the request.
    ///
    /// # Returns
    ///
    /// JSON response or None if no content.
    pub async fn get<T: DeserializeOwned>(
        &self,
        path: &str,
        query_params: Option<&[(&str, &str)]>,
    ) -> Result<Option<T>, ClientError> {
        let url = format!("{}{}", self.base_url, path);
        debug!("GET {url}");

        let mut request_builder = self.client.get(&url);
        if let Some(params) = query_params {
            request_builder = request_builder.query(params);
        }

        let response = request_builder.send().await.map_err(|e| {
            if e.is_connect() {
                ClientError::ConnectionError(
                    "Could not connect to Foundry Local! Please check if the Foundry Local service is running and the host URL is correct."
                    .to_string()
                )
            } else {
                ClientError::RequestError(e)
            }
        })?;

        self.handle_response(response).await
    }

    /// Send a POST request to the specified path with optional request body and show progress.
    ///
    /// # Arguments
    ///
    /// * `path` - Path for the POST request.
    /// * `body` - Optional request body in JSON format.
    ///
    /// # Returns
    ///
    /// JSON response.
    pub async fn post_with_progress<T: DeserializeOwned>(
        &self,
        path: &str,
        body: Option<Value>,
    ) -> Result<T, ClientError> {
        let url = format!("{}{}", self.base_url, path);
        debug!("POST with progress: {url}");

        let mut request_builder = self.client.post(&url);
        if let Some(ref json_body) = body {
            request_builder = request_builder.json(json_body);
        }

        let response = request_builder.send().await.map_err(|e| {
            if e.is_connect() {
                ClientError::ConnectionError(
                    "Could not connect to Foundry Local! Please check if the Foundry Local service is running and the host URL is correct."
                    .to_string()
                )
            } else {
                ClientError::RequestError(e)
            }
        })?;

        let pb = ProgressBar::new(100);
        pb.set_style(
            ProgressStyle::default_bar()
                .template("[{elapsed_precise}] [{wide_bar:.cyan/blue}] {percent}%")
                .unwrap()
                .progress_chars("█▉▊▋▌▍▎▏  "),
        );

        let mut final_json = String::new();
        let status = response.status();
        let mut stream = response.bytes_stream();

        use futures_util::StreamExt;
        let re = Regex::new(r"(\d+(?:\.\d+)?)%").unwrap();
        let mut prev_percent = 0.0;

        while let Some(chunk_result) = stream.next().await {
            match chunk_result {
                Ok(chunk) => {
                    let chunk_str = String::from_utf8_lossy(&chunk);

                    if chunk_str.starts_with("{") || !final_json.is_empty() {
                        final_json.push_str(&chunk_str);
                    } else if let Some(captures) = re.captures(&chunk_str) {
                        if let Some(percent_match) = captures.get(1) {
                            if let Ok(percent) = percent_match.as_str().parse::<f64>() {
                                if percent > prev_percent {
                                    pb.set_position((percent) as u64);
                                    prev_percent = percent;
                                }
                            }
                        }
                    }
                }
                Err(e) => {
                    error!("Error reading response chunk: {e}");
                    return Err(ClientError::RequestError(e));
                }
            }
        }

        pb.finish_and_clear();

        if !status.is_success() {
            return Err(ClientError::HttpResponseError(status, final_json));
        }

        if final_json.is_empty() {
            return Err(ClientError::Other("Empty response body".to_string()));
        }

        if !final_json.ends_with("}") {
            return Err(ClientError::Other(format!(
                "Invalid JSON response: {final_json}"
            )));
        }

        serde_json::from_str(&final_json).map_err(ClientError::JsonError)
    }

    async fn handle_response<T: DeserializeOwned>(
        &self,
        response: Response,
    ) -> Result<Option<T>, ClientError> {
        let status = response.status();
        if !status.is_success() {
            let text = response
                .text()
                .await
                .unwrap_or_else(|_| "No response text".to_string());
            return Err(ClientError::HttpResponseError(status, text));
        }

        let text = response.text().await.map_err(ClientError::RequestError)?;
        if text.is_empty() {
            return Ok(None);
        }

        let parsed = serde_json::from_str(&text)?;
        Ok(Some(parsed))
    }
}
