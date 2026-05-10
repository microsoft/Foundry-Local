# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""OpenAI-compatible embedding client backed by the Foundry Local native layer."""
from __future__ import annotations

import json
from typing import Any, List, Union

from openai.types import CreateEmbeddingResponse


class EmbeddingClient:
    """OpenAI-compatible embedding client backed by Foundry Local Core.

    Each call creates a fresh native session (stateless — no session history).

    Attributes:
        model_id: The ID of the loaded embedding model variant.
    """

    def __init__(self, model_id: str, model_ptr: Any) -> None:
        self.model_id = model_id
        self._model_ptr = model_ptr

    @staticmethod
    def _validate_input(input_text: str) -> None:
        """Validate that the input is a non-empty string."""
        if not isinstance(input_text, str) or not input_text.strip():
            raise ValueError("Input must be a non-empty string.")

    def _build_request_json(self, input_value: Union[str, List[str]]) -> str:
        """Build the JSON payload for an embeddings request."""
        return json.dumps({"model": self.model_id, "input": input_value})

    def _run_native_request(self, request_json: str) -> str:
        """Create a fresh native session, process the request, return the response JSON string."""
        from foundry_local._native import ffi
        from foundry_local._native.api import api
        from foundry_local.items import Item, TextItem, TextItemType
        from foundry_local.request import Request

        session_out = ffi.new("flSession**")
        api.check_status(api.inference.Session_Create(self._model_ptr, session_out))
        session_ptr = session_out[0]

        try:
            req = Request()
            text_item = TextItem(request_json, TextItemType.OPENAI_JSON)
            req.add_item(text_item)  # transfers ownership of text_item

            resp_out = ffi.new("flResponse**")
            api.check_status(api.inference.Session_ProcessRequest(session_ptr, req._ptr, resp_out))
            resp_ptr = resp_out[0]

            try:
                item_out = ffi.new("flItem**")
                api.check_status(api.inference.Response_GetItem(resp_ptr, 0, item_out))
                # owns=False — response owns the item; text is copied to a Python str.
                response_item = Item.from_native(item_out[0], owns=False)
                return response_item.text
            finally:
                api.inference.Response_Release(resp_ptr)
        finally:
            api.inference.Session_Release(session_ptr)

    def _parse_response(self, response_json: str) -> CreateEmbeddingResponse:
        """Parse the response JSON and apply fields required by the OpenAI type."""
        data = json.loads(response_json)

        # The server may omit "object" on embedding items and "usage" on the response;
        # add defaults so CreateEmbeddingResponse.model_validate doesn't reject them.
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
            FoundryLocalException: If the native embeddings call fails.
        """
        self._validate_input(input_text)

        request_json = self._build_request_json(input_text)
        response_json = self._run_native_request(request_json)
        return self._parse_response(response_json)

    def generate_embeddings(self, inputs: List[str]) -> CreateEmbeddingResponse:
        """Generate embeddings for multiple input texts in a single request.

        Args:
            inputs: The texts to generate embeddings for.

        Returns:
            A ``CreateEmbeddingResponse`` containing one embedding vector per input.

        Raises:
            ValueError: If *inputs* is empty or any element is empty.
            FoundryLocalException: If the native embeddings call fails.
        """
        if not inputs:
            raise ValueError("Inputs must be a non-empty list of strings.")
        for text in inputs:
            self._validate_input(text)

        request_json = self._build_request_json(inputs)
        response_json = self._run_native_request(request_json)
        return self._parse_response(response_json)
