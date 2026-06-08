// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "platform/android/ssl_cert_checker.h"

#ifdef __ANDROID__

#include "log_level.h"
#include "logger.h"

#include <fmt/format.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <cstdlib>
#include <cstdio>
#include <exception>
#include <string>

namespace fl {

namespace {

// Extract the most recent OpenSSL error as a human-readable string.
std::string GetOpenSslError() {
  unsigned long err = ERR_get_error();
  if (err == 0) {
    return "unknown error";
  }

  char buf[256];
  ERR_error_string_n(err, buf, sizeof(buf));
  return std::string(buf);
}

}  // namespace

bool CheckSslCertSetup(ILogger& logger) {
  try {
    // Step 1: Check SSL_CERT_FILE env var
    const char* cert_file = std::getenv("SSL_CERT_FILE");
    if (cert_file == nullptr || cert_file[0] == '\0') {
      logger.Log(LogLevel::Warning,
                 "SSL cert check: SSL_CERT_FILE is not set. "
                 "TLS verification will likely fail on Android.");
      return false;
    }

    // Step 2: Verify the file exists and is non-empty
    std::FILE* f = std::fopen(cert_file, "rb");
    if (f == nullptr) {
      logger.Log(LogLevel::Warning,
                 fmt::format("SSL cert check: SSL_CERT_FILE '{}' does not exist or cannot be opened.",
                             cert_file));
      return false;
    }

    std::fseek(f, 0, SEEK_END);
    const long file_size = std::ftell(f);
    std::fclose(f);

    if (file_size <= 0) {
      logger.Log(LogLevel::Warning,
                 fmt::format("SSL cert check: SSL_CERT_FILE '{}' is empty.", cert_file));
      return false;
    }

    // Step 3: Parse PEM certificates and count them
    BIO* bio = BIO_new_file(cert_file, "r");
    if (bio == nullptr) {
      logger.Log(LogLevel::Warning,
                 fmt::format("SSL cert check: failed to open '{}' as BIO: {}",
                             cert_file, GetOpenSslError()));
      return false;
    }

    int cert_count = 0;
    while (true) {
      X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
      if (cert == nullptr) {
        break;
      }
      X509_free(cert);
      cert_count++;
    }

    BIO_free(bio);

    // Clear the "no more certs" error that PEM_read_bio_X509 leaves behind
    ERR_clear_error();

    logger.Log(LogLevel::Information,
               fmt::format("SSL cert check: parsed {} PEM certificate(s) from '{}'.",
                           cert_count, cert_file));

    if (cert_count == 0) {
      logger.Log(LogLevel::Warning,
                 fmt::format("SSL cert check: no valid PEM certificates found in '{}'.",
                             cert_file));
      return false;
    }

    // Step 4: Verify OpenSSL can load the file as a trust store
    SSL_CTX* ctx = SSL_CTX_new(TLS_method());
    if (ctx == nullptr) {
      logger.Log(LogLevel::Warning,
                 fmt::format("SSL cert check: SSL_CTX_new failed: {}", GetOpenSslError()));
      return false;
    }

    int load_result = SSL_CTX_load_verify_locations(ctx, cert_file, nullptr);
    if (load_result != 1) {
      std::string err = GetOpenSslError();
      SSL_CTX_free(ctx);
      logger.Log(LogLevel::Warning,
                 fmt::format("SSL cert check: SSL_CTX_load_verify_locations failed for '{}': {}",
                             cert_file, err));
      return false;
    }

    SSL_CTX_free(ctx);

    // Step 5: All checks passed
    logger.Log(LogLevel::Information,
               fmt::format("SSL certificate setup OK: {} certificate(s) loaded from '{}'.",
                           cert_count, cert_file));
    return true;

  } catch (const std::exception& ex) {
    logger.Log(LogLevel::Warning,
               fmt::format("SSL cert check: unexpected exception: {}", ex.what()));
    return false;
  } catch (...) {
    logger.Log(LogLevel::Warning, "SSL cert check: unexpected unknown exception");
    return false;
  }
}

}  // namespace fl

#endif  // __ANDROID__
