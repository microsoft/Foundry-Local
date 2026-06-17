// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#ifdef FOUNDRY_LOCAL_HAS_WEB_SERVICE

#include "service/handler_utils.h"

#include <memory>

namespace fl {

struct ServiceContext;

/// Factory: POST /v1/embeddings
std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateEmbeddingsHandler(ServiceContext& ctx);

}  // namespace fl

#endif  // FOUNDRY_LOCAL_HAS_WEB_SERVICE
