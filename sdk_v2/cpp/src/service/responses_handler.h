// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#ifdef FOUNDRY_LOCAL_HAS_WEB_SERVICE

#include "inferencing/generative/openresponses/response_converter.h"
#include "service/handler_utils.h"

#include "inferencing/generative/openresponses/response_chain.h"

#include <memory>
#include <string>
#include <utility>

namespace fl {

struct ServiceContext;
struct Request;
class ChatSession;
class Model;
class GenAIModelInstance;

namespace responses {
struct ResponseCreateParams;
}  // namespace responses

// ========================================================================
// Handler: POST /v1/responses — OpenAI Responses API
// ========================================================================

class ResponsesHandler : public HttpRequestHandler {
 public:
  explicit ResponsesHandler(ServiceContext& ctx);

  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>& request) override;

 private:
  // --- Extracted steps from handle() ---

  /// Parse JSON body, validate required fields, and deserialize into ResponseCreateParams.
  /// Returns an error response on failure, nullptr on success.
  std::shared_ptr<OutgoingResponse> ParseAndValidateRequest(const std::string& body,
                                                            nlohmann::json& req_json,
                                                            responses::ResponseCreateParams& params);

  /// Look up model in catalog and verify it's loaded. Sets output pointers.
  /// Returns an error response on failure, nullptr on success.
  std::shared_ptr<OutgoingResponse> ResolveModel(const std::string& model_name,
                                                 Model*& model, GenAIModelInstance*& loaded);

  /// Reconstruct the full replay context for a chained request by walking `previous_response_id` to the root.
  /// Only called when no cached session is available — a live session already holds the conversation.
  /// Returns an error response when the chain cannot be reconstructed, nullptr on success.
  std::shared_ptr<OutgoingResponse> LoadPreviousContext(const responses::ResponseCreateParams& params,
                                                        ResponseChainContext& context_storage,
                                                        const ResponseChainContext*& previous_context);

  // --- Inference dispatch ---

  // `tool_kinds` is the registry snapshot taken for this turn: it decides whether a produced call
  // is reported as a function call or a custom tool call.
  std::shared_ptr<OutgoingResponse> HandleNonStreaming(std::unique_ptr<ChatSession> session, Request& session_request,
                                                       const std::string& model_name, const std::string& response_id,
                                                       int64_t created_at,
                                                       const responses::ResponseCreateParams& params,
                                                       const nlohmann::json& req_json,
                                                       const ResponseConverter::ToolKindsByName& tool_kinds);

  std::shared_ptr<OutgoingResponse> HandleStreaming(std::unique_ptr<ChatSession> session, Request session_request,
                                                    const std::string& model_name, const std::string& response_id,
                                                    int64_t created_at,
                                                    const responses::ResponseCreateParams& params,
                                                    const nlohmann::json& req_json,
                                                    ResponseConverter::ToolKindsByName tool_kinds);

  ServiceContext& ctx_;
};

// ========================================================================
// Handler: GET /v1/responses/{id} — Retrieve a stored response
// ========================================================================

class GetResponseHandler : public HttpRequestHandler {
 public:
  explicit GetResponseHandler(ServiceContext& ctx);

  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>& request) override;

 private:
  ServiceContext& ctx_;
};

// ========================================================================
// Handler: GET /v1/responses — List stored responses
// ========================================================================

class ListResponsesHandler : public HttpRequestHandler {
 public:
  explicit ListResponsesHandler(ServiceContext& ctx);

  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>& request) override;

 private:
  ServiceContext& ctx_;
};

// ========================================================================
// Handler: DELETE /v1/responses/{id} — Delete a stored response
// ========================================================================

class DeleteResponseHandler : public HttpRequestHandler {
 public:
  explicit DeleteResponseHandler(ServiceContext& ctx);

  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>& request) override;

 private:
  ServiceContext& ctx_;
};

// ========================================================================
// Handler: GET /v1/responses/{id}/input_items — Get input items for a response
// ========================================================================

class GetInputItemsHandler : public HttpRequestHandler {
 public:
  explicit GetInputItemsHandler(ServiceContext& ctx);

  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>& request) override;

 private:
  ServiceContext& ctx_;
};

// --- Factory functions ---

std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateResponsesHandler(ServiceContext& ctx);
std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateGetResponseHandler(ServiceContext& ctx);
std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateListResponsesHandler(ServiceContext& ctx);
std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateDeleteResponseHandler(ServiceContext& ctx);
std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateGetInputItemsHandler(ServiceContext& ctx);

}  // namespace fl

#endif  // FOUNDRY_LOCAL_HAS_WEB_SERVICE
