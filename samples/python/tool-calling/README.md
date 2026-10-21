# Tool calling

Registers a JSON-schema function tool on a native `ChatSession`, handles the
model's `ToolCallItem`, and returns a `ToolResultItem` for the final response.
The sample discovers a model whose catalog metadata explicitly reports
tool-calling support, preferring a cached model and then the smallest known
download. It fails with a clear error if the catalog has no compatible model.

```bash
python -m pip install -r requirements.txt
python src/app.py
```
