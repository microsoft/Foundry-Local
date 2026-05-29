// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Android-only diagnostic utility. Guarded by __ANDROID__ at the include site
// in manager.cc — this header should not be included on other platforms.
#pragma once

#include "logger.h"

namespace fl {

/// Diagnostic check for SSL certificate configuration on Android.
/// Validates that SSL_CERT_FILE is set and contains valid PEM certificates
/// that OpenSSL can use for TLS verification.
///
/// Logs warnings if the setup looks invalid, info on success.
/// Never throws — safe to call during initialization.
bool CheckSslCertSetup(ILogger& logger);

}  // namespace fl
