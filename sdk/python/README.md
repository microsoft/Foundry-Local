# Foundry Local Python Control-Plane SDK
This is a Python Control-Plane SDK for Foundry Local. It provides a simple interface to interact with the Foundry Local API.

## Prerequisites
Foundry Local must be installed and findable in your PATH.

## Installation
To install the SDK, run the following command at the root of the repository:

```bash
pip install .
```

You can also build the wheel using the following command:

```bash
python setup.py bdist_wheel
```

The wheel will be created in the `dist` directory. You can install it using pip:

```bash
pip install dist/foundry_local_sdk*.whl
```

## Usage
With bootstrapping, the SDK can automatically start the Foundry Local service and get the API URL. It can optionally download and load a model.
- The SDK accepts a `alias` or `model ID` for various apis. If an alias is provided, it selects the most preferred model. The models are ordered by preference based on your system's capabilities. An ID always points to a specific model.

```python
from foundry_local import foundryLocalManager

alias = "deepseek-r1-1.5b"
fl_manager = foundryLocalManager(alias)

# check that the service is running
print(fl_manager.is_service_running())

# list all available models in the catalog
print(fl_manager.list_catalog_models())

# list all downloaded models
print(fl_manager.list_local_models())

# get information on the selected model
model_info = fl_manager.get_model_info(alias)
print(model_info)
```

You can also use the SDK without bootstrapping.

```python
from foundry_local import foundryLocalManager

alias = "deepseek-r1-1.5b"
fl_manager = foundryLocalManager()

# start the service
fl_manager.start_service()

# download the model
fl_manager.download_model(alias)

# load the model
model_info = fl_manager.load_model(alias)
print(model_info)
```

Use the foundry local endpoint with an OpenAI compatible API client. For example, using the `openai` package:

```python
from openai import OpenAI

client = OpenAI(base_url=fl_manager.endpoint, api_key=fl_manager.api_key)

with client.chat.completions.create(
    model=model_info.id,
    messages=[{"role": "user", "content": "Solve x^2 + 5x + 6 = 0."}],
    max_tokens=250,
    stream=True,
) as stream:
    for chunk in stream:
        if chunk.choices[0].delta.content:
            print(chunk.choices[0].delta.content, end="", flush=True)
```

**Note:** The `openai` chat completion API requires a model ID. So, you need to get the model ID from the `model_info` object if using an alias.
