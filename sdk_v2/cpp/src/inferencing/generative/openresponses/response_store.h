// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <nlohmann/json.hpp>

#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace fl {

/// In-memory LRU store for Responses API responses and their input items.
/// Thread-safe. Usable by both the web service handler and the direct
/// ResponsesClient API.
///
/// Stores responses as JSON objects to avoid tight coupling with the
/// Responses API type hierarchy — the handler builds JSON, the store
/// keeps it.
class ResponseStore {
 public:
  static constexpr int kDefaultCapacity = 20;
  static constexpr int kMaxCapacity = 100;

  explicit ResponseStore(int capacity = kDefaultCapacity);

  /// Store a completed response and its input items atomically.
  /// Complements SessionManager's cached ChatSession instances: SessionManager caches a small number of live sessions
  /// (with their generators / KV cache) for fast continuation, while ResponseStore keeps a larger, lightweight history
  /// of completed responses and their input items for lookup and pagination beyond the session cache capacity.
  void Store(const std::string& response_id,
             nlohmann::json response,
             nlohmann::json input_items);

  /// Retrieve a stored response by ID. Returns nullopt if not found.
  std::optional<nlohmann::json> Get(const std::string& response_id);

  /// Retrieve stored input items for a response. Returns nullopt if not found.
  std::optional<nlohmann::json> GetInputItems(const std::string& response_id);

  /// Delete a stored response. Returns true if it existed.
  bool Delete(const std::string& response_id);

  /// List stored responses with cursor-based pagination.
  /// @param limit  Maximum number to return.
  /// @param after  Cursor — return responses after this ID. Empty = from start.
  /// @param order  "asc" or "desc" (default: "desc" = newest first).
  /// @return  Vector of response JSON objects.
  std::vector<nlohmann::json> List(int limit = 20,
                                   const std::string& after = "",
                                   const std::string& order = "desc");

  /// Number of responses currently stored.
  size_t Size() const;

 private:
  struct Entry {
    std::string id;
    nlohmann::json response;
    nlohmann::json input_items;
  };

  int capacity_;
  mutable std::mutex mutex_;
  std::list<Entry> entries_;  // front = most recently used
  std::unordered_map<std::string, std::list<Entry>::iterator> index_;

  void Evict();
  void TouchLocked(std::list<Entry>::iterator it);
};

}  // namespace fl
