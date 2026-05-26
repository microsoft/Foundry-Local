// CI-only. Stages the foundry_local shared library into prebuilds/ for npm
// publish. ORT / ORT-GenAI / WinML / WindowsAppRuntime are NOT bundled — the
// user supplies them at runtime per the legacy libraryPath contract (see
// ort-loading-contract.instructions.md). The .node addon itself is already
// produced into prebuilds/<plat>-<arch>/ by `node-gyp rebuild`.
import { copyFileSync, existsSync, mkdirSync, readdirSync, statSync } from "node:fs";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = fileURLToPath(new URL(".", import.meta.url));
const pkgRoot = resolve(here, "..");
const repoRoot = resolve(pkgRoot, "..", "..");

const config = process.env.FOUNDRY_LOCAL_CPP_CONFIG ?? "RelWithDebInfo";

const platformSegment = (() => {
  switch (process.platform) {
    case "win32":
      return "Windows";
    case "linux":
      return "Linux";
    case "darwin":
      return "macOS";
    default:
      throw new Error(`Unsupported platform: ${process.platform}`);
  }
})();

const sourceDir = resolve(repoRoot, "sdk_v2", "cpp", "build", platformSegment, config, "bin", config);

if (!existsSync(sourceDir)) {
  console.error(`[pack-prebuilds] source directory not found: ${sourceDir}`);
  console.error("[pack-prebuilds] Build the C++ SDK first:");
  console.error(`[pack-prebuilds]   python sdk_v2/cpp/build.py --configure --build --config ${config}`);
  process.exit(1);
}

const destDir = resolve(pkgRoot, "prebuilds", `${process.platform}-${process.arch}`);
mkdirSync(destDir, { recursive: true });

// Only foundry_local — ORT siblings are explicitly excluded.
const wanted = (() => {
  if (process.platform === "win32") return ["foundry_local.dll"];
  if (process.platform === "darwin") return ["libfoundry_local.dylib"];
  return ["libfoundry_local.so"];
})();

let copied = 0;
const available = new Set(readdirSync(sourceDir));
for (const file of wanted) {
  if (!available.has(file)) {
    console.error(`[pack-prebuilds] required file not found in source: ${file}`);
    process.exit(1);
  }
  const src = resolve(sourceDir, file);
  const dst = resolve(destDir, file);
  copyFileSync(src, dst);
  const size = statSync(dst).size;
  console.log(`[pack-prebuilds] ${file} (${size} bytes)`);
  copied += 1;
}

console.log(`[pack-prebuilds] Copied ${copied} file(s) to ${destDir}`);
