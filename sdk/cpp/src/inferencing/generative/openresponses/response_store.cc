// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "inferencing/generative/openresponses/response_store.h"

#include <algorithm>

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
