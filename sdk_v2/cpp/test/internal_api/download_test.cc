// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Tests for the download infrastructure:
// - ModelRegistryClient (SAS URI resolution)
// - DownloadBlobsToDirectory (blob filtering and orchestration)
// - InferenceModelWriter (inference_model.json)
// - FixVariantInferenceModelJson (variant fixup)
// - DownloadManager (full flow orchestration)
#if defined(FOUNDRY_LOCAL_HAVE_LIVE_CATALOG_CLIENT)
#include "catalog/azure_catalog_client.h"
#endif
#include "catalog/azure_catalog_models.h"
#include "download/blob_downloader.h"
#include "download/download_manager.h"
#include "download/inference_model_writer.h"
#include "download/model_registry_client.h"
#include "ep_detection/ep_detector.h"
#include "exception.h"
#include "logger.h"
#include "model_info.h"
#include "test_helpers.h"
#include "util/path_safety.h"
#include <foundry_local/foundry_local_c.h>
#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace fl;

// ========================================================================
// Test helpers
// ========================================================================

namespace {

/// Create a temporary directory for test isolation.
class TempDir {
 public:
  TempDir() {
    path_ = fs::temp_directory_path() / ("fl_test_" + std::to_string(std::hash<std::string>{}(
        std::to_string(reinterpret_cast<uintptr_t>(this)) +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))));
    fs::create_directories(path_);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  const fs::path& path() const { return path_; }
  std::string string() const { return path_.string(); }

 private:
  fs::path path_;
};

/// Read entire file contents.
std::string ReadFile(const fs::path& path) {
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

/// Mock blob downloader for testing download orchestration.
class MockBlobDownloader : public IBlobDownloader {
 public:
  std::vector<BlobItemInfo> blobs_to_return;
  std::vector<std::string> downloaded_blobs;  // names of blobs that were "downloaded"
  std::string expected_sas_uri;

  std::vector<BlobItemInfo> ListBlobs(const std::string& sas_uri) override {
    if (!expected_sas_uri.empty()) {
      EXPECT_EQ(sas_uri, expected_sas_uri);
    }
    return blobs_to_return;
  }

  void DownloadBlob(const std::string& /*sas_uri*/,
                    const std::string& blob_name,
                    const std::string& local_path,
                    int /*max_concurrency*/,
                    BlobBytesWrittenFn bytes_written_cb = nullptr,
                    std::atomic<bool>* /*cancelled*/ = nullptr) override {
    downloaded_blobs.push_back(blob_name);
    // Create a file so the test can verify it exists
    auto parent = fs::path(local_path).parent_path();
    if (!parent.empty()) {
      fs::create_directories(parent);
    }
    std::ofstream f(local_path);
    f << "mock content for " << blob_name;

    // Report byte count for progress tracking (content_length from the matching blob)
    if (bytes_written_cb) {
      for (const auto& b : blobs_to_return) {
        if (b.name == blob_name) {
          bytes_written_cb(b.content_length);
          break;
        }
      }
    }
  }
};

/// Mock that performs basic downloads but lets progress-callback exceptions propagate.
/// Used to test that cancellation from the progress callback stops further blobs.
class CancellingMockDownloader : public IBlobDownloader {
 public:
  int download_count = 0;
  std::vector<BlobItemInfo> blobs_to_return;

  std::vector<BlobItemInfo> ListBlobs(const std::string&) override {
    return blobs_to_return;
  }

  void DownloadBlob(const std::string&, const std::string&,
                    const std::string& local_path, int,
                    BlobBytesWrittenFn bytes_written_cb,
                    std::atomic<bool>* /*cancelled*/) override {
    ++download_count;

    auto parent = fs::path(local_path).parent_path();
    if (!parent.empty()) {
      fs::create_directories(parent);
    }

    std::ofstream f(local_path);
    f << "content";

    // Report progress so the orchestrator's per-chunk callback fires.
    if (bytes_written_cb) {
      bytes_written_cb(100);
    }
  }
};

/// Mock that catches callback exceptions and then checks the cancelled flag,
/// simulating how AzureBlobDownloader detects cancellation between chunks.
class CancelCheckingMockDownloader : public IBlobDownloader {
 public:
  int download_count = 0;
  std::vector<BlobItemInfo> blobs_to_return;

  std::vector<BlobItemInfo> ListBlobs(const std::string&) override {
    return blobs_to_return;
  }

  void DownloadBlob(const std::string&, const std::string&,
                    const std::string& local_path, int,
                    BlobBytesWrittenFn bytes_written_cb,
                    std::atomic<bool>* cancelled) override {
    ++download_count;

    auto parent = fs::path(local_path).parent_path();
    if (!parent.empty()) {
      fs::create_directories(parent);
    }

    std::ofstream f(local_path);
    f << "content";

    // Call the progress callback, catching any cancellation exception
    // (simulating chunk-level error handling in a real downloader).
    if (bytes_written_cb) {
      try {
        bytes_written_cb(100);
      } catch (const fl::Exception&) {
        // Swallowed — real downloader might handle this per-chunk.
      }
    }

    // Check the cancelled flag as AzureBlobDownloader would between chunks.
    if (cancelled && cancelled->load(std::memory_order_relaxed)) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_OPERATION_CANCELLED, "download cancelled");
    }
  }
};

#if defined(FOUNDRY_LOCAL_HAVE_LIVE_CATALOG_CLIENT)
/// EP detector returning all device types so the catalog returns the full model list.
class AllDevicesEpDetector : public IEpDetector {
 public:
  std::map<std::string, std::vector<std::string>> GetAvailableDevicesToEPs() const override {
    return {
        {"CPU", {"CPUExecutionProvider"}},
        {"GPU", {"CUDAExecutionProvider"}},
        {"NPU", {"QNNExecutionProvider"}},
    };
  }
};
#endif  // FOUNDRY_LOCAL_HAVE_LIVE_CATALOG_CLIENT

}  // anonymous namespace

// ========================================================================
// ModelRegistryClient tests
// ========================================================================

TEST(ModelRegistryClientTest, ResolvesModelContainerFromJson) {
  ModelRegistryClient client("eastus", fl::test::NullLog());
  client.SetHttpGet([](const std::string& url) -> std::string {
    // Verify the URL is correctly formed
    EXPECT_TRUE(url.find("assetId=") != std::string::npos);
    return R"({
      "blobSasUri": "https://storage.blob.core.windows.net/container?sv=2023-01-01&sig=abc",
      "modelEntity": {
        "description": "A test model"
      }
    })";
  });

  auto container = client.ResolveModelContainer("azureml://registries/test/models/phi-3");
  EXPECT_EQ(container.blob_sas_uri,
            "https://storage.blob.core.windows.net/container?sv=2023-01-01&sig=abc");
  EXPECT_EQ(container.description, "A test model");
}

TEST(ModelRegistryClientTest, ThrowsOnEmptyAssetId) {
  ModelRegistryClient client("eastus", fl::test::NullLog());
  EXPECT_THROW(client.ResolveModelContainer(""), fl::Exception);
}

TEST(ModelRegistryClientTest, ThrowsOnMissingSasUri) {
  ModelRegistryClient client("eastus", fl::test::NullLog());
  client.SetHttpGet([](const std::string&) -> std::string {
    return R"({"modelEntity": {"description": "no sas uri"}})";
  });
  EXPECT_THROW(client.ResolveModelContainer("azureml://test"), fl::Exception);
}

TEST(ModelRegistryClientTest, ThrowsOnEmptyResponse) {
  ModelRegistryClient client("eastus", fl::test::NullLog());
  client.SetHttpGet([](const std::string&) -> std::string { return ""; });
  EXPECT_THROW(client.ResolveModelContainer("azureml://test"), fl::Exception);
}

TEST(ModelRegistryClientTest, ThrowsOnMalformedJson) {
  ModelRegistryClient client("eastus", fl::test::NullLog());
  client.SetHttpGet([](const std::string&) -> std::string { return "not json"; });
  EXPECT_THROW(client.ResolveModelContainer("azureml://test"), fl::Exception);
}

TEST(ModelRegistryClientTest, HandlesOptionalDescription) {
  ModelRegistryClient client("eastus", fl::test::NullLog());
  client.SetHttpGet([](const std::string&) -> std::string {
    return R"({"blobSasUri": "https://example.com/blob?sig=x"})";
  });
  auto container = client.ResolveModelContainer("azureml://test");
  EXPECT_EQ(container.blob_sas_uri, "https://example.com/blob?sig=x");
  EXPECT_TRUE(container.description.empty());
}

TEST(ModelRegistryClientTest, UrlEncodesAssetId) {
  ModelRegistryClient client("eastus", fl::test::NullLog());
  std::string captured_url;
  client.SetHttpGet([&captured_url](const std::string& url) -> std::string {
    captured_url = url;
    return R"({"blobSasUri": "https://example.com/blob"})";
  });
  client.ResolveModelContainer("azureml://registries/test models/v1");
  // Spaces should be encoded as %20
  EXPECT_TRUE(captured_url.find("%20") != std::string::npos);
  EXPECT_TRUE(captured_url.find(" ") == std::string::npos);
}

TEST(ModelRegistryClientTest, Region_DefaultIsEastUs) {
  ModelRegistryClient client("eastus", fl::test::NullLog());
  std::string captured_url;
  client.SetHttpGet([&captured_url](const std::string& url) -> std::string {
    captured_url = url;
    return R"({"blobSasUri": "https://example.com/blob"})";
  });
  client.ResolveModelContainer("azureml://test");
  EXPECT_TRUE(captured_url.find("eastus.api.azureml.ms") != std::string::npos)
      << "Expected URL to target eastus region by default. Got: " << captured_url;
}

TEST(ModelRegistryClientTest, Region_CustomRegion) {
  ModelRegistryClient client("westeurope", fl::test::NullLog());
  std::string captured_url;
  client.SetHttpGet([&captured_url](const std::string& url) -> std::string {
    captured_url = url;
    return R"({"blobSasUri": "https://example.com/blob"})";
  });
  client.ResolveModelContainer("azureml://test");
  EXPECT_TRUE(captured_url.find("westeurope.api.azureml.ms") != std::string::npos)
      << "Expected URL to target westeurope region. Got: " << captured_url;
  EXPECT_TRUE(captured_url.find("eastus.api.azureml.ms") == std::string::npos)
      << "Expected URL to NOT contain eastus. Got: " << captured_url;
}

// ========================================================================
// Blob download orchestration tests
// ========================================================================

TEST(BlobDownloadTest, DownloadsAllBlobs) {
  TempDir tmpdir;
  MockBlobDownloader mock;
  mock.blobs_to_return = {
      {"model/weights.safetensors", 1000},
      {"model/config.json", 100},
  };

  BlobDownloadOptions opts;
  opts.path_prefix = "model";
  DownloadBlobsToDirectory(mock, "https://test.blob/container?sig=x", tmpdir.string(), opts);

  EXPECT_EQ(mock.downloaded_blobs.size(), 2u);
}

TEST(BlobDownloadTest, FiltersByPathPrefix) {
  TempDir tmpdir;
  MockBlobDownloader mock;
  mock.blobs_to_return = {
      {"variant-a/weights.safetensors", 1000},
      {"variant-b/other.bin", 500},
      {"unrelated/file.txt", 200},
  };

  BlobDownloadOptions opts;
  opts.path_prefix = "variant-a";
  DownloadBlobsToDirectory(mock, "https://test.blob/c?sig=x", tmpdir.string(), opts);

  ASSERT_EQ(mock.downloaded_blobs.size(), 1u);
  EXPECT_EQ(mock.downloaded_blobs[0], "variant-a/weights.safetensors");
}

TEST(BlobDownloadTest, FiltersOutInferenceModelJson) {
  TempDir tmpdir;
  MockBlobDownloader mock;
  mock.blobs_to_return = {
      {"weights.safetensors", 1000},
      {"inference_model.json", 50},
      {"config.json", 100},
  };

  BlobDownloadOptions opts;
  DownloadBlobsToDirectory(mock, "https://test.blob/c?sig=x", tmpdir.string(), opts);

  ASSERT_EQ(mock.downloaded_blobs.size(), 2u);
  for (const auto& name : mock.downloaded_blobs) {
    EXPECT_NE(name, "inference_model.json") << "Should filter out inference_model.json";
  }
}

TEST(BlobDownloadTest, ReportsProgress) {
  TempDir tmpdir;
  MockBlobDownloader mock;
  mock.blobs_to_return = {
      {"file1.bin", 500},
      {"file2.bin", 500},
  };

  std::vector<float> progress_values;
  BlobDownloadOptions opts;
  opts.progress = [&progress_values](float percent) {
    progress_values.push_back(percent);
    return 0;
  };

  DownloadBlobsToDirectory(mock, "https://test.blob/c?sig=x", tmpdir.string(), opts);

  // Should get progress reports, ending at 100
  ASSERT_FALSE(progress_values.empty());
  EXPECT_FLOAT_EQ(progress_values.back(), 100.0f);

  // All progress values should be in [0, 100]
  for (float v : progress_values) {
    EXPECT_GE(v, 0.0f);
    EXPECT_LE(v, 100.0f);
  }
}

TEST(BlobDownloadTest, HandlesEmptyBlobList) {
  TempDir tmpdir;
  MockBlobDownloader mock;
  // No blobs

  BlobDownloadOptions opts;
  DownloadBlobsToDirectory(mock, "https://test.blob/c?sig=x", tmpdir.string(), opts);

  EXPECT_TRUE(mock.downloaded_blobs.empty());
}

// ========================================================================
// Path-traversal hardening (security)
// ========================================================================

TEST(IsPathWithinDirectoryTest, AcceptsChildPath) {
  fs::path root = fs::temp_directory_path() / "fl_root_a";
  EXPECT_TRUE(IsPathWithinDirectory(root / "child" / "leaf.bin", root));
}

TEST(IsPathWithinDirectoryTest, AcceptsRootItself) {
  fs::path root = fs::temp_directory_path() / "fl_root_b";
  EXPECT_TRUE(IsPathWithinDirectory(root, root));
}

TEST(IsPathWithinDirectoryTest, RejectsParentEscape) {
  fs::path root = fs::temp_directory_path() / "fl_root_c";
  EXPECT_FALSE(IsPathWithinDirectory(root / ".." / "evil.bin", root));
}

TEST(IsPathWithinDirectoryTest, RejectsDeepParentEscape) {
  fs::path root = fs::temp_directory_path() / "fl_root_d" / "sub";
  EXPECT_FALSE(IsPathWithinDirectory(root / ".." / ".." / ".." / "evil.bin", root));
}

TEST(IsPathWithinDirectoryTest, RejectsSiblingPrefixCollision) {
  // Guard against naive string prefix matching: "/foo/bar2" must not be
  // considered inside "/foo/bar".
  fs::path root = fs::temp_directory_path() / "fl_root_bar";
  fs::path sibling = fs::temp_directory_path() / "fl_root_bar2" / "leaf.bin";
  EXPECT_FALSE(IsPathWithinDirectory(sibling, root));
}

TEST(BlobDownloadTest, RejectsPathTraversalBlobName) {
  TempDir tmpdir;
  MockBlobDownloader mock;
  mock.blobs_to_return = {
      {"../evil.bin", 4},
  };

  BlobDownloadOptions opts;
  EXPECT_THROW(DownloadBlobsToDirectory(mock, "https://test.blob/c?sig=x", tmpdir.string(), opts),
               fl::Exception);
  EXPECT_TRUE(mock.downloaded_blobs.empty());
}

TEST(BlobDownloadTest, RejectsBackslashPathTraversalBlobName) {
  TempDir tmpdir;
  MockBlobDownloader mock;
  mock.blobs_to_return = {
      {"..\\evil.bin", 4},
  };

  BlobDownloadOptions opts;
  EXPECT_THROW(DownloadBlobsToDirectory(mock, "https://test.blob/c?sig=x", tmpdir.string(), opts),
               fl::Exception);
  EXPECT_TRUE(mock.downloaded_blobs.empty());
}

TEST(BlobDownloadTest, RejectsNestedPathTraversalBlobName) {
  TempDir tmpdir;
  MockBlobDownloader mock;
  mock.blobs_to_return = {
      {"good/../../evil.bin", 4},
  };

  BlobDownloadOptions opts;
  EXPECT_THROW(DownloadBlobsToDirectory(mock, "https://test.blob/c?sig=x", tmpdir.string(), opts),
               fl::Exception);
  EXPECT_TRUE(mock.downloaded_blobs.empty());
}

TEST(BlobDownloadTest, CancellationStopsRemainingBlobs) {
  TempDir tmpdir;
  CancellingMockDownloader mock;
  mock.blobs_to_return = {
      {"blob1.bin", 100},
      {"blob2.bin", 100},
      {"blob3.bin", 100},
  };

  BlobDownloadOptions opts;
  opts.progress = [](float) {
    return 1;  // Cancel immediately on first progress report
  };

  try {
    DownloadBlobsToDirectory(mock, "https://test.blob/c?sig=x", tmpdir.string(), opts);
    FAIL() << "Expected fl::Exception to be thrown";
  } catch (const fl::Exception& e) {
    EXPECT_EQ(e.code(), FOUNDRY_LOCAL_ERROR_OPERATION_CANCELLED);
  }

  EXPECT_EQ(mock.download_count, 0)
      << "No blobs should have been downloaded — cancellation fired at 0% before any blob started";
}

TEST(BlobDownloadTest, CancelledFlagAbortsInFlightDownload) {
  TempDir tmpdir;
  CancelCheckingMockDownloader mock;
  mock.blobs_to_return = {
      {"blob1.bin", 100},
      {"blob2.bin", 100},
  };

  // Allow the initial 0% progress through, then cancel on the first chunk progress.
  // This ensures DownloadBlob is actually called so the mock can verify the cancelled flag.
  int call_count = 0;
  BlobDownloadOptions opts;
  opts.progress = [&call_count](float) {
    return (++call_count > 1) ? 1 : 0;
  };

  try {
    DownloadBlobsToDirectory(mock, "https://test.blob/c?sig=x", tmpdir.string(), opts);
    FAIL() << "Expected fl::Exception to be thrown";
  } catch (const fl::Exception& e) {
    EXPECT_EQ(e.code(), FOUNDRY_LOCAL_ERROR_OPERATION_CANCELLED);
  }

  // The mock caught the callback exception and checked the cancelled flag —
  // only one blob should have been attempted.
  EXPECT_EQ(mock.download_count, 1);
}

// ========================================================================
// Inference model writer tests
// ========================================================================

TEST(InferenceModelWriterTest, WritesJsonWithPromptTemplate) {
  TempDir tmpdir;
  fl::KeyValuePairs templates;
  templates.Add("system", "You are a helpful assistant.");
  templates.Add("user", "{input}");
  WriteInferenceModelJson(tmpdir.string(), "test-model", templates);

  auto content = ReadFile(tmpdir.path() / "inference_model.json");
  auto j = nlohmann::json::parse(content);

  EXPECT_EQ(j["Name"], "test-model");
  EXPECT_EQ(j["PromptTemplate"]["system"], "You are a helpful assistant.");
  EXPECT_EQ(j["PromptTemplate"]["user"], "{input}");
}

TEST(InferenceModelWriterTest, WritesNullPromptTemplateWhenEmpty) {
  TempDir tmpdir;
  fl::KeyValuePairs templates;
  WriteInferenceModelJson(tmpdir.string(), "test-model", templates);

  auto content = ReadFile(tmpdir.path() / "inference_model.json");
  auto j = nlohmann::json::parse(content);

  EXPECT_EQ(j["Name"], "test-model");
  EXPECT_TRUE(j["PromptTemplate"].is_null());
}

// ========================================================================
// Variant fixup tests
// ========================================================================

TEST(VariantFixupTest, CopiesInferenceModelToSubdirs) {
  TempDir tmpdir;
  const auto& root = tmpdir.path();

  // Create inference_model.json at root
  {
    std::ofstream f(root / "inference_model.json");
    f << R"({"Name": "test"})";
  }

  // Create subdirectories (simulating variant blobs)
  fs::create_directories(root / "variant-a");
  fs::create_directories(root / "variant-b");

  FixVariantInferenceModelJson(root.string());

  // inference_model.json should be in both subdirs
  EXPECT_TRUE(fs::exists(root / "variant-a" / "inference_model.json"));
  EXPECT_TRUE(fs::exists(root / "variant-b" / "inference_model.json"));
  // Root copy should be deleted
  EXPECT_FALSE(fs::exists(root / "inference_model.json"));
}

TEST(VariantFixupTest, DoesNotOverwriteExisting) {
  TempDir tmpdir;
  const auto& root = tmpdir.path();

  {
    std::ofstream f(root / "inference_model.json");
    f << R"({"Name": "root"})";
  }

  fs::create_directories(root / "variant-a");
  {
    std::ofstream f(root / "variant-a" / "inference_model.json");
    f << R"({"Name": "existing"})";
  }

  FixVariantInferenceModelJson(root.string());

  // Existing file should not be overwritten
  auto content = ReadFile(root / "variant-a" / "inference_model.json");
  auto j = nlohmann::json::parse(content);
  EXPECT_EQ(j["Name"], "existing");
}

TEST(VariantFixupTest, NoOpWhenNoRootFile) {
  TempDir tmpdir;
  fs::create_directories(tmpdir.path() / "sub");

  // Should not throw
  FixVariantInferenceModelJson(tmpdir.string());
  EXPECT_FALSE(fs::exists(tmpdir.path() / "sub" / "inference_model.json"));
}

TEST(VariantFixupTest, PreservesRootFileWhenNoSubdirs) {
  // Single-variant downloads put every blob (and inference_model.json) at the root
  // with no variant subdirectory. The fixup must not delete the root file in that
  // case, otherwise IsModelCached would report false on the next check.
  TempDir tmpdir;
  const auto& root = tmpdir.path();

  {
    std::ofstream f(root / "inference_model.json");
    f << R"({"Name": "root-only"})";
  }

  // Add a sibling blob file (not a directory) to make sure the iterator's
  // is_directory() filter doesn't accidentally count it as a variant.
  {
    std::ofstream f(root / "weights.safetensors");
    f << "blob";
  }

  FixVariantInferenceModelJson(root.string());

  EXPECT_TRUE(fs::exists(root / "inference_model.json"));
  auto content = ReadFile(root / "inference_model.json");
  auto j = nlohmann::json::parse(content);
  EXPECT_EQ(j["Name"], "root-only");
}

// ========================================================================
// DownloadManager tests
// ========================================================================

TEST(DownloadManagerTest, FullDownloadFlow) {
  TempDir tmpdir;

  auto manager = std::make_unique<DownloadManager>(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  // Mock the registry client
  auto registry = std::make_unique<ModelRegistryClient>("eastus", fl::test::NullLog());
  registry->SetHttpGet([](const std::string&) -> std::string {
    return R"({"blobSasUri": "https://storage.blob.core.windows.net/container?sig=test"})";
  });
  manager->SetModelRegistryClient(std::move(registry));

  // Mock the blob downloader
  auto mock_downloader = std::make_unique<MockBlobDownloader>();
  mock_downloader->expected_sas_uri =
      "https://storage.blob.core.windows.net/container?sig=test";
  mock_downloader->blobs_to_return = {
      {"weights.safetensors", 1024},
      {"config.json", 100},
  };
  manager->SetBlobDownloader(std::move(mock_downloader));

  ModelInfo info;
  info.model_id = "test-model:1";
  info.name = "test-model";
  info.uri = "azureml://registries/test/models/test-model/versions/1";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "TestPublisher";

  std::vector<float> progress_values;
  auto path = manager->DownloadModel(info, [&](float p) {
    progress_values.push_back(p);
    return 0;
  });

  // Verify the model path
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(fs::exists(path));

  // Verify inference_model.json was written (possibly moved by variant fixup)
  // The download signal file should be removed
  EXPECT_FALSE(fs::exists(fs::path(path) / "download.tmp"));

  // Verify progress was reported
  EXPECT_FALSE(progress_values.empty());
}

TEST(DownloadManagerTest, SkipsAlreadyCachedModel) {
  TempDir tmpdir;
  auto manager = std::make_unique<DownloadManager>(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "cached-model:1";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Publisher";

  // Pre-create the model directory (simulating an already-cached model)
  auto model_dir = fs::path(tmpdir.string()) / "Publisher" / "cached-model-1";
  fs::create_directories(model_dir);
  {
    std::ofstream f(model_dir / "inference_model.json");
    f << "{}";
  }

  float final_progress = 0;
  auto path = manager->DownloadModel(info, [&](float p) {
    final_progress = p;
    return 0;
  });

  EXPECT_EQ(path, model_dir.string());
  EXPECT_FLOAT_EQ(final_progress, 100.0f);
}

TEST(DownloadManagerTest, IsModelCachedReturnsFalseForMissing) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "nonexistent:1";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Publisher";

  EXPECT_FALSE(manager.IsModelCached(info));
}

TEST(DownloadManagerTest, IsModelCachedReturnsFalseForIncomplete) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "incomplete:1";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Publisher";

  // Create directory with download.tmp (incomplete)
  auto model_dir = fs::path(tmpdir.string()) / "Publisher" / "incomplete-1";
  fs::create_directories(model_dir);
  { std::ofstream f(model_dir / "download.tmp"); }

  EXPECT_FALSE(manager.IsModelCached(info));
}

TEST(DownloadManagerTest, IsModelCachedReturnsTrueForComplete) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "complete:2";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Publisher";

  // Create directory with inference_model.json and no download.tmp (complete)
  auto model_dir = fs::path(tmpdir.string()) / "Publisher" / "complete-2";
  fs::create_directories(model_dir);
  {
    std::ofstream f(model_dir / "inference_model.json");
    f << "{}";
  }

  EXPECT_TRUE(manager.IsModelCached(info));
}

TEST(DownloadManagerTest, IsModelCachedReturnsFalseForEmptyDir) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "empty:1";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Publisher";

  // Create directory without inference_model.json — an empty directory from
  // a failed or aborted download should not be treated as cached.
  auto model_dir = fs::path(tmpdir.string()) / "Publisher" / "empty-1";
  fs::create_directories(model_dir);

  EXPECT_FALSE(manager.IsModelCached(info));
}

TEST(DownloadManagerTest, VersionSuffixConversion) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "mymodel:42";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Microsoft";

  auto path = manager.GetModelCachePath(info);
  // Path doesn't exist yet so should be empty
  EXPECT_TRUE(path.empty());

  // Create it and check the path uses '-' not ':'
  auto expected_dir = fs::path(tmpdir.string()) / "Microsoft" / "mymodel-42";
  fs::create_directories(expected_dir);

  path = manager.GetModelCachePath(info);
  EXPECT_EQ(path, expected_dir.string());
}

TEST(DownloadManagerTest, ThrowsOnEmptyUri) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "test:1";
  // No URI set

  EXPECT_THROW(manager.DownloadModel(info), fl::Exception);
}

// Concurrency: two threads downloading the same model must serialize so the second
// thread sees the cached result rather than re-downloading. Different models still
// proceed in parallel — covered by the unrelated-model test below.
TEST(DownloadManagerTest, ConcurrentDownloadsOfSameModelSerialize) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  auto registry = std::make_unique<ModelRegistryClient>("eastus", fl::test::NullLog());
  registry->SetHttpGet([](const std::string&) -> std::string {
    return R"({"blobSasUri": "https://storage.blob.core.windows.net/c?sig=test"})";
  });
  manager.SetModelRegistryClient(std::move(registry));

  // Counting mock — increments an atomic on every DownloadBlob call.
  class CountingDownloader : public IBlobDownloader {
   public:
    std::atomic<int> download_calls{0};
    std::atomic<int> list_calls{0};

    std::vector<BlobItemInfo> ListBlobs(const std::string&) override {
      ++list_calls;
      return {{"variant-cpu/weights.bin", 16}};
    }

    void DownloadBlob(const std::string&, const std::string& blob_name,
                      const std::string& local_path, int,
                      BlobBytesWrittenFn bytes_written_cb,
                      std::atomic<bool>*) override {
      ++download_calls;

      auto parent = fs::path(local_path).parent_path();
      if (!parent.empty()) {
        fs::create_directories(parent);
      }
      std::ofstream f(local_path);
      f << "data for " << blob_name;
      if (bytes_written_cb) {
        bytes_written_cb(16);
      }
    }
  };

  auto counting = std::make_unique<CountingDownloader>();
  auto* counting_raw = counting.get();
  manager.SetBlobDownloader(std::move(counting));

  ModelInfo info;
  info.model_id = "concurrent-model:1";
  info.name = "concurrent-model";
  info.uri = "azureml://registries/test/models/concurrent-model/versions/1";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Pub";

  constexpr int kThreadCount = 4;
  std::vector<std::thread> threads;
  std::vector<std::string> results(kThreadCount);
  std::atomic<int> exceptions{0};

  for (int i = 0; i < kThreadCount; ++i) {
    threads.emplace_back([&, i]() {
      try {
        results[i] = manager.DownloadModel(info);
      } catch (...) {
        ++exceptions;
      }
    });
  }
  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(exceptions.load(), 0);

  // Only ONE download should actually have run; the other three must have hit the cache.
  EXPECT_EQ(counting_raw->download_calls.load(), 1)
      << "Concurrent downloads of the same model must serialize and share the result.";

  // All four threads should report the same resolved path.
  for (int i = 1; i < kThreadCount; ++i) {
    EXPECT_EQ(results[i], results[0]);
  }
}

// HasInferenceModelJson must return false instead of throwing when the path
// it's asked about is not a directory (e.g. a regular file). Previously the
// underlying directory_iterator would throw filesystem_error.
TEST(DownloadManagerTest, IsModelCachedReturnsFalseWhenPathIsRegularFile) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "filemodel:1";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Pub";

  // Plant a regular file where the model directory would live.
  auto pub_dir = fs::path(tmpdir.string()) / "Pub";
  fs::create_directories(pub_dir);
  {
    std::ofstream f(pub_dir / "filemodel-1");
    f << "not a directory";
  }

  EXPECT_NO_THROW({
    EXPECT_FALSE(manager.IsModelCached(info));
  });
}

// ========================================================================
// End-to-end integration test — fetches catalog then downloads smallest model
// Only built when the live Azure catalog client is present in the source tree.
// Disabled by default. Run with: --gtest_also_run_disabled_tests
// ========================================================================

#if defined(FOUNDRY_LOCAL_HAVE_LIVE_CATALOG_CLIENT)
TEST(EndToEndTest, LiveCatalogAndDownload) {
  // 1. Fetch the full model list from the Azure catalog
  AllDevicesEpDetector ep;
  StderrLogger logger;
  AzureCatalogClient catalog("https://ai.azure.com/api/eastus/ux/v1.0", "''", ep, logger);
  auto models = catalog.FetchAllModelInfos();

  ASSERT_GT(models.size(), 50u)
      << "Expected 50+ models from the public catalog, got " << models.size();

  // 2. Find the smallest chat-completion model by filesize_mb
  const ModelInfo* smallest = nullptr;
  int64_t smallest_size = std::numeric_limits<int64_t>::max();

  for (const auto& m : models) {
    auto task_it = m.string_properties.find(FOUNDRY_LOCAL_MODEL_PROP_TASK_STR);
    if (task_it == m.string_properties.end() || task_it->second != "chat-completion") {
      continue;
    }

    auto size_it = m.int_properties.find(FOUNDRY_LOCAL_MODEL_PROP_FILESIZE_MB_INT);
    if (size_it == m.int_properties.end()) {
      continue;
    }

    if (size_it->second < smallest_size) {
      smallest_size = size_it->second;
      smallest = &m;
    }
  }

  ASSERT_NE(smallest, nullptr)
      << "No chat-completion model with a filesize_mb property found in catalog";

  std::cout << "\n=== Live Catalog + Download Test ==="
            << "\nSelected model: " << smallest->name
            << "\nAlias:          " << smallest->alias
            << "\nSize (MB):      " << smallest_size
            << "\nURI:            " << smallest->uri
            << "\n====================================\n";

  // 3. Download the model — use build output dir so reruns skip the download
  auto cache_path = fs::path(__FILE__).parent_path().parent_path() / "build" / "test_cache";
  fs::create_directories(cache_path);
  DownloadManager dm(cache_path.string());

  std::vector<float> progress_values;
  std::string local_path = dm.DownloadModel(*smallest, [&](float pct) {
    progress_values.push_back(pct);
  });

  // 4. Verify progress was reported and reached ~100%
  EXPECT_FALSE(progress_values.empty()) << "Progress callback was never called";
  if (!progress_values.empty()) {
    EXPECT_GE(progress_values.back(), 99.0f)
        << "Final progress should be ~100%, got " << progress_values.back();
  }

  // 5. Verify the download path exists on disk
  EXPECT_TRUE(fs::exists(local_path))
      << "Download path does not exist: " << local_path;

  // 6. Find inference_model.json — may be in root or a variant subdirectory
  fs::path inference_model_path;
  for (auto const& entry : fs::recursive_directory_iterator(local_path)) {
    if (entry.is_regular_file() && entry.path().filename() == "inference_model.json") {
      inference_model_path = entry.path();
      break;
    }
  }

  EXPECT_FALSE(inference_model_path.empty())
      << "inference_model.json not found anywhere under " << local_path;

  if (!inference_model_path.empty()) {
    // 7. Parse as JSON and validate the Name field
    std::string json_text = ReadFile(inference_model_path);
    nlohmann::json doc;
    EXPECT_NO_THROW(doc = nlohmann::json::parse(json_text))
        << "inference_model.json is not valid JSON";

    if (doc.is_object()) {
      EXPECT_TRUE(doc.contains("Name"))
          << "inference_model.json missing 'Name' field";
      if (doc.contains("Name")) {
        EXPECT_EQ(doc["Name"].get<std::string>(), smallest->model_id)
            << "Name mismatch between inference_model.json and catalog";
      }
    }
  }

  // 8. Verify no download.tmp signal file remains
  bool found_download_tmp = false;
  for (auto const& entry : fs::recursive_directory_iterator(local_path)) {
    if (entry.is_regular_file() && entry.path().filename() == "download.tmp") {
      found_download_tmp = true;
      break;
    }
  }
  EXPECT_FALSE(found_download_tmp)
      << "download.tmp signal file should be removed after a complete download";

  std::cout << "\nDownload path:          " << local_path
            << "\ninference_model.json:   " << inference_model_path.string()
            << "\nProgress callbacks:     " << progress_values.size()
            << "\n====================================\n";
}
#endif  // FOUNDRY_LOCAL_HAVE_LIVE_CATALOG_CLIENT

// ========================================================================
// Path-injection hardening (H9) — DownloadManager::ComputeModelPath must
// reject catalog inputs that could escape the cache root or alias another
// publisher/model on disk.
// ========================================================================

TEST(DownloadManagerTest, RejectsParentEscapeInModelId) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "../evil:1";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Publisher";

  EXPECT_THROW(manager.GetModelCachePath(info), fl::Exception);
  EXPECT_THROW(manager.IsModelCached(info), fl::Exception);
}

TEST(DownloadManagerTest, RejectsBackslashInPublisher) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "test:1";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Pub\\..\\..\\evil";

  EXPECT_THROW(manager.GetModelCachePath(info), fl::Exception);
}

TEST(DownloadManagerTest, RejectsForwardSlashInPublisher) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "test:1";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Pub/sub";

  EXPECT_THROW(manager.GetModelCachePath(info), fl::Exception);
}

TEST(DownloadManagerTest, RejectsColonInBareModelId) {
  // model_id "drive:c:1" splits as bare="drive:c", version="1"; the bare half then
  // contains a stray ':' that would let a Windows drive letter slip through.
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "drive:c:1";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Publisher";

  EXPECT_THROW(manager.GetModelCachePath(info), fl::Exception);
}

TEST(DownloadManagerTest, RejectsTrailingDotInPublisher) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "test:1";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Publisher.";

  EXPECT_THROW(manager.GetModelCachePath(info), fl::Exception);
}

TEST(DownloadManagerTest, RejectsEmptyModelId) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Publisher";

  EXPECT_THROW(manager.GetModelCachePath(info), fl::Exception);
}

TEST(DownloadManagerTest, AcceptsNormalModelIdAndPublisher) {
  TempDir tmpdir;
  DownloadManager manager(tmpdir.string(), "eastus", 64, fl::test::NullLog());

  ModelInfo info;
  info.model_id = "phi-3-mini:1";
  info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR] = "Microsoft";

  // Should not throw. Path returned is empty until the directory is created on disk.
  EXPECT_NO_THROW(manager.GetModelCachePath(info));
  EXPECT_NO_THROW(manager.IsModelCached(info));
  EXPECT_FALSE(manager.IsModelCached(info));
}
