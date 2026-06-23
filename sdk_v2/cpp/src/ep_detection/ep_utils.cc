// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ep_detection/ep_utils.h"

#include "logger.h"
#include "util/sha256.h"

#include <fmt/format.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace fl {

bool VerifyEpPackage(
    const std::filesystem::path& dir,
    std::initializer_list<std::pair<std::string_view, std::string_view>> expected,
    std::string_view ep_name,
    ILogger& logger) {
  for (const auto& [filename, expected_hash] : expected) {
    auto file_path = dir / filename;

    if (!std::filesystem::exists(file_path)) {
      return false;
    }

    auto hash = Sha256File(file_path);

    // Case-insensitive hex comparison
    if (!std::equal(hash.begin(), hash.end(), expected_hash.begin(), expected_hash.end(),
                    [](char a, char b) { return std::toupper(a) == std::toupper(b); })) {
      logger.Log(LogLevel::Warning,
                 fmt::format("{}: hash mismatch for {}: got {}, expected {}",
                             ep_name, filename, hash, expected_hash));
      return false;
    }
  }

  return true;
}

bool VerifyRsaSha256Signature(
    std::string_view data,
    std::string_view base64_sig,
    std::string_view public_key_pem,
    ILogger& logger) {
  // Load the RSA public key from PEM.
  BIO* key_bio = BIO_new_mem_buf(public_key_pem.data(), static_cast<int>(public_key_pem.size()));
  if (!key_bio) {
    logger.Log(LogLevel::Warning, "manifest signature: failed to allocate BIO for public key");
    return false;
  }
  EVP_PKEY* pkey = PEM_read_bio_PUBKEY(key_bio, nullptr, nullptr, nullptr);
  BIO_free(key_bio);
  if (!pkey) {
    logger.Log(LogLevel::Warning, "manifest signature: failed to parse public key PEM");
    return false; 
  }

  // Decode the base64 signature (single-line, no newlines).
  BIO* b64 = BIO_new(BIO_f_base64());
  if (!b64) {
    logger.Log(LogLevel::Warning, "manifest signature: failed to allocate BIO for base64");
    return false;
  }

  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  BIO* mem = BIO_new_mem_buf(base64_sig.data(), static_cast<int>(base64_sig.size()));
  if (!mem) {
    BIO_free(b64);
    logger.Log(LogLevel::Warning, "manifest signature: failed to allocate BIO for signature buffer");
    return false;
  }

  BIO_push(b64, mem);

  // Upper bound: base64 expands by ~4/3.
  std::vector<unsigned char> sig_bytes(base64_sig.size());
  int sig_len = BIO_read(b64, sig_bytes.data(), static_cast<int>(sig_bytes.size()));
  BIO_free_all(b64);

  if (sig_len <= 0) {
    EVP_PKEY_free(pkey);
    logger.Log(LogLevel::Warning, "manifest signature: failed to decode base64 signature");
    return false;
  }
  sig_bytes.resize(static_cast<size_t>(sig_len));

  // Verify RSA-SHA256-PKCS1v15.
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) {
    EVP_PKEY_free(pkey);
    logger.Log(LogLevel::Warning, "manifest signature: failed to allocate EVP_MD_CTX");
    return false;
  }

  bool ok = false;
  if (EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) == 1 &&
      EVP_DigestVerifyUpdate(ctx, data.data(), data.size()) == 1) {
    ok = (EVP_DigestVerifyFinal(ctx, sig_bytes.data(), sig_bytes.size()) == 1);
  }

  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(pkey);

  if (!ok) {
    logger.Log(LogLevel::Warning, "manifest signature: RSA-SHA256 verification failed");
  }

  return ok;
}

}  // namespace fl
