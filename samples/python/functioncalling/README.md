# Foundry Local Function Calling Configuration Guide

This guide walks you through enabling function calling support in Foundry Local with Phi-4-mini.

## Prerequisites

- Foundry Local version 0.5.100 or higher
- Access to modify model configuration files

## Setup Instructions

### Step 1: Install Foundry Local

Ensure you have Foundry Local version 0.5.100 or higher installed on your system.

### Step 2: Configure Phi-4-mini Chat Template

Replace the existing **inference_model.json** file for Phi-4-mini with the following configuration:

```json
{
    "Name": "Phi-4-mini-instruct-generic-cpu",
    "PromptTemplate": {
        "system": "<|system|>{Content}<|tool|>{Tool}<|/tool|><|end|>",
        "user": "<|user|>{Content}<|end|>",
        "assistant": "<|assistant|>{Content}<|end|>",
        "tool": "<|tool|>{Tool}<|/tool|>",
        "prompt": "<|system|> You are a helpful assistant with these tools. If you decide to call functions:\n* prefix function calls with functools marker (no closing marker required)\n* all function calls should be generated in a single JSON list formatted as functools[{\"name\": [function name], \"arguments\": [function arguments as JSON]}, ...]\n  * follow the provided JSON schema. Do not hallucinate arguments or values. Do not blindly copy values from the provided samples\n  * respect the argument type formatting. E.g., if the type is number and format is float, write value 7 as 7.0\n  * make sure you pick the right functions that match the user intent<|end|><|user|>{Content}<|end|><|assistant|>"
    }
}
```

### Step 3: Restart Foundry Service

Execute the following command in your terminal to restart the Foundry service:

```bash
foundry service restart
```

### Step 4: Test the Configuration

Run the provided [Notebook](./fl_tools..ipynb) to test and validate the function calling functionality.

## Related Resources

- **Test Notebook**: [fl_tools.ipynb](./fl_tools..ipynb)

## Notes

- The configuration enables proper function calling syntax with the `functools` marker
- Ensure all JSON formatting rules are followed when the model generates function calls
- The system prompt includes specific instructions for proper function argument handling