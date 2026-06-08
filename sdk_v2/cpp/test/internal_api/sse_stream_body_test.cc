// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Tests for SseStreamBody in handler_utils.h — Push, Finish, read, declareHeaders.
//

#ifdef FOUNDRY_LOCAL_HAS_WEB_SERVICE

#include "service/handler_utils.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <thread>
#include <unordered_set>

using namespace fl;

// ========================================================================
// SseStreamBody::Push + read basics
// ========================================================================

TEST(SseStreamBodyTest, PushAndReadSingleChunk) {
  SseStreamBody body;
  body.Push("data: {\"text\":\"hello\"}\n\n");
  body.Finish();

  char buffer[256] = {};
  oatpp::async::Action action;
  auto bytes_read = body.read(buffer, sizeof(buffer), action);

  ASSERT_GT(bytes_read, 0);
  std::string result(buffer, bytes_read);
  EXPECT_EQ(result, "data: {\"text\":\"hello\"}\n\n");
}

TEST(SseStreamBodyTest, PushMultipleChunks_ReadDrainsAll) {
  SseStreamBody body;
  body.Push("data: chunk1\n\n");
  body.Push("data: chunk2\n\n");
  body.Finish();

  // Use a large buffer to drain everything in one read
  char buffer[1024] = {};
  oatpp::async::Action action;
  auto bytes_read = body.read(buffer, sizeof(buffer), action);

  std::string result(buffer, bytes_read);
  EXPECT_EQ(result, "data: chunk1\n\ndata: chunk2\n\n");
}

TEST(SseStreamBodyTest, ReadReturnsZeroWhenFinishedAndEmpty) {
  SseStreamBody body;
  body.Finish();

  char buffer[64] = {};
  oatpp::async::Action action;
  auto bytes_read = body.read(buffer, sizeof(buffer), action);

  EXPECT_EQ(bytes_read, 0);
}

TEST(SseStreamBodyTest, PartialRead_SmallBuffer) {
  SseStreamBody body;
  std::string chunk = "data: hello world\n\n";
  body.Push(chunk);
  body.Finish();

  // Read with a buffer smaller than the chunk
  char buffer[10] = {};
  oatpp::async::Action action;
  auto bytes1 = body.read(buffer, sizeof(buffer), action);
  ASSERT_EQ(bytes1, 10);
  std::string part1(buffer, bytes1);
  EXPECT_EQ(part1, "data: hell");

  // Read the rest
  char buffer2[64] = {};
  auto bytes2 = body.read(buffer2, sizeof(buffer2), action);
  std::string part2(buffer2, bytes2);
  EXPECT_EQ(part2, "o world\n\n");
}

// ========================================================================
// SseStreamBody::Finish
// ========================================================================

TEST(SseStreamBodyTest, FinishAfterPush_DataStillReadable) {
  SseStreamBody body;
  body.Push("data: important\n\n");
  body.Finish();

  char buffer[128] = {};
  oatpp::async::Action action;
  auto bytes_read = body.read(buffer, sizeof(buffer), action);
  std::string result(buffer, bytes_read);
  EXPECT_EQ(result, "data: important\n\n");

  // Subsequent read returns EOF
  auto bytes_eof = body.read(buffer, sizeof(buffer), action);
  EXPECT_EQ(bytes_eof, 0);
}

// ========================================================================
// SseStreamBody::declareHeaders
// ========================================================================

TEST(SseStreamBodyTest, DeclareHeaders_SetsCorrectValues) {
  SseStreamBody body;

  // oatpp Headers is a multimap-like container
  oatpp::web::protocol::http::Headers headers;
  body.declareHeaders(headers);

  auto content_type = headers.get("Content-Type");
  ASSERT_TRUE(content_type);
  EXPECT_EQ(std::string(content_type->c_str()), std::string("text/event-stream"));

  auto cache_control = headers.get("Cache-Control");
  ASSERT_TRUE(cache_control);
  EXPECT_EQ(std::string(cache_control->c_str()), std::string("no-cache"));

  auto connection = headers.get("Connection");
  ASSERT_TRUE(connection);
  EXPECT_EQ(std::string(connection->c_str()), std::string("keep-alive"));
}

// ========================================================================
// SseStreamBody metadata
// ========================================================================

TEST(SseStreamBodyTest, GetKnownSize_ReturnsNegativeOne) {
  SseStreamBody body;
  EXPECT_EQ(body.getKnownSize(), -1);
}

TEST(SseStreamBodyTest, GetKnownData_ReturnsNullptr) {
  SseStreamBody body;
  EXPECT_EQ(body.getKnownData(), nullptr);
}

// ========================================================================
// SseStreamBody — threaded Push + read
// ========================================================================

TEST(SseStreamBodyTest, ConcurrentPushAndRead) {
  SseStreamBody body;
  constexpr int kChunkCount = 50;
  std::string total_read;

  // Producer thread
  std::thread producer([&body] {
    for (int i = 0; i < kChunkCount; ++i) {
      body.Push("data: " + std::to_string(i) + "\n\n");
    }

    body.Finish();
  });

  // Consumer: read until EOF
  char buffer[256];
  oatpp::async::Action action;
  while (true) {
    auto n = body.read(buffer, sizeof(buffer), action);
    if (n == 0) {
      break;
    }

    total_read.append(buffer, n);
  }

  producer.join();

  // Verify all chunks were received
  for (int i = 0; i < kChunkCount; ++i) {
    std::string expected = "data: " + std::to_string(i) + "\n\n";
    EXPECT_NE(total_read.find(expected), std::string::npos)
        << "Missing chunk " << i;
  }
}

// ========================================================================
// GenerateCompletionId — fixed-width IDs
// ========================================================================

TEST(GenerateCompletionIdTest, ProducesFixedWidthIdsAcrossManyCalls) {
  // Two 32-bit values rendered as zero-padded hex = 16 hex chars.
  const std::string prefix = "chatcmpl";
  const size_t expected_len = prefix.size() + 1 /* '-' */ + 16;

  std::unordered_set<std::string> ids;
  for (int i = 0; i < 1000; ++i) {
    std::string id = GenerateCompletionId(prefix);
    EXPECT_EQ(id.size(), expected_len) << "id=" << id;
    EXPECT_EQ(id.substr(0, prefix.size() + 1), prefix + "-");
    ids.insert(id);
  }

  // Sanity: IDs should be (overwhelmingly) unique.
  EXPECT_GT(ids.size(), 990u);
}

TEST(GenerateCompletionIdTest, ZeroPadsLowEntropyValues) {
  // Generate many IDs and confirm none of them have a hex tail shorter than 16
  // chars (the original bug: small random values produced shorter IDs).
  const std::string prefix = "x";
  for (int i = 0; i < 5000; ++i) {
    std::string id = GenerateCompletionId(prefix);
    auto dash = id.find('-');
    ASSERT_NE(dash, std::string::npos);
    EXPECT_EQ(id.size() - dash - 1, 16u) << "id=" << id;
  }
}

#endif  // FOUNDRY_LOCAL_HAS_WEB_SERVICE
