use anyhow::{anyhow, Result};
use log::{info, warn};
use regex::Regex;
use std::{
    process::{Command, Stdio},
    thread,
    time::Duration,
};
use which::which;

/// Assert that Foundry is installed.
///
/// # Errors
///
/// Returns an error if Foundry is not installed or not on PATH.
pub fn assert_foundry_installed() -> Result<()> {
    which("foundry").map_err(|_| anyhow!("Foundry is not installed or not on PATH!"))?;
    Ok(())
}

/// Get the service URI if the service is running.
///
/// # Returns
///
/// URI of the running Foundry service, or None if not running.
pub fn get_service_uri() -> Option<String> {
    let output = Command::new("foundry")
        .args(["service", "status"])
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .output()
        .ok()?;

    let stdout = String::from_utf8_lossy(&output.stdout);
    let re = Regex::new(r"http://(?:[a-zA-Z0-9.-]+|\d{1,3}(\.\d{1,3}){3}):\d+").ok()?;

    re.find(&stdout).map(|m| m.as_str().to_string())
}

/// Start the Foundry service.
///
/// # Returns
///
/// URI of the started Foundry service, or None if it failed to start.
pub fn start_service() -> Result<String> {
    if let Some(service_url) = get_service_uri() {
        info!("Foundry service is already running at {}", service_url);
        return Ok(service_url);
    }

    // Start the service in the background
    let _child = Command::new("foundry").args(["service", "start"]).spawn()?;

    // Check for service availability
    for _ in 0..10 {
        if let Some(service_url) = get_service_uri() {
            info!("Foundry service started successfully at {}", service_url);
            return Ok(service_url);
        }
        thread::sleep(Duration::from_millis(100));
    }

    warn!("Foundry service did not start within the expected time. May not be running.");
    Err(anyhow!("Failed to start Foundry service"))
}
