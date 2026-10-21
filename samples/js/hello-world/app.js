import { ChatSession, FoundryLocalManager, Item, Request } from "foundry-local-sdk";

const requestedAlias = process.argv[2] ?? "qwen2.5-0.5b";
const manager = FoundryLocalManager.create({
  appName: "foundry_local_samples",
  logLevel: "info",
});

let model;
let session;
let modelLoaded = false;

try {
  console.log("Execution providers:");
  const providers = manager.discoverEps();
  for (const provider of providers) {
    console.log(`  ${provider.name}: ${provider.isRegistered ? "registered" : "not registered"}`);
  }

  console.log("Downloading and registering execution providers...");
  await manager.downloadAndRegisterEps((name, percent) => {
    process.stdout.write(`\r  ${name}: ${percent.toFixed(1)}%`);
  });
  process.stdout.write("\n");

  const catalog = manager.catalog;
  model = await catalog.getModel(requestedAlias);
  console.log(`Selected ${model.info.displayName ?? model.alias} (${model.id})`);

  if (model.isCached) {
    console.log("Model is already cached.");
  } else {
    console.log("Downloading model...");
    await model.download((percent) => {
      process.stdout.write(`\r  ${percent.toFixed(1)}%`);
    });
    process.stdout.write("\n");
  }

  await model.load();
  modelLoaded = true;
  console.log("Model loaded.");

  session = new ChatSession(model);
  const request = new Request()
    .addItem(Item.userMessage("Why is the sky blue? Answer in two sentences."))
    .setOptions({ search: { temperature: 0, maxOutputTokens: 128 } });

  const response = await session.processRequest(request);
  const text = response.output
    .map((item) => {
      if (item.type === "text") return item.text;
      if (item.type === "message") {
        if (item.content !== undefined) return item.content;
        return item.parts?.filter((part) => part.type === "text").map((part) => part.text).join("") ?? "";
      }
      return "";
    })
    .join("");

  console.log(`\nAssistant: ${text}`);
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
