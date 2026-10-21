import { EmbeddingsSession, FoundryLocalManager, Item, Request } from "foundry-local-sdk";

const modelAlias = process.argv[2] ?? "qwen3-embedding-0.6b";
const inputs = [
  "The quick brown fox jumps over the lazy dog.",
  "Local inference keeps data on the device.",
  "Paris is the capital of France.",
];

const manager = FoundryLocalManager.create({
  appName: "foundry_local_samples",
  logLevel: "info",
});

let model;
let session;
let modelLoaded = false;

try {
  model = await manager.catalog.getModel(modelAlias);
  if (!model.isCached) {
    console.log(`Downloading ${model.alias}...`);
    await model.download();
  }
  await model.load();
  modelLoaded = true;

  session = new EmbeddingsSession(model);
  const request = new Request();
  for (const input of inputs) {
    request.addItem(Item.text(input));
  }

  const response = await session.processRequest(request);
  for (const [index, item] of response.output.entries()) {
    if (item.type !== "tensor" || item.dataType !== "float") {
      throw new Error(`Expected float tensor output at index ${index}.`);
    }
    const bytes = item.data.slice();
    const values = new Float32Array(bytes.buffer, bytes.byteOffset, bytes.byteLength / Float32Array.BYTES_PER_ELEMENT);
    console.log(`[${index}] dimensions=${values.length}, first five=${Array.from(values.slice(0, 5)).join(", ")}`);
  }
} finally {
  session?.dispose();
  try {
    if (model !== undefined && modelLoaded) {
      await model.unload();
    }
  } finally {
    manager.dispose();
  }
}
