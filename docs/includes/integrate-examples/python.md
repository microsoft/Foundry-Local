## Using the OpenAI SDK

```python
# Install with: pip install openai
import openai

# Configure the client to use your local endpoint, noting the port is dynamically assigned
client = openai.OpenAI(
    base_url="http://localhost:5273/v1",
    api_key="not-needed-for-local"  # API key is not required for local usage
)

# Chat completions
response = client.chat.completions.create(
    model="Phi-4-mini-instruct-cuda-gpu",  # Use a model loaded in your service
    messages=[
        {"role": "user", "content": "Explain how Foundry Local works."}
    ]
)

print(response.choices[0].message.content)
```

## Using Direct HTTP Requests

```python
# Install with: pip install requests
import requests
import json
# note the port is dynamically assigned, so check the logs for the correct port
url = "http://localhost:5273/v1/chat/completions"

payload = {
    "model": "Phi-4-mini-instruct-cuda-gpu",
    "messages": [
        {"role": "user", "content": "What are the benefits of running AI models locally?"}
    ]
}

headers = {
    "Content-Type": "application/json"
}

response = requests.post(url, headers=headers, data=json.dumps(payload))
print(response.json()["choices"][0]["message"]["content"])
```

## Streaming Response

```python
import openai
# note the port is dynamically assigned, so check the logs for the correct port
client = openai.OpenAI(
    base_url="http://localhost:5273/v1",
    api_key="not-needed-for-local"
)

stream = client.chat.completions.create(
    model="Phi-4-mini-instruct-cuda-gpu",
    messages=[{"role": "user", "content": "Write a short story about AI"}],
    stream=True
)

for chunk in stream:
    if chunk.choices[0].delta.content is not None:
        print(chunk.choices[0].delta.content, end="")
```
