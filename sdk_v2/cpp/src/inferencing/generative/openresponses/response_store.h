// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <nlohmann/json.hpp>

#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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

  /// Reconstruct the complete replay context for a chained response: for every hop from the root of the
  /// `previous_response_id` chain to `response_id`, that hop's own input items followed by its output items, oldest
  /// first.
  ///
  /// Each entry stores only its own request's input items — that is exactly what the /input_items endpoint must
  /// return — so a caller that replayed a single hop would lose everything before it, including the assistant tool
  /// calls that later tool results have to correlate against.
  ///
  /// Instructions are request-scoped in the Responses API and are not carried across a chain, so each hop's
  /// instructions-derived system item is left out of the replay while remaining in that hop's stored input items.
  ///
  /// Returns nullopt when the chain cannot be reconstructed: a link is missing (evicted or never stored) or the
  /// stored links form a cycle. Callers must fail explicitly rather than run inference on a truncated conversation.
  std::optional<nlohmann::json> BuildChainContext(const std::string& response_id);

  /// Mark every hop of a chain as recently used without materializing its context. Callers that continue a
  /// conversation from a live session skip reconstruction entirely; without this the conversation's own early hops
  /// would age out of the store while it is still active, and a later cache miss could no longer rebuild it.
  /// Returns false when the chain is already broken.
  bool TouchChain(const std::string& response_id);

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

  /// Walk `response_id` back to the root of its `previous_response_id` chain. Returns the hops newest-first, or an
  /// empty vector when a link is missing or the links form a cycle.
  std::vector<std::list<Entry>::iterator> WalkChainLocked(const std::string& response_id);
};

}  // namespace fl
