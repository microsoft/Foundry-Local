// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "mock_core.h"
#include "foundry_local.h"
#include "foundry_local_exception.h"

#include <nlohmann/json.hpp>

using namespace foundry_local;
using namespace foundry_local::Testing;

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

    core_.OnCall("get_catalog_name", "test-catalog");
    core_.OnCall("initialize", "");
    core_.OnCall("discover_eps", eps.dump());

    // We can't easily create a Manager in unit tests without full init,
    // so test the JSON parsing logic directly via the core mock.
    auto response = core_.call("discover_eps", logger_);
    ASSERT_FALSE(response.HasError());

    auto json = nlohmann::json::parse(response.data);
    ASSERT_TRUE(json.is_array());
    ASSERT_EQ(2u, json.size());

    EXPECT_EQ("WebGpuExecutionProvider", json[0].value("Name", ""));
    EXPECT_FALSE(json[0].value("IsRegistered", true));

    EXPECT_EQ("CUDAExecutionProvider", json[1].value("Name", ""));
    EXPECT_TRUE(json[1].value("IsRegistered", false));
}

TEST_F(DiscoverEpsTest, EmptyResponseReturnsEmptyArray) {
    core_.OnCall("discover_eps", "");

    auto response = core_.call("discover_eps", logger_);
    ASSERT_FALSE(response.HasError());
    EXPECT_TRUE(response.data.empty());
}

TEST_F(DiscoverEpsTest, CoreErrorPropagates) {
    core_.OnCallThrow("discover_eps", "Core error: unknown command");

    auto response = core_.call("discover_eps", logger_);
    ASSERT_TRUE(response.HasError());
    EXPECT_EQ("Core error: unknown command", response.error);
}

TEST_F(DiscoverEpsTest, MalformedJsonIsDetectable) {
    core_.OnCall("discover_eps", "not valid json{");

    auto response = core_.call("discover_eps", logger_);
    auto json = nlohmann::json::parse(response.data, nullptr, false);
    EXPECT_TRUE(json.is_discarded());
}

TEST_F(DiscoverEpsTest, NonArrayJsonIsDetectable) {
    core_.OnCall("discover_eps", R"({"not": "an array"})");

    auto response = core_.call("discover_eps", logger_);
    auto json = nlohmann::json::parse(response.data, nullptr, false);
    ASSERT_FALSE(json.is_discarded());
    EXPECT_FALSE(json.is_array());
}

// ===========================================================================
// DownloadAndRegisterEps — request payload
// ===========================================================================

class DownloadAndRegisterEpsTest : public ::testing::Test {
protected:
    MockCore core_;
    NullLogger logger_;
};

TEST_F(DownloadAndRegisterEpsTest, NoNames_PassesNullData) {
    core_.OnCall("download_and_register_eps",
        [](std::string_view, const std::string* dataArg, NativeCallbackFn, void*) -> std::string {
            // When no names are specified, dataArgument should be null
            EXPECT_EQ(nullptr, dataArg);
            return R"({"Success": true, "Status": "Completed", "RegisteredEps": [], "FailedEps": []})";
        });

    auto response = core_.call("download_and_register_eps", logger_);
    ASSERT_FALSE(response.HasError());
}

TEST_F(DownloadAndRegisterEpsTest, WithNames_PassesNamesInParams) {
    std::string capturedData;
    core_.OnCall("download_and_register_eps",
        [&](std::string_view, const std::string* dataArg, NativeCallbackFn, void*) -> std::string {
            EXPECT_NE(nullptr, dataArg);
            if (dataArg) capturedData = *dataArg;
            return R"({"Success": true, "Status": "OK", "RegisteredEps": ["WebGpuExecutionProvider"], "FailedEps": []})";
        });

    // Simulate what Manager::DownloadAndRegisterEps does with names
    nlohmann::json wrapper;
    wrapper["Params"]["Names"] = "WebGpuExecutionProvider,CUDAExecutionProvider";
    std::string requestData = wrapper.dump();

    auto response = core_.call("download_and_register_eps", logger_, &requestData);
    ASSERT_FALSE(response.HasError());

    // Verify the request payload structure
    auto json = nlohmann::json::parse(requestData);
    ASSERT_TRUE(json.contains("Params"));
    EXPECT_EQ("WebGpuExecutionProvider,CUDAExecutionProvider", json["Params"]["Names"].get<std::string>());
}

TEST_F(DownloadAndRegisterEpsTest, ResultJsonParsesCorrectly) {
    std::string resultJson = R"({
        "Success": true,
        "Status": "Completed",
        "RegisteredEps": ["WebGpuExecutionProvider", "CUDAExecutionProvider"],
        "FailedEps": ["BadEP"]
    })";

    auto json = nlohmann::json::parse(resultJson);
    EXPECT_TRUE(json.value("Success", false));
    EXPECT_EQ("Completed", json.value("Status", ""));

    auto registered = json["RegisteredEps"].get<std::vector<std::string>>();
    ASSERT_EQ(2u, registered.size());
    EXPECT_EQ("WebGpuExecutionProvider", registered[0]);
    EXPECT_EQ("CUDAExecutionProvider", registered[1]);

    auto failed = json["FailedEps"].get<std::vector<std::string>>();
    ASSERT_EQ(1u, failed.size());
    EXPECT_EQ("BadEP", failed[0]);
}

TEST_F(DownloadAndRegisterEpsTest, MalformedResultJsonIsDetectable) {
    std::string badJson = "not json at all";
    auto json = nlohmann::json::parse(badJson, nullptr, false);
    EXPECT_TRUE(json.is_discarded());
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
    std::string name = progressStr.substr(0, sepIndex);
    EXPECT_EQ("EP", name);

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
    auto sepIndex = progressStr.find('|');
    EXPECT_EQ(std::string::npos, sepIndex);
}

TEST(EpProgressParsingTest, EmptyPercent_FailsParsing) {
    std::string progressStr = "EP|";
    auto sepIndex = progressStr.find('|');
    ASSERT_NE(std::string::npos, sepIndex);

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
