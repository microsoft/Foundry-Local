// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "logger.h"

#include <chrono>
#include <functional>
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

        // ------------------------------------------------------------------
        // Retry primitive — used by registry resolution to ride out transient
        // 5xx / network blips without giving up after a single attempt.
        // ------------------------------------------------------------------

        /// Outcome of one attempt of a retryable operation.
        enum class RetryDecision
        {
            Success,         ///< op succeeded; `body` is the result
            RetryTransient,  ///< transient failure (5xx, transport error); retry if budget remains
            FailPermanent    ///< permanent failure (4xx, validation); do not retry
        };

        struct RetryAttempt
        {
            RetryDecision decision = RetryDecision::FailPermanent;
            std::string body;           // populated on Success
            std::string error_message;  // populated on RetryTransient / FailPermanent
        };

        struct RetryConfig
        {
            int max_retries = 3;                         // 4 attempts total
            std::chrono::milliseconds base_delay{500};   // 500ms, 1s, 2s, ...
            std::chrono::milliseconds max_total{10000};  // overall budget
        };

        /// Execute `op` up to (config.max_retries + 1) times. Between attempts, sleep for
        /// `base_delay * 2^attempt + rand(0..base_delay)`, capped so that total elapsed time stays
        /// inside `max_total`. On Success returns `body`. On exhausted retries or FailPermanent
        /// throws fl::Exception with FOUNDRY_LOCAL_ERROR_INTERNAL.
        ///
        /// `sleep_fn` is injected for tests so they don't block on real wall-clock delays.
        std::string RetryWithBackoff(const std::function<RetryAttempt()> &op,
                                     const RetryConfig &config,
                                     ILogger &logger,
                                     const std::function<void(std::chrono::milliseconds)> &sleep_fn = {});

        /// Convenience wrapper: HTTP GET with retry on transient 5xx / network errors.
        /// 4xx responses are treated as permanent failures (no retry).
        std::string HttpGetWithRetry(const std::string &url,
                                     const std::string &user_agent,
                                     ILogger &logger,
                                     bool close_connection = false,
                                     const RetryConfig &config = {});

    } // namespace http
} // namespace fl
