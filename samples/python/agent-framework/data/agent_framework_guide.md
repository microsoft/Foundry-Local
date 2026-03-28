# Microsoft Agent Framework Guide

The Microsoft Agent Framework provides building blocks for creating multi-agent
applications in Python. Agents are autonomous units that receive instructions,
process messages, and optionally invoke tools.

## Core Concepts

### ChatAgent
A `ChatAgent` wraps a language model client with a persona (system instructions),
an optional set of tools, and a conversation history. You create agents via:

```python
from agent_framework import ChatAgent
from agent_framework.openai import OpenAIChatClient

client = OpenAIChatClient(api_key="...", base_url="...", model_id="phi-4-mini")
agent = ChatAgent(chat_client=client, name="Planner", instructions="...")
response = await agent.run("What is the capital of France?")
```

### Tools
Tools are plain Python functions annotated with `Annotated[<type>, Field(...)]`
parameters. The framework automatically generates JSON Schema for tool calling:

```python
from typing import Annotated
from pydantic import Field

def calculate_sum(
    a: Annotated[int, Field(description="First number")],
    b: Annotated[int, Field(description="Second number")],
) -> str:
    return f"The sum is {a + b}"
```

Register tools when creating an agent: `ChatAgent(..., tools=[calculate_sum])`.

### Orchestration Patterns

| Pattern     | Description                                                      |
|-------------|------------------------------------------------------------------|
| Sequential  | Agents run one after another; each receives the previous output. |
| Concurrent  | Multiple agents run in parallel (fan-out) on the same input.     |
| Feedback    | A Critic agent reviews output and can request re-processing.     |
| Hybrid      | Combines sequential, concurrent, and feedback patterns.          |

## Best Practices

1. **Keep instructions focused** — each agent should have a single responsibility.
2. **Limit context length** — chunk large documents before passing to agents.
3. **Use tool calling** — delegate structured tasks to deterministic code.
4. **Set loop limits** — always cap iterative feedback loops to prevent runaway costs.
5. **Stream results** — use Server-Sent Events (SSE) for real-time UI updates.
