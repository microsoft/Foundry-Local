# Text Summarizer

A simple command-line utility that uses Foundry Local to generate summaries of text files or direct text input.

## Features

- **Cache-aware**: Checks the local model cache before downloading — if the model is already cached, the download is skipped automatically.
- **Visual feedback**: Shows step-by-step status (service start → cache check → download/skip → load → ready) so you always know what's happening.
- **Flexible model selection**: Use `--model` to pick a specific model alias, or omit it to automatically use the first cached model.

## Setup

1. Install the required dependencies:
   ```bash
   pip install -r requirements.txt
   ```

## Usage

The utility can be used in two ways:

1. Summarize a text file:
   ```bash
   python summarize.py path/to/your/file.txt
   ```

2. Summarize direct text input:
   ```bash
   python summarize.py "Your text to summarize here" --text
   ```

You can also specify which model to use with the `--model` parameter:
   ```bash
   python summarize.py path/to/your/file.txt --model "your-model-alias"
   ```

If the specified model is not found, the script will use the first available model.

## Requirements

- Python 3.6 or higher
- Foundry Local Service
- Required Python packages (see requirements.txt)

