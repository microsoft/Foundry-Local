// Print the absolute prebuilds/<platform>-<arch>/ directory where the
// .node addon is copied after build. node-gyp invokes this via `<!(node ...)`.
import { mkdirSync } from "node:fs";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = fileURLToPath(new URL(".", import.meta.url));
const pkgRoot = resolve(here, "..", "..");
const prebuildDir = resolve(pkgRoot, "prebuilds", `${process.platform}-${process.arch}`);

// node-gyp's copies action requires the destination to exist before we get here.
mkdirSync(prebuildDir, { recursive: true });

process.stdout.write(prebuildDir);
