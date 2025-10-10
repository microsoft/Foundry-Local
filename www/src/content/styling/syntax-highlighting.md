---
title: Code Display & Formatting
description: Customize how code examples appear in the Foundry Local interface and documentation
---

# Code Display & Formatting

Foundry Local provides comprehensive code formatting and syntax highlighting options for code examples in the web interface, SDK documentation, and API references.

## Interface Code Blocks

The Foundry Local web interface uses Microsoft's code styling guidelines to ensure consistency with other Microsoft developer tools.

### Default Theme Settings

Foundry Local uses a customized version of the Microsoft Visual Studio theme for code highlighting:

```typescript
// Configuration in foundry-local/src/web/styles/code-theme.ts
export const codeThemeConfig = {
	lightTheme: 'microsoft-light',
	darkTheme: 'microsoft-dark',
	fontFamily: "'Cascadia Code', 'JetBrains Mono', Consolas, monospace",
	fontSize: '14px',
	lineHeight: 1.5,
	borderRadius: '4px',
	highlightColor: {
		light: 'rgba(0, 120, 212, 0.1)',
		dark: 'rgba(0, 120, 212, 0.2)'
	}
};
```

### Supported Languages

Foundry Local provides optimized syntax highlighting for the following languages commonly used with AI APIs:

```typescript
// Core supported languages
const supportedLanguages = [
	'python', // Python SDK examples
	'typescript', // TypeScript SDK examples
	'javascript', // JavaScript client examples
	'csharp', // C# SDK examples
	'java', // Java SDK examples
	'go', // Go SDK examples
	'rust', // Rust client examples
	'json', // API request/response examples
	'yaml', // Configuration examples
	'bash', // CLI commands
	'powershell', // Windows CLI commands
	'http', // Raw HTTP requests
	'markdown', // Documentation examples
	'sql' // Database integration examples
];
```

## Code Copy Functionality

Foundry Local's interface includes intelligent code copying features:

1. **Smart Copy**: Automatically removes comments and annotations when copying example code
2. **Format Preservation**: Maintains indentation and formatting
3. **Variable Substitution**: Replaces placeholders with your actual API keys
4. **Code Sectioning**: Allows copying specific parts of larger examples

To customize copy behavior:

```yaml
# config.yaml
ui:
  code_blocks:
    copy_button: true
    copy_options:
      remove_comments: true
      substitute_variables: true
      notification_duration_ms: 2000
```

## API Reference Styling

API reference documentation has specialized code block formatting:

### Request/Response Formatting

```yaml
code_blocks:
  api_reference:
    show_line_numbers: true
    show_request_headers: true
    collapsible_sections: true
    max_height: '500px'
    request_response_style:
      request_background: '#f5f5f5'
      response_background: '#f0f7ff'
      separator_color: '#d0d0d0'
```

### Request Examples

Foundry Local automatically generates code examples for multiple languages. Configure which languages appear by default:

```yaml
api_documentation:
  default_languages:
    - python
    - javascript
    - curl
  available_languages:
    - python
    - javascript
    - typescript
    - curl
    - csharp
    - java
    - go
```

## Code Editor Integration

When using the built-in code playground for testing API calls:

```yaml
playground:
  editor:
    theme: 'vs-code-light' # Options: vs-code-light, vs-code-dark, github, monokai
    font_family: 'Cascadia Code'
    font_size: 14
    tab_size: 2
    auto_complete: true
    format_on_paste: true
    auto_closing_brackets: true
    minimap: false
```

## Custom API Examples

For organizations developing custom examples, Foundry Local can display company-specific code samples:

```yaml
custom_examples:
  enabled: true
  repository_path: '/path/to/examples'
  languages:
    - name: 'Python'
      tag: 'python'
      extension: '.py'
    - name: 'TypeScript'
      tag: 'typescript'
      extension: '.ts'
  display_options:
    show_company_logo: true
    custom_header_text: 'ACME Corp Examples'
```

## Example Formatting Guide

When creating code examples for your organization's internal documentation:

1. **Use consistent indentation** (2 or 4 spaces)
2. **Include helpful comments**
3. **Show complete examples** with imports and setup
4. **Annotate key lines** with `# ⬅ Important!` or similar
5. **Keep examples concise** but functional
6. **Include error handling** in production examples

For example:

```python
# Example: Using Foundry Local for text embeddings
from foundry_local import FoundryClient

# Initialize the client
client = FoundryClient()  # ⬅ No API key needed for local deployment

try:
    # Generate embeddings
    response = client.embeddings.create(
        model="text-embedding-3-small-local",  # Local model
        input="The quick brown fox jumps over the lazy dog",
        dimensions=1536  # Optional: specify dimensions
    )

    # Access the embedding vector
    embedding = response.data[0].embedding
    print(f"Generated embedding with {len(embedding)} dimensions")

except Exception as e:
    print(f"Error generating embedding: {e}")
```

## Best Practices

1. **Consistent Styling**: Match code styling with Microsoft's developer documentation
2. **Performance**: Only load syntax highlighting for languages you actively use
3. **Accessibility**: Ensure sufficient color contrast in all code blocks
4. **Responsive Design**: Test code blocks on mobile and desktop views
5. **Error Examples**: Include examples of proper error handling
