// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <filesystem>
#include <initializer_list>
#include <string_view>
#include <utility>

namespace fl {

class ILogger;

/// Verify a set of binaries in @p dir all exist and match their expected SHA-256 hashes.
///
/// @param dir            Directory containing the extracted EP binaries.
/// @param expected       List of (filename, expected_sha256_hex) pairs.
/// @param ep_name        EP name used in warning log messages (e.g. "CUDA EP").
/// @param logger         Logger for diagnostic output.
/// @return true if every file exists and its hash matches; false otherwise.
bool VerifyEpPackage(
    const std::filesystem::path& dir,
    std::initializer_list<std::pair<std::string_view, std::string_view>> expected,
    std::string_view ep_name,
    ILogger& logger);

/// Verify an RSA-SHA256-PKCS1v15 detached signature over @p data.
///
/// @param data            The signed data bytes (e.g. raw manifest JSON text).
/// @param base64_sig      Base64-encoded RSA signature (single line, no newlines).
/// @param public_key_pem  PEM-encoded RSA public key (-----BEGIN PUBLIC KEY----- block).
/// @param logger          Logger for diagnostic output.
/// @return true if the signature is valid; false otherwise.
bool VerifyRsaSha256Signature(
    std::string_view data,
    std::string_view base64_sig,
    std::string_view public_key_pem,
    ILogger& logger);

}  // namespace fl
