## Basic Request

For quick tests or integrations with command line scripts:

```bash
curl http://localhost:5272/v1/chat/completions ^
  -H "Content-Type: application/json" ^
  -d "{\"model\": \"Phi-4-mini-instruct-cuda-gpu\", \"messages\": [{\"role\": \"user\", \"content\": \"Tell me a short story\"}]}"
```

## Streaming Response

**Note**: The example here works, but because there's no cleansing of the output, it may not be as clean as the other examples.

```bash
curl http://localhost:5272/v1/chat/completions ^
  -H "Content-Type: application/json" ^
  -d "{\"model\": \"Phi-4-mini-instruct-cuda-gpu\", \"messages\": [{\"role\": \"user\", \"content\": \"Tell me a short story\"}], \"stream\": true}"
```
