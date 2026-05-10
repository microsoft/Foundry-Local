// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "util/sha256.h"

#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt")

namespace fl {

std::string Sha256File(const std::filesystem::path& file_path) {
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return {};
  }

  BCRYPT_ALG_HANDLE alg = nullptr;
  BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
  if (!alg) {
    return {};
  }

  BCRYPT_HASH_HANDLE hash = nullptr;
  BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
  if (!hash) {
    BCryptCloseAlgorithmProvider(alg, 0);
    return {};
  }

  // Read file in 64KB chunks to avoid loading entire file into memory
  char buf[65536];
  while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
    BCryptHashData(hash, reinterpret_cast<PUCHAR>(buf), static_cast<ULONG>(file.gcount()), 0);
  }

  UCHAR digest[32];
  BCryptFinishHash(hash, digest, sizeof(digest), 0);
  BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(alg, 0);

  // Convert to uppercase hex
  std::ostringstream hex;
  hex << std::hex << std::uppercase << std::setfill('0');
  for (auto b : digest) {
    hex << std::setw(2) << static_cast<int>(b);
  }

  return hex.str();
}

}  // namespace fl

#else  // POSIX — use OpenSSL EVP API

#include <openssl/evp.h>

namespace fl {

std::string Sha256File(const std::filesystem::path& file_path) {
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return {};
  }

  auto* ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

  char buf[65536];
  while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
    EVP_DigestUpdate(ctx, buf, static_cast<size_t>(file.gcount()));
  }

  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0;
  EVP_DigestFinal_ex(ctx, digest, &digest_len);
  EVP_MD_CTX_free(ctx);

  std::ostringstream hex;
  hex << std::hex << std::uppercase << std::setfill('0');
  for (unsigned int i = 0; i < digest_len; ++i) {
    hex << std::setw(2) << static_cast<int>(digest[i]);
  }

  return hex.str();
}

}  // namespace fl

#endif
