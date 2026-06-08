// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <gtest/gtest.h>

#include <charconv>
#include <string>
#include <vector>

#include "mock_core.h"
#include "foundry_local.h"
#include "foundry_local_exception.h"
#include "core_interop_request.h"

#include <nlohmann/json.hpp>

using namespace foundry_local;
using namespace foundry_local::Testing;

// ===========================================================================
// Helper: replicates Manager::DiscoverEps logic for unit testing without
// needing a Manager instance (which requires the real Core DLL).
// ===========================================================================
static std::vector<EpInfo> TestDiscoverEps(Internal::IFoundryLocalCore& core, ILogger& logger) {
    auto response = core.call("discover_eps", logger);
    if (response.HasError()) {
        throw Exception(std::string("Error discovering execution providers: ") + response.error, logger);
    }

    std::vector<EpInfo> result;
    if (response.data.empty()) return result;

    auto json = nlohmann::json::parse(response.data, nullptr, false);
    if (json.is_discarded()) {
        throw Exception(std::string("Failed to parse discover_eps response: ") + response.data, logger);
    }
    if (!json.is_array()) {
        throw Exception(std::string("Expected JSON array from discover_eps, got: ") + response.data, logger);
    }

    for (const auto& item : json) {
        EpInfo ep;
        ep.name = item.value("Name", "");
        ep.is_registered = item.value("IsRegistered", false);
        result.push_back(std::move(ep));
    }
    return result;
}

// ===========================================================================
// Helper: replicates Manager::DownloadAndRegisterEps logic for unit testing.
// ===========================================================================
static EpDownloadResult TestDownloadAndRegisterEps(
    Internal::IFoundryLocalCore& core, ILogger& logger,
    const std::vector<std::string>& names = {},
    EpProgressCallback progressCallback = nullptr) {

    std::string requestData;
    std::string* requestDataPtr = nullptr;

    if (!names.empty()) {
        CoreInteropRequest request("download_and_register_eps");
        std::string namesList;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) namesList += ",";
            namesList += names[i];
        }
        request.AddParam("Names", namesList);
        requestData = request.ToJson();
        requestDataPtr = &requestData;
    }

    struct EpCallbackContext { EpProgressCallback* callback; };

    auto nativeCb = [](const void* data, int32_t dataLength, void* userData) -> int32_t {
        auto* ctx = static_cast<EpCallbackContext*>(userData);
        if (!ctx || !ctx->callback || !*ctx->callback) return 0;
        if (!data || dataLength <= 0) return 0;

        std::string progressStr(static_cast<const char*>(data), static_cast<size_t>(dataLength));
        auto sepIndex = progressStr.find('|');
        if (sepIndex != std::string::npos) {
            std::string name = progressStr.substr(0, sepIndex);
            const auto* begin = progressStr.data() + sepIndex + 1;
            const auto* end = progressStr.data() + progressStr.size();
            double percent = 0.0;
            auto [ptr, ec] = std::from_chars(begin, end, percent);
            if (ec == std::errc{}) {
                (*ctx->callback)(name, percent);
            }
        }
        return 0;
    };

    CoreResponse response;
    if (progressCallback) {
        EpCallbackContext ctx{&progressCallback};
        response = core.call("download_and_register_eps", logger, requestDataPtr, nativeCb, &ctx);
    } else {
        response = core.call("download_and_register_eps", logger, requestDataPtr);
    }

    if (response.HasError()) {
        throw Exception(std::string("Error downloading execution providers: ") + response.error, logger);
    }

    EpDownloadResult result;
    if (!response.data.empty()) {
        auto json = nlohmann::json::parse(response.data, nullptr, false);
        if (json.is_discarded()) {
            throw Exception(std::string("Failed to parse response: ") + response.data, logger);
        }
        result.success = json.value("Success", false);
        result.status = json.value("Status", "");
        if (json.contains("RegisteredEps") && json["RegisteredEps"].is_array()) {
            for (const auto& ep : json["RegisteredEps"]) {
                result.registered_eps.push_back(ep.get<std::string>());
            }
        }
        if (json.contains("FailedEps") && json["FailedEps"].is_array()) {
            for (const auto& ep : json["FailedEps"]) {
                result.failed_eps.push_back(ep.get<std::string>());
            }
        }
    } else {
        result.success = true;
        result.status = "Completed";
    }
    return result;
}

// ===========================================================================
// DiscoverEps
// ===========================================================================

class DiscoverEpsTest : public ::testing::Test {
protected:
    MockCore core_;
    NullLogger logger_;
};

TEST_F(DiscoverEpsTest, ParsesValidJsonArray) {
    nlohmann::json eps = nlohmann::json::array({
        {{"Name", "WebGpuExecutionProvider"}, {"IsRegistered", false}},
        {{"Name", "CUDAExecutionProvider"}, {"IsRegistered", true}},
    });
    core_.OnCall("discover_eps", eps.dump());

    auto result = TestDiscoverEps(core_, logger_);
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ("WebGpuExecutionProvider", result[0].name);
    EXPECT_FALSE(result[0].is_registered);
    EXPECT_EQ("CUDAExecutionProvider", result[1].name);
    EXPECT_TRUE(result[1].is_registered);
}

TEST_F(DiscoverEpsTest, EmptyResponseReturnsEmptyVector) {
    core_.OnCall("discover_eps", "");
    auto result = TestDiscoverEps(core_, logger_);
    EXPECT_TRUE(result.empty());
}

TEST_F(DiscoverEpsTest, CoreErrorThrows) {
    core_.OnCallThrow("discover_eps", "Core error: unknown command");
    EXPECT_THROW(TestDiscoverEps(core_, logger_), Exception);
}

TEST_F(DiscoverEpsTest, MalformedJsonThrows) {
    core_.OnCall("discover_eps", "not valid json{");
    EXPECT_THROW(TestDiscoverEps(core_, logger_), Exception);
}

TEST_F(DiscoverEpsTest, NonArrayJsonThrows) {
    core_.OnCall("discover_eps", R"({"not": "an array"})");
    EXPECT_THROW(TestDiscoverEps(core_, logger_), Exception);
}

// ===========================================================================
// DownloadAndRegisterEps
// ===========================================================================

class DownloadAndRegisterEpsTest : public ::testing::Test {
protected:
    MockCore core_;
    NullLogger logger_;
};

TEST_F(DownloadAndRegisterEpsTest, NoNames_PassesNullData) {
    core_.OnCall("download_and_register_eps",
        [](std::string_view, const std::string* dataArg, NativeCallbackFn, void*) -> std::string {
            EXPECT_EQ(nullptr, dataArg);
            return R"({"Success": true, "Status": "Completed", "RegisteredEps": [], "FailedEps": []})";
        });

    auto result = TestDownloadAndRegisterEps(core_, logger_);
    EXPECT_TRUE(result.success);
    EXPECT_EQ("Completed", result.status);
}

TEST_F(DownloadAndRegisterEpsTest, WithNames_PassesNamesInParams) {
    std::string capturedData;
    core_.OnCall("download_and_register_eps",
        [&](std::string_view, const std::string* dataArg, NativeCallbackFn, void*) -> std::string {
            EXPECT_NE(nullptr, dataArg);
            if (dataArg) capturedData = *dataArg;
            return R"({"Success": true, "Status": "OK", "RegisteredEps": ["WebGpuExecutionProvider"], "FailedEps": []})";
        });

    auto result = TestDownloadAndRegisterEps(core_, logger_, {"WebGpuExecutionProvider", "CUDAExecutionProvider"});
    EXPECT_TRUE(result.success);
    ASSERT_EQ(1u, result.registered_eps.size());
    EXPECT_EQ("WebGpuExecutionProvider", result.registered_eps[0]);

    auto json = nlohmann::json::parse(capturedData);
    ASSERT_TRUE(json.contains("Params"));
    EXPECT_EQ("WebGpuExecutionProvider,CUDAExecutionProvider", json["Params"]["Names"].get<std::string>());
}

TEST_F(DownloadAndRegisterEpsTest, ResultJsonParsesCorrectly) {
    core_.OnCall("download_and_register_eps", R"({
        "Success": true,
        "Status": "Completed",
        "RegisteredEps": ["WebGpuExecutionProvider", "CUDAExecutionProvider"],
        "FailedEps": ["BadEP"]
    })");

    auto result = TestDownloadAndRegisterEps(core_, logger_);
    EXPECT_TRUE(result.success);
    EXPECT_EQ("Completed", result.status);
    ASSERT_EQ(2u, result.registered_eps.size());
    EXPECT_EQ("WebGpuExecutionProvider", result.registered_eps[0]);
    EXPECT_EQ("CUDAExecutionProvider", result.registered_eps[1]);
    ASSERT_EQ(1u, result.failed_eps.size());
    EXPECT_EQ("BadEP", result.failed_eps[0]);
}

TEST_F(DownloadAndRegisterEpsTest, MalformedResultJsonThrows) {
    core_.OnCall("download_and_register_eps", "not json at all");
    EXPECT_THROW(TestDownloadAndRegisterEps(core_, logger_), Exception);
}

TEST_F(DownloadAndRegisterEpsTest, CoreErrorThrows) {
    core_.OnCallThrow("download_and_register_eps", "download failed");
    EXPECT_THROW(TestDownloadAndRegisterEps(core_, logger_), Exception);
}

TEST_F(DownloadAndRegisterEpsTest, CallbackInvokedWithProgressData) {
    core_.OnCall("download_and_register_eps",
        [](std::string_view, const std::string*, NativeCallbackFn callback, void* userData) -> std::string {
            if (callback) {
                std::string p1 = "WebGpuExecutionProvider|25.0";
                callback(p1.data(), static_cast<int32_t>(p1.size()), userData);
                std::string p2 = "WebGpuExecutionProvider|100.0";
                callback(p2.data(), static_cast<int32_t>(p2.size()), userData);
            }
            return R"({"Success": true, "Status": "OK", "RegisteredEps": ["WebGpuExecutionProvider"], "FailedEps": []})";
        });

    std::vector<std::pair<std::string, double>> progress;
    auto result = TestDownloadAndRegisterEps(core_, logger_, {},
        [&](const std::string& name, double pct) { progress.push_back({name, pct}); });

    EXPECT_TRUE(result.success);
    ASSERT_EQ(2u, progress.size());
    EXPECT_EQ("WebGpuExecutionProvider", progress[0].first);
    EXPECT_DOUBLE_EQ(25.0, progress[0].second);
    EXPECT_EQ("WebGpuExecutionProvider", progress[1].first);
    EXPECT_DOUBLE_EQ(100.0, progress[1].second);
}

TEST_F(DownloadAndRegisterEpsTest, EmptyResponse_DefaultsToSuccess) {
    core_.OnCall("download_and_register_eps", "");
    auto result = TestDownloadAndRegisterEps(core_, logger_);
    EXPECT_TRUE(result.success);
    EXPECT_EQ("Completed", result.status);
}

// ===========================================================================
// Progress callback parsing
// ===========================================================================

TEST(EpProgressParsingTest, ValidProgressString) {
    std::string progressStr = "WebGpuExecutionProvider|75.5";
    auto sepIndex = progressStr.find('|');
    ASSERT_NE(std::string::npos, sepIndex);
    std::string name = progressStr.substr(0, sepIndex);
    EXPECT_EQ("WebGpuExecutionProvider", name);
    const auto* begin = progressStr.data() + sepIndex + 1;
    const auto* end = progressStr.data() + progressStr.size();
    double percent = 0.0;
    auto [ptr, ec] = std::from_chars(begin, end, percent);
    EXPECT_EQ(std::errc{}, ec);
    EXPECT_DOUBLE_EQ(75.5, percent);
}

TEST(EpProgressParsingTest, ZeroPercent) {
    std::string progressStr = "EP|0.0";
    auto sepIndex = progressStr.find('|');
    const auto* begin = progressStr.data() + sepIndex + 1;
    const auto* end = progressStr.data() + progressStr.size();
    double percent = -1.0;
    auto [ptr, ec] = std::from_chars(begin, end, percent);
    EXPECT_EQ(std::errc{}, ec);
    EXPECT_DOUBLE_EQ(0.0, percent);
}

TEST(EpProgressParsingTest, HundredPercent) {
    std::string progressStr = "EP|100.0";
    auto sepIndex = progressStr.find('|');
    const auto* begin = progressStr.data() + sepIndex + 1;
    const auto* end = progressStr.data() + progressStr.size();
    double percent = 0.0;
    auto [ptr, ec] = std::from_chars(begin, end, percent);
    EXPECT_EQ(std::errc{}, ec);
    EXPECT_DOUBLE_EQ(100.0, percent);
}

TEST(EpProgressParsingTest, NoSeparator_Skipped) {
    std::string progressStr = "NoPipeHere";
    EXPECT_EQ(std::string::npos, progressStr.find('|'));
}

TEST(EpProgressParsingTest, EmptyPercent_FailsParsing) {
    std::string progressStr = "EP|";
    auto sepIndex = progressStr.find('|');
    const auto* begin = progressStr.data() + sepIndex + 1;
    const auto* end = progressStr.data() + progressStr.size();
    double percent = 0.0;
    auto [ptr, ec] = std::from_chars(begin, end, percent);
    EXPECT_NE(std::errc{}, ec);
}

TEST(EpProgressParsingTest, NonNumericPercent_FailsParsing) {
    std::string progressStr = "EP|abc";
    auto sepIndex = progressStr.find('|');
    const auto* begin = progressStr.data() + sepIndex + 1;
    const auto* end = progressStr.data() + progressStr.size();
    double percent = 0.0;
    auto [ptr, ec] = std::from_chars(begin, end, percent);
    EXPECT_NE(std::errc{}, ec);
}
