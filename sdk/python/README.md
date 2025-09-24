# Foundry Local Python SDK
The Foundry Local SDK simplifies AI model management in local environments by providing control-plane operations separate from data-plane inferencing code.

## Prerequisites
Foundry Local must be installed and findable in your PATH.

## Getting Started
```
pip install foundry-local-sdk
```

## Usage

The SDK provides a simple interface to interact with the Foundry Local API. You can use it to manage models, check the status of the service, and make requests to the models.

### Bootstrapping

The SDK can *bootstrap* Foundry Local, which will initiate the following sequence:

1. Start the Foundry Local service, if it is not already running.
1. Automatically detect the hardware and software requirements for the model.
1. Download the highest-performance model for the detected hardware, if it is not already downloaded.
1. Load the model into memory.

To use the SDK with bootstrapping, you can use the following code:

```python
from foundry_local import FoundryLocalManager

# Provide the alias of the model you want to use
# The alias is a string that identifies the model in the Foundry Local catalog.
# The alias can be found in the Foundry Local catalog.
# For example, "phi-3.5-mini" is the alias for the Phi 3.5 model.
# Foundry local will automatically download the most suitable model for your hardware.
alias = "phi-3.5-mini"
fl_manager = FoundryLocalManager(alias)

# check that the service is running
print(fl_manager.is_service_running())

# list all available models in the catalog
print(fl_manager.list_catalog_models())

# list all downloaded models
print(fl_manager.list_cached_models())

# get information on the selected model
model_info = fl_manager.get_model_info(alias)
print(model_info)
```

Alternatively, you can use the `FoundryLocalManager` class to manage the service and models manually. This is useful if you want to control the service and models without bootstrapping. For example, you want to present to the end user what is happening in the background.

```python
from foundry_local import FoundryLocalManager

alias = "phi-3.5-mini"
fl_manager = FoundryLocalManager()

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
import openai
from foundry_local import FoundryLocalManager

# By using an alias, the most suitable model will be downloaded 
# to your end-user's device.
alias = "phi-3.5-mini"

# Create a FoundryLocalManager instance. This will start the Foundry 
# Local service if it is not already running and load the specified model.
manager = FoundryLocalManager(alias)

# The remaining code uses the OpenAI Python SDK to interact with the local model.

# Configure the client to use the local Foundry service
client = openai.OpenAI(
    base_url=manager.endpoint,
    api_key=manager.api_key  # API key is not required for local usage
)

# Set the model to use and generate a streaming response
stream = client.chat.completions.create(
    model=manager.get_model_info(alias).id,
    messages=[{"role": "user", "content": "What is the golden ratio?"}],
    stream=True
)

# Print the streaming response
for chunk in stream:
    if chunk.choices[0].delta.content is not None:
        print(chunk.choices[0].delta.content, end="", flush=True)
```
