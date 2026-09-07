// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "inferencing/generative/openresponses/response_store.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

namespace fl {

ResponseStore::ResponseStore(int capacity)
    : capacity_(std::clamp(capacity, 1, kMaxCapacity)) {
}

void ResponseStore::Store(const std::string& response_id,
                          nlohmann::json response,
                          nlohmann::json input_items) {
  std::lock_guard<std::mutex> lock(mutex_);

  // If already exists, remove old entry
  auto it = index_.find(response_id);
  if (it != index_.end()) {
    entries_.erase(it->second);
    index_.erase(it);
  }

  // Insert at front (most recently used)
  entries_.push_front(Entry{response_id, std::move(response), std::move(input_items)});
  index_[response_id] = entries_.begin();

  Evict();
}

std::optional<nlohmann::json> ResponseStore::Get(const std::string& response_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = index_.find(response_id);
  if (it == index_.end()) {
    return std::nullopt;
  }

  TouchLocked(it->second);
  return it->second->response;
}

std::optional<nlohmann::json> ResponseStore::GetInputItems(const std::string& response_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = index_.find(response_id);
  if (it == index_.end()) {
    return std::nullopt;
  }

  TouchLocked(it->second);
  return it->second->input_items;
}

std::vector<std::list<ResponseStore::Entry>::iterator> ResponseStore::WalkChainLocked(
    const std::string& response_id) {
  // A visited set bounds the walk: cyclic links would otherwise loop forever, and a cycle means the stored chain is
  // not a conversation that can be replayed.
  std::vector<std::list<Entry>::iterator> chain;
  std::unordered_set<std::string> visited;
  std::string id = response_id;

  while (!id.empty()) {
    if (!visited.insert(id).second) {
      return {};
    }

    auto it = index_.find(id);
    if (it == index_.end()) {
      return {};
    }

    chain.push_back(it->second);

    const auto previous = it->second->response.find("previous_response_id");
    if (previous == it->second->response.end() || !previous->is_string()) {
      break;
    }

    id = previous->get<std::string>();
  }

  return chain;
}

namespace {

/// True when `item` is the system message ToInputItems synthesized from a request's `instructions`.
/// Instructions are request-scoped in the Responses API, so replaying them from every hop of a chain would stack up
/// copies of the system prompt in the middle of the conversation.
///
/// Every field is type-checked before it is read: stored items are arbitrary caller input, and a system message whose
/// `content` is an array of content parts must be replayed, not throw.
bool IsInstructionsItem(const nlohmann::json& item, const nlohmann::json& response) {
  const auto instructions = response.find("instructions");
  if (instructions == response.end() || !instructions->is_string()) {
    return false;
  }

  if (!item.is_object()) {
    return false;
  }

  const auto role = item.find("role");
  if (role == item.end() || !role->is_string() || role->get<std::string>() != "system") {
    return false;
  }

  // ToInputItems always writes the instructions as a plain string; any other shape is caller-supplied content.
  const auto content = item.find("content");
  if (content == item.end() || !content->is_string()) {
    return false;
  }

  return content->get<std::string>() == instructions->get<std::string>();
}

}  // namespace

std::optional<nlohmann::json> ResponseStore::BuildChainContext(const std::string& response_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto chain = WalkChainLocked(response_id);
  if (chain.empty()) {
    return std::nullopt;
  }

  // The walk collected hops newest-first; replay oldest-first so each hop's tool calls precede the results for them.
  auto context = nlohmann::json::array();
  for (auto hop = chain.rbegin(); hop != chain.rend(); ++hop) {
    const auto& entry = **hop;

    if (entry.input_items.is_array()) {
      for (const auto& item : entry.input_items) {
        if (!IsInstructionsItem(item, entry.response)) {
          context.push_back(item);
        }
      }
    }

    const auto output = entry.response.find("output");
    if (output != entry.response.end() && output->is_array()) {
      context.insert(context.end(), output->begin(), output->end());
    }
  }

  // Requesting the chain is a use of every entry in it. list::splice keeps the collected iterators valid.
  for (auto& hop : chain) {
    TouchLocked(hop);
  }

  return context;
}

bool ResponseStore::TouchChain(const std::string& response_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto chain = WalkChainLocked(response_id);
  for (auto& hop : chain) {
    TouchLocked(hop);
  }

  return !chain.empty();
}

bool ResponseStore::Delete(const std::string& response_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = index_.find(response_id);
  if (it == index_.end()) {
    return false;
  }

  entries_.erase(it->second);
  index_.erase(it);
  return true;
}

std::vector<nlohmann::json> ResponseStore::List(int limit, const std::string& after, const std::string& order) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Collect all entries in insertion order (front = newest)
  std::vector<const Entry*> ordered;
  ordered.reserve(entries_.size());
  for (const auto& entry : entries_) {
    ordered.push_back(&entry);
  }

  // For ascending order, reverse so oldest is first
  if (order == "asc") {
    std::reverse(ordered.begin(), ordered.end());
  }

  // Apply cursor — skip entries until we find the "after" ID
  auto start = ordered.begin();
  if (!after.empty()) {
    for (auto it = ordered.begin(); it != ordered.end(); ++it) {
      if ((*it)->id == after) {
        start = it + 1;
        break;
      }
    }
  }

  // Collect results up to limit
  std::vector<nlohmann::json> results;
  int count = 0;
  for (auto it = start; it != ordered.end() && count < limit; ++it, ++count) {
    results.push_back((*it)->response);
  }

  return results;
}

size_t ResponseStore::Size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return entries_.size();
}

void ResponseStore::Evict() {
  while (static_cast<int>(entries_.size()) > capacity_) {
    auto& back = entries_.back();
    index_.erase(back.id);
    entries_.pop_back();
  }
}

void ResponseStore::TouchLocked(std::list<Entry>::iterator it) {
  if (it != entries_.begin()) {
    entries_.splice(entries_.begin(), entries_, it);
  }
}

}  // namespace fl
