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

  std::shared_ptr<const ReplayPrefix> replay_prefix;

  // If already exists, preserve its compacted ancestry when the replacement keeps the same parent.
  auto it = index_.find(response_id);
  if (it != index_.end()) {
    const auto old_previous = it->second->response.find("previous_response_id");
    const auto new_previous = response.find("previous_response_id");
    if (old_previous != it->second->response.end() && new_previous != response.end() &&
        *old_previous == *new_previous) {
      replay_prefix = it->second->replay_prefix;
    }

    entries_.erase(it->second);
    index_.erase(it);
  }

  // Insert at front (most recently used)
  entries_.push_front(Entry{response_id, std::move(response), std::move(input_items), std::move(replay_prefix)});
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
    if (id.empty()) {
      break;
    }

    if (index_.find(id) == index_.end()) {
      return chain.back()->replay_prefix ? chain : std::vector<std::list<Entry>::iterator>{};
    }
  }

  return chain;
}

std::optional<ResponseChainContext> ResponseStore::BuildChainContext(const std::string& response_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto chain = WalkChainLocked(response_id);
  if (chain.empty()) {
    return std::nullopt;
  }

  // The walk collected hops newest-first; replay oldest-first so each hop's tool calls precede the results for them.
  ResponseChainContext context;
  std::vector<const ReplayPrefix*> prefix;
  for (auto node = chain.back()->replay_prefix.get(); node != nullptr; node = node->previous.get()) {
    prefix.push_back(node);
  }

  context.reserve(prefix.size() + chain.size());
  for (auto hop = prefix.rbegin(); hop != prefix.rend(); ++hop) {
    context.push_back((*hop)->hop);
  }

  for (auto hop = chain.rbegin(); hop != chain.rend(); ++hop) {
    context.push_back(ToReplayHop(**hop));
  }

  // Touch oldest-first so the requested endpoint finishes at the front as the most-recent entry. Losing any ancestor
  // breaks the chain, but evicting the endpoint first would strand every ancestor without preserving continuation.
  // list::splice keeps the collected iterators valid.
  for (auto hop = chain.rbegin(); hop != chain.rend(); ++hop) {
    TouchLocked(*hop);
  }

  return context;
}

bool ResponseStore::TouchChain(const std::string& response_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto chain = WalkChainLocked(response_id);
  // Touch oldest-first so the requested endpoint finishes as the most-recent entry.
  for (auto hop = chain.rbegin(); hop != chain.rend(); ++hop) {
    TouchLocked(*hop);
  }

  return !chain.empty();
}

bool ResponseStore::Delete(const std::string& response_id) {
  return !DeleteWithDependents(response_id).empty();
}

std::vector<std::string> ResponseStore::DeleteWithDependents(const std::string& response_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<std::list<Entry>::iterator> dependents;
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (DependsOnLocked(*it, response_id)) {
      dependents.push_back(it);
    }
  }

  std::vector<std::string> deleted_ids;
  deleted_ids.reserve(dependents.size() + 1);
  for (auto dependent : dependents) {
    deleted_ids.push_back(dependent->id);
    index_.erase(dependent->id);
    entries_.erase(dependent);
  }

  // A compacted response has no metadata entry of its own, but its ID must still be returned so the handler can evict
  // a session cached under the exact ID the caller deleted.
  if (!deleted_ids.empty() &&
      std::find(deleted_ids.begin(), deleted_ids.end(), response_id) == deleted_ids.end()) {
    deleted_ids.push_back(response_id);
  }

  return deleted_ids;
}

ResponseStore::Page ResponseStore::List(int limit, const std::string& after, const std::string& order) {
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
  Page page;
  auto it = start;
  for (int count = 0; it != ordered.end() && count < limit; ++it, ++count) {
    page.data.push_back((*it)->response);
  }

  // The iterator says exactly whether anything follows the page. Reporting `data.size() == limit` instead claimed a
  // next page whenever the last page happened to be exactly full, and the caller's follow-up request came back empty.
  page.has_more = it != ordered.end();

  return page;
}

size_t ResponseStore::Size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return entries_.size();
}

void ResponseStore::Evict() {
  while (static_cast<int>(entries_.size()) > capacity_) {
    // Evict within the least-recently-used conversation. Walk that entry toward its root so an internal hop is never
    // removed between a retained parent and child. A cycle is malformed; evict the LRU entry itself so malformed
    // conversations cannot become immortal and force every newer response back out of the store.
    auto lru = std::prev(entries_.end());
    auto root = lru;
    std::unordered_set<std::string> visited;
    bool cycle = false;

    while (true) {
      if (!visited.insert(root->id).second) {
        cycle = true;
        break;
      }

      const auto previous = root->response.find("previous_response_id");
      if (previous == root->response.end() || !previous->is_string() || previous->get<std::string>().empty()) {
        break;
      }

      const auto parent = index_.find(previous->get<std::string>());
      if (parent == index_.end()) {
        break;
      }

      root = parent->second;
    }

    if (cycle) {
      root = lru;
    }

    CompactAndEraseLocked(root);
  }
}

void ResponseStore::CompactAndEraseLocked(std::list<Entry>::iterator root) {
  const auto previous = root->response.find("previous_response_id");
  const bool is_complete_root = previous == root->response.end() || !previous->is_string() ||
                                previous->get<std::string>().empty() || root->replay_prefix != nullptr;

  if (is_complete_root) {
    const auto prefix = std::make_shared<ReplayPrefix>(
        ReplayPrefix{root->id, root->replay_prefix, ToReplayHop(*root)});

    // Branches share the immutable prefix rather than copying every earlier tool result into every child.
    for (auto& entry : entries_) {
      const auto entry_previous = entry.response.find("previous_response_id");
      if (entry_previous != entry.response.end() && entry_previous->is_string() &&
          entry_previous->get<std::string>() == root->id) {
        entry.replay_prefix = prefix;
      }
    }
  }

  index_.erase(root->id);
  entries_.erase(root);
}

bool ResponseStore::PrefixContains(const std::shared_ptr<const ReplayPrefix>& prefix,
                                   const std::string& response_id) {
  for (auto node = prefix.get(); node != nullptr; node = node->previous.get()) {
    if (node->id == response_id) {
      return true;
    }
  }

  return false;
}

bool ResponseStore::DependsOnLocked(const Entry& entry, const std::string& response_id) const {
  const Entry* current = &entry;
  std::unordered_set<std::string> visited;

  while (current != nullptr && visited.insert(current->id).second) {
    if (current->id == response_id || PrefixContains(current->replay_prefix, response_id)) {
      return true;
    }

    const auto previous = current->response.find("previous_response_id");
    if (previous == current->response.end() || !previous->is_string() || previous->get<std::string>().empty()) {
      break;
    }

    const auto parent = index_.find(previous->get<std::string>());
    current = parent == index_.end() ? nullptr : &*parent->second;
  }

  return false;
}

ResponseChainHop ResponseStore::ToReplayHop(const Entry& entry) {
  ResponseChainHop replay;
  if (entry.input_items.is_array()) {
    replay.input_items = entry.input_items;
  }

  const auto output = entry.response.find("output");
  if (output != entry.response.end() && output->is_array()) {
    replay.output_items = *output;
  }

  return replay;
}

void ResponseStore::TouchLocked(std::list<Entry>::iterator it) {
  if (it != entries_.begin()) {
    entries_.splice(entries_.begin(), entries_, it);
  }
}

}  // namespace fl
