import { ChatSession, FoundryLocalManager, Item, Request } from "foundry-local-sdk";

const manager = FoundryLocalManager.create({
  appName: "foundry_local_samples",
  logLevel: "info",
});

let model;
let session;
let modelLoaded = false;

try {
  model = await manager.catalog.getModel("qwen2.5-0.5b");
  if (model.supportsToolCalling !== true) {
    throw new Error("The selected qwen2.5-0.5b variant does not support tool calling.");
  }
  console.log(`Selected tool-capable model: ${model.info.displayName ?? model.alias} (${model.id})`);

  if (!model.isCached) {
    console.log(`Downloading ${model.alias}...`);
    await model.download();
  }
  await model.load();
  modelLoaded = true;

  session = new ChatSession(model);
  session.addToolDefinition({
    name: "multiply_numbers",
    description: "Multiply two numbers.",
    jsonSchema: JSON.stringify({
      type: "object",
      properties: {
        first: { type: "number" },
        second: { type: "number" },
      },
      required: ["first", "second"],
      additionalProperties: false,
    }),
  });

  const toolRequest = new Request()
    .addItem(Item.systemMessage("Use the provided tool for multiplication."))
    .addItem(Item.userMessage("What is 7 multiplied by 6?"))
    .setOptions({ toolChoice: "required", search: { temperature: 0, maxOutputTokens: 128 } });
  const toolResponse = await session.processRequest(toolRequest);
  const toolCall = toolResponse.output.find((item) => item.type === "toolCall");
  if (toolCall?.type !== "toolCall" || toolCall.name !== "multiply_numbers") {
    throw new Error("The model did not return the expected multiply_numbers tool call.");
  }

  const parsed = JSON.parse(toolCall.arguments);
  if (
    typeof parsed !== "object" ||
    parsed === null ||
    typeof parsed.first !== "number" ||
    typeof parsed.second !== "number"
  ) {
    throw new Error(`Invalid tool arguments: ${toolCall.arguments}`);
  }
  const product = parsed.first * parsed.second;
  console.log(`Tool call: ${parsed.first} × ${parsed.second} = ${product}`);

  const finalRequest = new Request()
    .addItem(Item.toolResult(toolCall.callId, String(product)))
    .setOptions({ toolChoice: "none", search: { temperature: 0, maxOutputTokens: 128 } });
  const finalResponse = await session.processRequest(finalRequest);
  const answer = finalResponse.output
    .map((item) => {
      if (item.type === "text") return item.text;
      if (item.type === "message") return item.content ?? "";
      return "";
    })
    .join("");
  console.log(`Assistant: ${answer}`);
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
