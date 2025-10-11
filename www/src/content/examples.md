---
title: Model Examples
description: Real-world usage scenarios and examples of Foundry Local model deployments
---

# Model Examples

Discover how organizations and developers are using Foundry Local to deploy powerful AI models in various scenarios.

## Featured Use Cases

### Private Coding Assistant

**Scale AI's local development environment**

A software development team implemented Foundry Local to create a private coding assistant that never transmits proprietary code to external services. By running a 7B parameter model locally, developers get intelligent code completion, documentation generation, and refactoring suggestions while maintaining complete data privacy.

```python
from foundry_local import FoundryClient

client = FoundryClient()

# Generate a function from a docstring
response = client.chat.completions.create(
    model="codellama-7b-local",
    messages=[
        {"role": "system", "content": "You are a helpful coding assistant."},
        {"role": "user", "content":
         """Write a Python function with this specification:

         def preprocess_text(text: str, max_length: int = 512) -> str:
             '''
             Preprocesses text for semantic search by removing special characters,
             normalizing whitespace, and truncating to max_length.

             Args:
                 text: The input text to preprocess
                 max_length: Maximum character length

             Returns:
                 Preprocessed text ready for embedding
             '''
         """}
    ],
    max_tokens=500
)
```

### Healthcare Decision Support

**Regional Hospital System**

A healthcare provider deployed Foundry Local to assist clinicians in summarizing patient records and identifying potential treatment options based on historical data. By keeping all patient data on-premises, they maintain HIPAA compliance while providing AI-augmented decision support.

```python
# Example healthcare decision support (with synthetic data)
response = client.completions.create(
    model="clinical-bert-local",
    prompt="""Patient presenting with: elevated blood pressure (160/95),
    headache, dizziness, family history of cardiovascular disease.
    Previous medications: lisinopril (discontinued due to cough).

    Summarize key concerns and suggest potential treatment paths:""",
    max_tokens=350,
    temperature=0.2
)
```

### Data Analysis Pipeline

**Financial Services Company**

A financial institution integrated Foundry Local into their data analysis workflow to extract insights from complex financial documents. The solution processes PDFs, extracts structured data, and identifies key metrics without transmitting sensitive financial information outside their network.

```python
# Example document analysis with multimodal capabilities
from foundry_local import FoundryClient
import base64

client = FoundryClient()

# Load a financial statement image
with open("quarterly_statement.jpg", "rb") as image_file:
    base64_image = base64.b64encode(image_file.read()).decode('utf-8')

response = client.chat.completions.create(
    model="gpt-4-vision-local",
    messages=[
        {"role": "user",
         "content": [
            {"type": "text", "text": "Extract the following data from this financial statement: Revenue, EBITDA, Net Income, and Year-over-Year Growth."},
            {"type": "image_url", "image_url": {"url": f"data:image/jpeg;base64,{base64_image}"}}
         ]
        }
    ],
    max_tokens=300
)
```

## Implementation Guidelines

To implement similar solutions with Foundry Local:

1. **Assess your requirements**:
   - Identify which models best fit your use case
   - Determine hardware requirements based on model size
   - Consider privacy and security constraints

2. **Prepare your infrastructure**:
   - Ensure sufficient GPU resources for optimal performance
   - Configure Docker settings for memory allocation
   - Set up appropriate network access controls

3. **Integration approach**:
   - Use the Python SDK for deep integration with applications
   - Leverage REST APIs for cross-language compatibility
   - Consider WebSocket connections for streaming responses

4. **Performance optimization**:
   - Experiment with different quantization settings
   - Implement caching for repeated queries
   - Use batching for high-volume scenarios

## Community Showcase

Are you using Foundry Local in an interesting way? We'd love to feature your implementation!

Share your use case by submitting details to our [community showcase](https://github.com/microsoft/foundry-local/discussions/categories/showcase) with:

- Description of your implementation
- Technical approach and architecture
- Performance metrics and lessons learned
- Code samples (if possible)

---

_Looking for more examples? Check out our [GitHub repository](https://github.com/microsoft/foundry-local/tree/main/examples) for additional code samples and reference implementations._
