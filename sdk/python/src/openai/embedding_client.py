# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from __future__ import annotations

import json
import logging
from typing import List, Union

from ..detail.core_interop import CoreInterop, InteropRequest
from ..exception import FoundryLocalException

from openai.types import CreateEmbeddingResponse
from openai.types.embedding_create_params import EmbeddingCreateParams

logger = logging.getLogger(__name__)


class EmbeddingClient:
    """OpenAI-compatible embedding client backed by Foundry Local Core.

    Attributes:
        model_id: The ID of the loaded embedding model variant.
    """

    def __init__(self, model_id: str, core_interop: CoreInterop):
        self.model_id = model_id
        self._core_interop = core_interop

    @staticmethod
    def _validate_input(input_text: str) -> None:
        """Validate that the input is a non-empty string."""
        if not isinstance(input_text, str) or input_text.strip() == "":
            raise ValueError("Input must be a non-empty string.")

    def _create_request_json(self, input_value: Union[str, List[str]]) -> str:
        """Build the JSON payload for the ``embeddings`` native command."""
        request: dict = {
            "model": self.model_id,
            "input": input_value,
        }

        embedding_request = EmbeddingCreateParams(request)

        return json.dumps(embedding_request)

    def _execute_embedding_request(self, input_value: Union[str, List[str]]) -> CreateEmbeddingResponse:
        """Send an embedding request and parse the response."""
        request_json = self._create_request_json(input_value)
        request = InteropRequest(params={"OpenAICreateRequest": request_json})

        response = self._core_interop.execute_command("embeddings", request)
        if response.error is not None:
            raise FoundryLocalException(
                f"Embedding generation failed for model '{self.model_id}': {response.error}"
            )

        data = json.loads(response.data)

        # Add fields required by the OpenAI SDK type that the server doesn't return
        for item in data.get("data", []):
            if "object" not in item:
                item["object"] = "embedding"

        if "usage" not in data:
            data["usage"] = {"prompt_tokens": 0, "total_tokens": 0}

        return CreateEmbeddingResponse.model_validate(data)

    def generate_embedding(self, input_text: str) -> CreateEmbeddingResponse:
        """Generate embeddings for a single input text.

        Args:
            input_text: The text to generate embeddings for.

        Returns:
            A ``CreateEmbeddingResponse`` containing the embedding vector.

        Raises:
            ValueError: If *input_text* is not a non-empty string.
            FoundryLocalException: If the underlying native embeddings command fails.
        """
        self._validate_input(input_text)
        return self._execute_embedding_request(input_text)

    def generate_embeddings(self, inputs: List[str]) -> CreateEmbeddingResponse:
        """Generate embeddings for multiple input texts in a single request.

        Args:
            inputs: The texts to generate embeddings for.

        Returns:
            A ``CreateEmbeddingResponse`` containing one embedding vector per input.

        Raises:
            ValueError: If *inputs* is empty or contains empty strings.
            FoundryLocalException: If the underlying native embeddings command fails.
        """
        if not inputs:
            raise ValueError("Inputs must be a non-empty list of strings.")

        for text in inputs:
            self._validate_input(text)

        return self._execute_embedding_request(inputs)
