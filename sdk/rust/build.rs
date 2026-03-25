use std::env;
use std::fs;
use std::io::{self, Read};
use std::path::{Path, PathBuf};

const NUGET_FEED: &str = "https://api.nuget.org/v3/index.json";
const ORT_NIGHTLY_FEED: &str =
    "https://pkgs.dev.azure.com/aiinfra/PublicPackages/_packaging/ORT-Nightly/nuget/v3/index.json";

const CORE_VERSION: &str = "0.9.0-dev-20260325T055742-33ebe7c";
const ORT_VERSION: &str = "1.24.3";
const GENAI_VERSION: &str = "0.13.0-dev-20260319-1131106-439ca0d5";

const WINML_ORT_VERSION: &str = "1.23.2.3";

struct NuGetPackage {
    name: &'static str,
    version: String,
    feed_url: &'static str,
}

fn get_rid() -> Option<&'static str> {
    let os = env::consts::OS;
    let arch = env::consts::ARCH;
    match (os, arch) {
        ("windows", "x86_64") => Some("win-x64"),
        ("windows", "aarch64") => Some("win-arm64"),
        ("linux", "x86_64") => Some("linux-x64"),
        ("macos", "aarch64") => Some("osx-arm64"),
        _ => None,
    }
}

fn native_lib_extension() -> &'static str {
    match env::consts::OS {
        "windows" => "dll",
        "linux" => "so",
        "macos" => "dylib",
        _ => "so",
    }
}

fn get_packages(rid: &str) -> Vec<NuGetPackage> {
    let winml = env::var("CARGO_FEATURE_WINML").is_ok();
    let nightly = env::var("CARGO_FEATURE_NIGHTLY").is_ok();
    let is_linux = rid.starts_with("linux");

    let core_version = if nightly {
        resolve_latest_version("Microsoft.AI.Foundry.Local.Core", ORT_NIGHTLY_FEED)
            .unwrap_or_else(|| CORE_VERSION.to_string())
    } else {
        CORE_VERSION.to_string()
    };

    let mut packages = Vec::new();

    if winml {
        let winml_core_version = if nightly {
            resolve_latest_version("Microsoft.AI.Foundry.Local.Core.WinML", ORT_NIGHTLY_FEED)
                .unwrap_or_else(|| CORE_VERSION.to_string())
        } else {
            CORE_VERSION.to_string()
        };

        packages.push(NuGetPackage {
            name: "Microsoft.AI.Foundry.Local.Core.WinML",
            version: winml_core_version,
            feed_url: ORT_NIGHTLY_FEED,
        });
        packages.push(NuGetPackage {
            name: "Microsoft.ML.OnnxRuntime.Foundry",
            version: WINML_ORT_VERSION.to_string(),
            feed_url: NUGET_FEED,
        });
        let winml_genai_version = if nightly {
            resolve_latest_version("Microsoft.ML.OnnxRuntimeGenAI.WinML", ORT_NIGHTLY_FEED)
                .unwrap_or_else(|| GENAI_VERSION.to_string())
        } else {
            GENAI_VERSION.to_string()
        };

        packages.push(NuGetPackage {
            name: "Microsoft.ML.OnnxRuntimeGenAI.WinML",
            version: winml_genai_version,
            feed_url: ORT_NIGHTLY_FEED,
        });
    } else {
        packages.push(NuGetPackage {
            name: "Microsoft.AI.Foundry.Local.Core",
            version: core_version,
            feed_url: ORT_NIGHTLY_FEED,
        });

        if is_linux {
            packages.push(NuGetPackage {
                name: "Microsoft.ML.OnnxRuntime.Gpu.Linux",
                version: ORT_VERSION.to_string(),
                feed_url: NUGET_FEED,
            });
        } else {
            packages.push(NuGetPackage {
                name: "Microsoft.ML.OnnxRuntime.Foundry",
                version: ORT_VERSION.to_string(),
                feed_url: NUGET_FEED,
            });
        }

        let genai_version = if nightly {
            resolve_latest_version("Microsoft.ML.OnnxRuntimeGenAI.Foundry", ORT_NIGHTLY_FEED)
                .unwrap_or_else(|| GENAI_VERSION.to_string())
        } else {
            GENAI_VERSION.to_string()
        };

        packages.push(NuGetPackage {
            name: "Microsoft.ML.OnnxRuntimeGenAI.Foundry",
            version: genai_version,
            feed_url: ORT_NIGHTLY_FEED,
        });
    }

    packages
}

/// Resolve the PackageBaseAddress from a NuGet v3 service index.
fn resolve_base_address(feed_url: &str) -> Result<String, String> {
    let body: String = ureq::get(feed_url)
        .call()
        .map_err(|e| format!("Failed to fetch NuGet feed index at {feed_url}: {e}"))?
        .body_mut()
        .read_to_string()
        .map_err(|e| format!("Failed to read feed index response: {e}"))?;

    let index: serde_json::Value =
        serde_json::from_str(&body).map_err(|e| format!("Failed to parse feed index JSON: {e}"))?;

    let resources = index["resources"]
        .as_array()
        .ok_or("Feed index missing 'resources' array")?;

    for resource in resources {
        let rtype = resource["@type"].as_str().unwrap_or("");
        if rtype == "PackageBaseAddress/3.0.0" {
            if let Some(id) = resource["@id"].as_str() {
                let base = if id.ends_with('/') {
                    id.to_string()
                } else {
                    format!("{id}/")
                };
                return Ok(base);
            }
        }
    }

    Err(format!(
        "Could not find PackageBaseAddress/3.0.0 in feed {feed_url}"
    ))
}

/// Resolve the latest version of a package from a NuGet feed.
fn resolve_latest_version(package_name: &str, feed_url: &str) -> Option<String> {
    let base_address = resolve_base_address(feed_url).ok()?;
    let lower_name = package_name.to_lowercase();
    let index_url = format!("{base_address}{lower_name}/index.json");

    let body: String = ureq::get(&index_url)
        .call()
        .ok()?
        .body_mut()
        .read_to_string()
        .ok()?;

    let index: serde_json::Value = serde_json::from_str(&body).ok()?;
    let versions = index["versions"].as_array()?;
    versions.last()?.as_str().map(|s| s.to_string())
}

/// Download a .nupkg and extract native libraries for the given RID into `out_dir`.
fn download_and_extract(pkg: &NuGetPackage, rid: &str, out_dir: &Path) -> Result<(), String> {
    let base_address = resolve_base_address(pkg.feed_url)?;
    let lower_name = pkg.name.to_lowercase();
    let lower_version = pkg.version.to_lowercase();
    let url =
        format!("{base_address}{lower_name}/{lower_version}/{lower_name}.{lower_version}.nupkg");

    println!(
        "cargo:warning=Downloading {name} {ver} from {feed}",
        name = pkg.name,
        ver = pkg.version,
        feed = if pkg.feed_url == NUGET_FEED {
            "NuGet.org"
        } else {
            "ORT-Nightly"
        },
    );

    let mut response = ureq::get(&url)
        .call()
        .map_err(|e| format!("Failed to download {}: {e}", pkg.name))?;

    let mut bytes = Vec::new();
    response
        .body_mut()
        .as_reader()
        .read_to_end(&mut bytes)
        .map_err(|e| format!("Failed to read response body for {}: {e}", pkg.name))?;

    let ext = native_lib_extension();
    let prefix = format!("runtimes/{rid}/native/");

    let cursor = io::Cursor::new(&bytes);
    let mut archive = zip::ZipArchive::new(cursor)
        .map_err(|e| format!("Failed to open nupkg as zip for {}: {e}", pkg.name))?;

    let mut extracted = 0usize;
    for i in 0..archive.len() {
        let mut file = archive
            .by_index(i)
            .map_err(|e| format!("Failed to read zip entry: {e}"))?;

        let name = file.name().to_string();
        if !name.starts_with(&prefix) {
            continue;
        }
        if !name.ends_with(&format!(".{ext}")) {
            continue;
        }

        let file_name = Path::new(&name)
            .file_name()
            .map(|n| n.to_string_lossy().to_string())
            .unwrap_or_default();

        if file_name.is_empty() {
            continue;
        }

        let dest = out_dir.join(&file_name);
        let mut out_file = fs::File::create(&dest)
            .map_err(|e| format!("Failed to create {}: {e}", dest.display()))?;
        io::copy(&mut file, &mut out_file)
            .map_err(|e| format!("Failed to write {}: {e}", dest.display()))?;

        println!("cargo:warning=  Extracted {file_name}");
        extracted += 1;
    }

    if extracted == 0 {
        println!(
            "cargo:warning=  No native libraries found for RID '{rid}' in {} {}",
            pkg.name, pkg.version
        );
    }

    Ok(())
}

/// Check whether the core native library is already present in `out_dir`.
fn libs_already_present(out_dir: &Path) -> bool {
    let core_lib = match env::consts::OS {
        "windows" => "Microsoft.AI.Foundry.Local.Core.dll",
        "linux" => "libMicrosoft.AI.Foundry.Local.Core.so",
        "macos" => "libMicrosoft.AI.Foundry.Local.Core.dylib",
        _ => return false,
    };
    out_dir.join(core_lib).exists()
}

fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR not set"));

    let rid = match get_rid() {
        Some(r) => r,
        None => {
            println!(
                "cargo:warning=Unsupported platform: {} {}. Native libraries will not be downloaded.",
                env::consts::OS,
                env::consts::ARCH,
            );
            return;
        }
    };

    // Skip download if libraries already exist
    if libs_already_present(&out_dir) {
        println!("cargo:warning=Native libraries already present in OUT_DIR, skipping download.");
        println!("cargo:rustc-link-search=native={}", out_dir.display());
        println!("cargo:rustc-env=FOUNDRY_NATIVE_DIR={}", out_dir.display());
        #[cfg(windows)]
        println!("cargo:rustc-link-lib=kernel32");
        return;
    }

    let packages = get_packages(rid);

    for pkg in &packages {
        if let Err(e) = download_and_extract(pkg, rid, &out_dir) {
            println!("cargo:warning=Error downloading {}: {e}", pkg.name);
            println!("cargo:warning=Build will continue, but runtime loading may fail.");
            println!(
                "cargo:warning=You can manually place native libraries in the output directory."
            );
        }
    }

    println!("cargo:rustc-link-search=native={}", out_dir.display());
    println!("cargo:rustc-env=FOUNDRY_NATIVE_DIR={}", out_dir.display());

    // LocalFree (used to free native-allocated buffers) lives in kernel32.lib on Windows.
    #[cfg(windows)]
    println!("cargo:rustc-link-lib=kernel32");
}
