// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <string>

namespace fl
{
    namespace http
    {

        /// Perform an HTTP GET request. Returns the response body.
        /// Throws fl::Exception on HTTP errors or connection failures.
        std::string HttpGet(const std::string &url,
                            const std::string &user_agent = "",
                            bool close_connection = false);

        /// Perform an HTTP POST request with a JSON body. Returns the response body.
        /// Throws fl::Exception on HTTP errors or connection failures.
        std::string HttpPost(const std::string &url,
                             const std::string &json_body,
                             const std::string &user_agent = "",
                             bool close_connection = false);

        /// Perform an HTTP DELETE request. Returns the response body.
        /// Throws fl::Exception on HTTP errors or connection failures.
        std::string HttpDelete(const std::string &url,
                               const std::string &user_agent = "",
                               bool close_connection = false);

    } // namespace http
} // namespace fl
