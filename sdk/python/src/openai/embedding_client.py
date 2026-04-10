# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from __future__ import annotations

import json
import logging
from dataclasses import dataclass, field
from typing import List, Optional

from ..detail.core_interop import CoreInterop, InteropRequest
from ..exception import FoundryLocalException

logger = logging.getLogger(__name__)


class EmbeddingSettings:
    """Settings supported by Foundry Local for embedding generation.

    Attributes:
        dimensions: The number of dimensions for the output embeddings (optional).
        encoding_format: The format to return embeddings in (``"float"`` or ``"base64"``).
    """

    def __init__(
        self,
        dimensions: Optional[int] = None,
        encoding_format: Optional[str] = None,
    ):
        self.dimensions = dimensions
        self.encoding_format = encoding_format


@dataclass
class EmbeddingData:
    """A single embedding result.

    Attributes:
        index: The index of the embedding in the list.
        embedding: The embedding vector.
    """

    index: int
    embedding: List[float]


@dataclass
class EmbeddingResponse:
    """Response from an embedding request.

    Attributes:
        model: The model used to generate the embedding.
        data: List of embedding results.
    """

    model: str
    data: List[EmbeddingData] = field(default_factory=list)


class EmbeddingClient:
    """OpenAI-compatible embedding client backed by Foundry Local Core.

    Attributes:
        model_id: The ID of the loaded embedding model variant.
        settings: Tunable ``EmbeddingSettings`` (dimensions, encoding_format).
    """

    def __init__(self, model_id: str, core_interop: CoreInterop):
        self.model_id = model_id
        self.settings = EmbeddingSettings()
        self._core_interop = core_interop

    @staticmethod
    def _validate_input(input_text: str) -> None:
        """Validate that the input is a non-empty string."""
        if not isinstance(input_text, str) or input_text.strip() == "":
            raise ValueError("Input must be a non-empty string.")

    def _create_request_json(self, input_text: str) -> str:
        """Build the JSON payload for the ``embeddings`` native command."""
        request: dict = {
            "model": self.model_id,
            "input": input_text,
        }

        if self.settings.dimensions is not None:
            request["dimensions"] = self.settings.dimensions

        if self.settings.encoding_format is not None:
            request["encoding_format"] = self.settings.encoding_format

        return json.dumps(request)

    def generate_embedding(self, input_text: str) -> EmbeddingResponse:
        """Generate embeddings for the given input text.

        Args:
            input_text: The text to generate embeddings for.

        Returns:
            An ``EmbeddingResponse`` containing the embedding vector.

        Raises:
            ValueError: If *input_text* is not a non-empty string.
            FoundryLocalException: If the underlying native embeddings command fails.
        """
        self._validate_input(input_text)

        request_json = self._create_request_json(input_text)
        request = InteropRequest(params={"OpenAICreateRequest": request_json})

        response = self._core_interop.execute_command("embeddings", request)
        if response.error is not None:
            raise FoundryLocalException(
                f"Embedding generation failed for model '{self.model_id}': {response.error}"
            )

        data = json.loads(response.data)
        embedding_data = [
            EmbeddingData(index=item["index"], embedding=item["embedding"])
            for item in data.get("data", [])
        ]

        return EmbeddingResponse(
            model=data.get("model", self.model_id),
            data=embedding_data,
        )
