# Tool calling

This sample uses native `ChatSession` tool definitions and the `ToolCallItem`/`ToolResultItem` flow. It forces the
first turn to call a multiplication tool, executes the function in JavaScript, and submits the result to the same
session for the final answer.

```bash
npm install
npm start
```

With no argument, the sample enumerates the catalog and selects the smallest model that declares tool-calling
support. To request a specific model, pass its alias as the first argument:

```bash
npm start -- <model-alias>
```

The sample validates that a requested model declares tool-calling support and fails clearly when it does not.
