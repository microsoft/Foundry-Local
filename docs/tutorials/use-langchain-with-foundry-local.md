# Build an application with LangChain

This tutorial shows you how to create an application using Foundry Local and LangChain. You learn how to integrate locally hosted AI models with the popular LangChain framework.

## Prerequisites

Before starting this tutorial, you need:

- **Foundry Local** [installed](../get-started.md) on your computer
- **At least one model loaded** using the `foundry model load` command:
  ```bash
  foundry model load phi4-cpu
  ```
- **LangChain with OpenAI support** installed:

  ```bash
  pip install langchain[openai]
  ```

## Create a LangChain application

Foundry Local supports the OpenAI Chat Completion API, making it easy to integrate with LangChain. Here's how to build a translation application:

```python
import os

from langchain_openai import ChatOpenAI
from langchain_core.prompts import ChatPromptTemplate

# Set a placeholder API key (not actually used by Foundry Local)
if not os.environ.get("OPENAI_API_KEY"):
   os.environ["OPENAI_API_KEY"] = "no_key"

# Configure ChatOpenAI to use your locally-running model
llm = ChatOpenAI(
    model="Phi-4-mini-instruct-generic-cpu",
    base_url="http://localhost:5272/v1/",
    temperature=0.0,
    streaming=False
)

# Create a translation prompt template
prompt = ChatPromptTemplate.from_messages([
    (
        "system",
        "You are a helpful assistant that translates {input_language} to {output_language}."
    ),
    ("human", "{input}")
])

# Build a simple chain by connecting the prompt to the language model
chain = prompt | llm

# Run the chain with your inputs
ai_msg = chain.invoke({
    "input_language": "English",
    "output_language": "French",
    "input": "I love programming."
})

# Display the result
print(ai_msg)
```

## Next steps

- Explore the [LangChain documentation](https://python.langchain.com/docs/introduction) for more advanced features and capabilities.
- [How to compile Hugging Face models to run on Foundry Local](../how-to/how-to-compile-hugging-face-models.md)
