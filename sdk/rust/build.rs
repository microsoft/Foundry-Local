use std::collections::HashMap;
use std::env;
use std::fs;
use std::io::{self, Read};
use std::path::{Path, PathBuf};
use std::sync::Mutex;

/// Feeds tried in order. Primary: nuget.org (stable releases). Fallback:
/// the public ORT-Nightly Azure DevOps NuGet feed (where dev / pre-release
/// builds of Foundry Local Core, ONNX Runtime and ONNX Runtime GenAI live
/// before they reach nuget.org). If a download from a feed fails for any
/// reason, the next feed is tried.
const FEEDS: &[&str] = &[
    "https://api.nuget.org/v3/index.json",
    "https://pkgs.dev.azure.com/aiinfra/PublicPackages/_packaging/ORT-Nightly/nuget/v3/index.json",
];

/// Versions loaded from deps_versions.json (or deps_versions_winml.json).
/// Both files share the same key structure — the build script picks the
/// right file based on the winml cargo feature.
struct DepsVersions {
    core: String,
    ort: String,
    genai: String,
}

fn load_deps_versions() -> DepsVersions {
    let winml = env::var("CARGO_FEATURE_WINML").is_ok();
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap_or_default();
    let manifest_path = Path::new(&manifest_dir);

    // Standard and WinML each have their own file with identical key structure.
    let filename = if winml {
        "deps_versions_winml.json"
    } else {
        "deps_versions.json"
    };

    // Check manifest dir first (packaged crate), then parent (repo layout)
    let json_path = if manifest_path.join(filename).exists() {
        manifest_path.join(filename)
    } else {
        manifest_path.join("..").join(filename)
    };

    // Tell Cargo to rebuild if the versions file changes
    println!(
        "cargo:rerun-if-changed={}",
        json_path
            .canonicalize()
            .unwrap_or(json_path.clone())
            .display()
    );

    let content = fs::read_to_string(&json_path).expect("Failed to read deps_versions.json");
    // Strip UTF-8 BOM if present (PowerShell may write files with BOM)
    let stripped_content = content.strip_prefix('\u{FEFF}').unwrap_or(&content);
    let val: serde_json::Value =
        serde_json::from_str(stripped_content).expect("Failed to parse deps_versions.json");

    let s = |obj: &serde_json::Value, key: &str| -> String {
        obj.get(key)
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_string()
    };
    let flc = &val["foundry-local-core"];
    let ort = &val["onnxruntime"];
    let genai = &val["onnxruntime-genai"];
    DepsVersions {
        core: s(flc, "nuget"),
        ort: s(ort, "version"),
        genai: s(genai, "version"),
    }
}

struct NuGetPackage {
    name: &'static str,
    version: String,
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
    let is_linux = rid.starts_with("linux");
    let deps = load_deps_versions();

    // Use pinned versions directly — dynamic resolution via resolve_latest_version
    // is unreliable (feed returns versions in unexpected order, and some old versions
    // require authentication).

    let mut packages = Vec::new();

    if winml {
        packages.push(NuGetPackage {
            name: "Microsoft.AI.Foundry.Local.Core.WinML",
            version: deps.core.clone(),
        });
        packages.push(NuGetPackage {
            name: "Microsoft.ML.OnnxRuntime.Foundry",
            version: deps.ort.clone(),
        });
        packages.push(NuGetPackage {
            name: "Microsoft.ML.OnnxRuntimeGenAI.Foundry",
            version: deps.genai.clone(),
        });
    } else {
        packages.push(NuGetPackage {
            name: "Microsoft.AI.Foundry.Local.Core",
            version: deps.core.clone(),
        });

        if is_linux {
            packages.push(NuGetPackage {
                name: "Microsoft.ML.OnnxRuntime.Gpu.Linux",
                version: deps.ort.clone(),
            });
        } else {
            packages.push(NuGetPackage {
                name: "Microsoft.ML.OnnxRuntime.Foundry",
                version: deps.ort.clone(),
            });
        }

        packages.push(NuGetPackage {
            name: "Microsoft.ML.OnnxRuntimeGenAI.Foundry",
            version: deps.genai.clone(),
        });
    }

    packages
}

/// Resolve the PackageBaseAddress from a NuGet v3 service index. The result
/// is cached per feed URL so repeated calls within a single build (e.g. one
/// per package, plus retries on fallback feeds) only hit the network once.
fn resolve_base_address(feed_url: &str) -> Result<String, String> {
    static BASE_ADDRESS_CACHE: Mutex<Option<HashMap<String, String>>> = Mutex::new(None);
    {
        let guard = BASE_ADDRESS_CACHE.lock().unwrap();
        if let Some(map) = guard.as_ref() {
            if let Some(cached) = map.get(feed_url) {
                return Ok(cached.clone());
            }
        }
    }

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
                let mut guard = BASE_ADDRESS_CACHE.lock().unwrap();
                guard
                    .get_or_insert_with(HashMap::new)
                    .insert(feed_url.to_string(), base.clone());
                return Ok(base);
            }
        }
    }

    Err(format!(
        "Could not find PackageBaseAddress/3.0.0 in feed {feed_url}"
    ))
}

/// Try to download and extract a single package from a specific feed. Returns
/// `Ok(())` on success, `Err(reason)` on any failure (network, HTTP error,
/// zip parse error, etc.).
fn try_download_from_feed(
    pkg: &NuGetPackage,
    rid: &str,
    out_dir: &Path,
    feed_url: &str,
) -> Result<(), String> {
    let base_address = resolve_base_address(feed_url)?;
    let lower_name = pkg.name.to_lowercase();
    let lower_version = pkg.version.to_lowercase();
    let url =
        format!("{base_address}{lower_name}/{lower_version}/{lower_name}.{lower_version}.nupkg");

    let feed_host = feed_url
        .split("://")
        .nth(1)
        .and_then(|s| s.split('/').next())
        .unwrap_or(feed_url);

    println!(
        "cargo:warning=Downloading {name} {ver} from {host}",
        name = pkg.name,
        ver = pkg.version,
        host = feed_host,
    );

    let mut response = ureq::get(&url)
        .call()
        .map_err(|e| format!("Failed to download {} from {feed_host}: {e}", pkg.name))?;

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

/// Download a .nupkg and extract native libraries for the given RID into `out_dir`.
/// Skips download if native files from this package are already present.
/// Tries each configured feed in order; on failure falls back to the next.
fn download_and_extract(pkg: &NuGetPackage, rid: &str, out_dir: &Path) -> Result<(), String> {
    // Skip if this package's main native library is already in out_dir
    // (e.g. pre-populated from FOUNDRY_NATIVE_OVERRIDE_DIR).
    let ext = native_lib_extension();
    let prefix = if env::consts::OS == "windows" {
        ""
    } else {
        "lib"
    };
    let expected_file = if pkg.name.contains("Foundry.Local.Core") {
        format!("Microsoft.AI.Foundry.Local.Core.{ext}")
    } else if pkg.name.contains("OnnxRuntimeGenAI") {
        format!("{prefix}onnxruntime-genai.{ext}")
    } else if pkg.name.contains("OnnxRuntime") {
        format!("{prefix}onnxruntime.{ext}")
    } else {
        String::new()
    };
    if !expected_file.is_empty() && out_dir.join(&expected_file).exists() {
        println!(
            "cargo:warning={} already present, skipping download.",
            pkg.name
        );
        return Ok(());
    }

    let mut last_error = String::new();
    for (i, feed_url) in FEEDS.iter().enumerate() {
        match try_download_from_feed(pkg, rid, out_dir, feed_url) {
            Ok(()) => return Ok(()),
            Err(e) => {
                let is_last = i == FEEDS.len() - 1;
                if !is_last {
                    println!(
                        "cargo:warning={} {}: {e}; trying next feed...",
                        pkg.name, pkg.version
                    );
                }
                last_error = e;
            }
        }
    }
    Err(format!(
        "Failed to download {} {} from any configured feed: {last_error}",
        pkg.name, pkg.version
    ))
}

/// Check whether all required native libraries are already present in `out_dir`.
fn libs_already_present(out_dir: &Path) -> bool {
    let ext = native_lib_extension();
    let prefix = if env::consts::OS == "windows" {
        ""
    } else {
        "lib"
    };
    let required = [
        format!("Microsoft.AI.Foundry.Local.Core.{ext}"),
        format!("{prefix}onnxruntime.{ext}"),
        format!("{prefix}onnxruntime-genai.{ext}"),
    ];
    required.iter().all(|f| out_dir.join(f).exists())
}

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-env-changed=FOUNDRY_NATIVE_OVERRIDE_DIR");
    println!("cargo:rerun-if-env-changed=CARGO_FEATURE_WINML");

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

    // If FOUNDRY_NATIVE_OVERRIDE_DIR is set (e.g. by CI), copy all native
    // libraries from that directory into OUT_DIR. This pre-populates FLC Core
    // binaries that aren't published to a feed yet. The download loop below
    // will then only fetch packages whose files are still missing (ORT, GenAI).
    if let Ok(override_dir) = env::var("FOUNDRY_NATIVE_OVERRIDE_DIR") {
        let src = Path::new(&override_dir);
        if src.is_dir() {
            let ext = native_lib_extension();
            for entry in fs::read_dir(src).expect("Failed to read FOUNDRY_NATIVE_OVERRIDE_DIR") {
                let path = entry.expect("Failed to read dir entry").path();
                if path.extension().and_then(|e| e.to_str()) == Some(ext) {
                    let dest = out_dir.join(path.file_name().unwrap());
                    fs::copy(&path, &dest).expect("Failed to copy native lib from override dir");
                    println!(
                        "cargo:warning=Copied {} from override dir",
                        path.file_name().unwrap().to_string_lossy()
                    );
                }
            }
        }
    }

    // Skip all downloads if every required library is already present
    if libs_already_present(&out_dir) {
        println!("cargo:warning=Native libraries already present in OUT_DIR, skipping download.");
        println!("cargo:rustc-link-search=native={}", out_dir.display());
        println!("cargo:rustc-env=FOUNDRY_NATIVE_DIR={}", out_dir.display());
        #[cfg(windows)]
        println!("cargo:rustc-link-lib=kernel32");
        return;
    }

    let packages = get_packages(rid);

    let mut download_failed = false;
    for pkg in &packages {
        if let Err(e) = download_and_extract(pkg, rid, &out_dir) {
            println!("cargo:warning=Error downloading {}: {e}", pkg.name);
            download_failed = true;
        }
    }

    if download_failed && !libs_already_present(&out_dir) {
        panic!(
            "One or more native library downloads failed and required libraries are missing. \
             You can manually place native libraries in the output directory: {}",
            out_dir.display()
        );
    }

    println!("cargo:rustc-link-search=native={}", out_dir.display());
    println!("cargo:rustc-env=FOUNDRY_NATIVE_DIR={}", out_dir.display());

    // LocalFree (used to free native-allocated buffers) lives in kernel32.lib on Windows.
    #[cfg(windows)]
    println!("cargo:rustc-link-lib=kernel32");
}
