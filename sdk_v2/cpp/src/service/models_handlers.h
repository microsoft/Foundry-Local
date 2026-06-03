// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#ifdef FOUNDRY_LOCAL_HAS_WEB_SERVICE

#include <oatpp/web/server/HttpRequestHandler.hpp>

#include <memory>

namespace fl {

struct ServiceContext;

std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateListLoadedModelsHandler(ServiceContext& ctx);
std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateLoadModelHandler(ServiceContext& ctx);
std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateUnloadModelHandler(ServiceContext& ctx);
std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateOpenAIListModelsHandler(ServiceContext& ctx);
std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateOpenAIRetrieveModelHandler(ServiceContext& ctx);

}  // namespace fl

#endif  // FOUNDRY_LOCAL_HAS_WEB_SERVICE
