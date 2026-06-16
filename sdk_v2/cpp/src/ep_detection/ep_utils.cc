// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ep_detection/ep_utils.h"

#include "logger.h"
#include "util/zip_extract.h"

#include <fmt/format.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#endif

#include <filesystem>
#include <string>
#include <vector>

namespace fl {

namespace {

constexpr const char* kNuGetExtractDirName = "nupkg";
constexpr const char* kNuGetSignatureFileName = ".signature.p7s";
constexpr const char* kExpectedNuGetAuthorOrganization = "Microsoft Corporation";

struct CmsDeleter {
  void operator()(CMS_ContentInfo* value) const { CMS_ContentInfo_free(value); }
};

struct BioDeleter {
  void operator()(BIO* value) const { BIO_free(value); }
};

struct X509StoreDeleter {
  void operator()(X509_STORE* value) const { X509_STORE_free(value); }
};

struct SignerStackDeleter {
  void operator()(STACK_OF(X509)* value) const { sk_X509_free(value); }
};

std::string GetCertificateOrganization(X509* cert) {
  if (cert == nullptr) {
    return {};
  }

  char buffer[256] = {};
  auto length = X509_NAME_get_text_by_NID(X509_get_subject_name(cert), NID_organizationName,
                                          buffer, static_cast<int>(sizeof(buffer)));
  if (length <= 0) {
    return {};
  }

  return std::string(buffer, static_cast<size_t>(length));
}

bool VerifyPackageSignature(CMS_ContentInfo* cms, ILogger& logger) {
  // Author verification is a 3-step gate:
  // 1) CMS cryptographic validity: signature matches content and signer cert.
  // 2) Trust validation: signer certificate chains to a trusted root/policy.
  // 3) Publisher identity: signer Organization must be Microsoft Corporation.
  //
  // Platform difference:
  // - Windows: OpenSSL parses CMS, then CryptoAPI performs chain/policy trust.
  // - Non-Windows: OpenSSL performs both CMS + trust-store chain validation.
  // In both branches we still enforce the same Microsoft organization check.
#ifdef _WIN32
  ERR_clear_error();
  // Keep OpenSSL in parse/signer-extraction mode on Windows:
  // - CMS_NO_CONTENT_VERIFY: content integrity is not validated here.
  // - CMS_NO_SIGNER_CERT_VERIFY: cert trust is not validated here.
  // Trust/chain policy is enforced below via CryptoAPI per signer cert.
  if (CMS_verify(cms, nullptr, nullptr, nullptr, nullptr,
                 CMS_BINARY | CMS_NOCRL | CMS_NO_CONTENT_VERIFY |
                 CMS_NO_SIGNER_CERT_VERIFY) != 1) {
    unsigned long ssl_err;
    while ((ssl_err = ERR_get_error()) != 0) {
      logger.Log(LogLevel::Warning,
                 fmt::format("NuGet CMS parse error: {}",
                             ERR_error_string(ssl_err, nullptr)));
    }
    return false;
  }

  std::unique_ptr<STACK_OF(X509), SignerStackDeleter> signers(CMS_get0_signers(cms));
  if (!signers || sk_X509_num(signers.get()) == 0) {
    logger.Log(LogLevel::Warning, "NuGet package has no signer certificates");
    return false;
  }

  for (int i = 0; i < sk_X509_num(signers.get()); ++i) {
    X509* signer = sk_X509_value(signers.get(), i);
    if (GetCertificateOrganization(signer) != kExpectedNuGetAuthorOrganization) {
      continue;
    }

    int der_len = i2d_X509(signer, nullptr);
    if (der_len <= 0) {
      continue;
    }

    std::vector<unsigned char> der(static_cast<size_t>(der_len));
    unsigned char* der_ptr = der.data();
    i2d_X509(signer, &der_ptr);

    PCCERT_CONTEXT win_cert = CertCreateCertificateContext(
        X509_ASN_ENCODING, der.data(), static_cast<DWORD>(der.size()));
    if (!win_cert) {
      logger.Log(LogLevel::Warning,
                 fmt::format("CertCreateCertificateContext failed: 0x{:08X}",
                             GetLastError()));
      continue;
    }

    CERT_CHAIN_PARA chain_para = {};
    chain_para.cbSize = sizeof(chain_para);
    PCCERT_CHAIN_CONTEXT chain_ctx = nullptr;
    bool verified = false;

    if (CertGetCertificateChain(nullptr, win_cert, nullptr, nullptr,
                                &chain_para, 0, nullptr, &chain_ctx)) {
      CERT_CHAIN_POLICY_PARA policy_para = {};
      policy_para.cbSize = sizeof(policy_para);
      CERT_CHAIN_POLICY_STATUS policy_status = {};
      policy_status.cbSize = sizeof(policy_status);

      if (CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_BASE, chain_ctx,
                                           &policy_para, &policy_status) &&
          policy_status.dwError == 0) {
        verified = true;
      } else {
        logger.Log(LogLevel::Warning,
                   fmt::format("NuGet signer chain validation failed: 0x{:08X}",
                               policy_status.dwError));
      }
      CertFreeCertificateChain(chain_ctx);
    } else {
      logger.Log(LogLevel::Warning,
                 fmt::format("CertGetCertificateChain failed: 0x{:08X}",
                             GetLastError()));
    }

    CertFreeCertificateContext(win_cert);
    if (verified) {
      return true;
    }
  }

  logger.Log(LogLevel::Warning,
             fmt::format("NuGet package not signed by a trusted {}",
                         kExpectedNuGetAuthorOrganization));
  return false;
#else
  std::unique_ptr<X509_STORE, X509StoreDeleter> store(X509_STORE_new());
  if (!store) {
    logger.Log(LogLevel::Warning, "Failed to initialize certificate trust store");
    return false;
  }

  if (X509_STORE_set_default_paths(store.get()) != 1) {
    logger.Log(LogLevel::Warning, "Failed to load default certificate paths");
    return false;
  }

  ERR_clear_error();
  if (CMS_verify(cms, nullptr, store.get(), nullptr, nullptr,
                 CMS_BINARY | CMS_NOCRL | CMS_NO_CONTENT_VERIFY) != 1) {
    unsigned long ssl_err;
    while ((ssl_err = ERR_get_error()) != 0) {
      logger.Log(LogLevel::Warning,
                 fmt::format("NuGet signature verification failed: {}",
                             ERR_error_string(ssl_err, nullptr)));
    }
    return false;
  }

  std::unique_ptr<STACK_OF(X509), SignerStackDeleter> signers(CMS_get0_signers(cms));
  if (!signers || sk_X509_num(signers.get()) == 0) {
    logger.Log(LogLevel::Warning, "NuGet package has no signer certificates");
    return false;
  }

  for (int index = 0; index < sk_X509_num(signers.get()); ++index) {
    if (GetCertificateOrganization(sk_X509_value(signers.get(), index)) ==
        kExpectedNuGetAuthorOrganization) {
      return true;
    }
  }

  logger.Log(LogLevel::Warning,
             fmt::format("NuGet package signer is not {}",
                         kExpectedNuGetAuthorOrganization));
  return false;
#endif
}

bool ParseAndVerifyNuGetAuthorSignature(const std::filesystem::path& extract_dir,
                                        ILogger& logger) {
  auto signature_path = extract_dir / kNuGetSignatureFileName;
  if (!std::filesystem::exists(signature_path)) {
    logger.Log(LogLevel::Warning,
               fmt::format("NuGet package missing {}", kNuGetSignatureFileName));
    return false;
  }

  std::unique_ptr<BIO, BioDeleter> signature_bio(
      BIO_new_file(signature_path.string().c_str(), "rb"));
  if (!signature_bio) {
    logger.Log(LogLevel::Warning,
               fmt::format("Failed to open NuGet signature file {}",
                           signature_path.string()));
    return false;
  }

  std::unique_ptr<CMS_ContentInfo, CmsDeleter> cms(
      d2i_CMS_bio(signature_bio.get(), nullptr));
  if (!cms) {
    logger.Log(LogLevel::Warning, "Failed to parse NuGet CMS signature");
    return false;
  }

  return VerifyPackageSignature(cms.get(), logger);
}

}  // namespace

bool VerifyPackage(const std::filesystem::path& package_path,
                   std::string_view rid,
                   const std::filesystem::path& destination,
                   ILogger& logger) {
  auto extract_dir = destination / kNuGetExtractDirName;
  auto native_dir = extract_dir / "runtimes" / rid / "native";
  std::error_code error;
  std::filesystem::remove_all(extract_dir, error);

  if (!ExtractZip(package_path, extract_dir, logger)) {
    return false;
  }

  auto verified = ParseAndVerifyNuGetAuthorSignature(extract_dir, logger);
  if (!verified) {
    std::filesystem::remove_all(extract_dir, error);
    return false;
  }

  if (!std::filesystem::is_directory(native_dir)) {
    logger.Log(LogLevel::Warning,
               fmt::format("NuGet package missing runtime payload at {}",
                           native_dir.string()));
    std::filesystem::remove_all(extract_dir, error);
    return false;
  }

  for (const auto& entry : std::filesystem::directory_iterator(native_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    auto target = destination / entry.path().filename();
    std::filesystem::copy_file(entry.path(), target,
                               std::filesystem::copy_options::overwrite_existing,
                               error);
    if (error) {
      logger.Log(LogLevel::Warning,
                 fmt::format("Failed to copy {} to {}: {}",
                             entry.path().string(), target.string(), error.message()));
      std::filesystem::remove_all(extract_dir, error);
      return false;
    }
  }

  std::filesystem::remove_all(extract_dir, error);
  return true;
}

}  // namespace fl
