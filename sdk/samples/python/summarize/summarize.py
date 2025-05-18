#!/usr/bin/env python3

import os
import sys
import argparse
from pathlib import Path
from openai import OpenAI
from foundry_local import FoundryLocalManager

def read_file_content(file_path):
    """Read content from a file."""
    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            return file.read()
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)

def get_summary(text, client, model_name):
    """Get summary from OpenAI API."""
    try:
        response = client.chat.completions.create(
            model=model_name,
            messages=[
                {"role": "system", "content": "You are a helpful assistant that summarizes text. Provide a concise summary."},
                {"role": "user", "content": f"Please summarize the following text:\n\n{text}"}
            ]
        )
        return response.choices[0].message.content
    except Exception as e:
        print(f"Error getting summary from OpenAI: {e}")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description='Summarize text from a file or string using OpenAI.')
    parser.add_argument('input', help='File path or text string to summarize')
    parser.add_argument('--text', action='store_true', help='Treat input as direct text instead of a file path')
    parser.add_argument('--model', help='Model alias to use for summarization')
    args = parser.parse_args()

    fl_manager = FoundryLocalManager()

    fl_manager.start_service()

    model_list = fl_manager.list_local_models()
    
    if not model_list:
        print("No local models available")
        sys.exit(1)
        
    # Select model based on alias or use first one
    if args.model:
        selected_model = next((model for model in model_list if model.alias == args.model), None)
        if selected_model:
            model_name = selected_model.id
        else:
            model_name = model_list[0].id
            print(f"Model alias '{args.model}' not found, using default model: {model_name}")
    else:
        model_name = model_list[0].id
        
    print(f"Using model: {model_name}")
    
    # Initialize OpenAI client
    client = OpenAI(base_url=fl_manager.endpoint, api_key=fl_manager.api_key)

    # Get input text
    if args.text:
        text = args.input
    else:
        text = read_file_content(args.input)

    # Get and print summary
    summary = get_summary(text, client, model_name)
    print("\nSummary:")
    print("-" * 50)
    print(summary)
    print("-" * 50)

if __name__ == "__main__":
    main() 