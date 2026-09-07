// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "inferencing/generative/openresponses/response_store.h"

#include "contracts/responses.h"

#include <algorithm>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fl {

namespace {

/// The client-visible view of a stored response.
///
/// A stored response carries one thing a client never sees: for each `custom_tool_call` the model wrote as a bare
/// envelope, the dialect it used (see ResponseConverter::ToStoredJson). That is this runtime's own bookkeeping,
/// kept so a conversation continued after its warm session was evicted replays in the model's own dialect. It is
/// removed on the way out, here, because this is the one place a stored response becomes a response a caller reads
/// — GET and the list page both pass through it — so the published Responses schema stays exactly as it was.
nlohmann::json WireView(nlohmann::json response) {
  auto output = response.find("output");

  if (output == response.end() || !output->is_array()) {
    return response;
  }

  for (auto& item : *output) {
    if (item.is_object()) {
      item.erase(responses::kRawEnvelopeReplayKey);
    }
  }

  return response;
}

}  // namespace

// --- ResponseLease ---

ResponseLease::~ResponseLease() {
  Release();
}

ResponseLease::ResponseLease(ResponseLease&& other) noexcept
    : store_(std::exchange(other.store_, nullptr)), id_(std::exchange(other.id_, 0)) {
}

ResponseLease& ResponseLease::operator=(ResponseLease&& other) noexcept {
  if (this != &other) {
    Release();
    store_ = std::exchange(other.store_, nullptr);
    id_ = std::exchange(other.id_, 0);
  }

  return *this;
}

void ResponseLease::Release() noexcept {
  if (store_ != nullptr) {
    store_->ReleaseLease(id_);
    store_ = nullptr;
    id_ = 0;
  }
}

// --- ResponseStore ---

ResponseStore::ResponseStore(int capacity, IResponseCacheCoordinator* cache)
    : capacity_(std::clamp(capacity, 1, kMaxCapacity)), cache_(cache) {
}

void ResponseStore::Store(const std::string& response_id,
                          nlohmann::json response,
                          nlohmann::json input_items,
                          std::string model_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  StoreLocked(StoredResponse{response_id, std::move(model_id), std::move(response), std::move(input_items)});
}

void ResponseStore::StoreLocked(StoredResponse response) {
  std::shared_ptr<const ReplayPrefix> replay_prefix;

  // If already exists, preserve its compacted ancestry when the replacement keeps the same parent.
  auto it = index_.find(response.id);
  if (it != index_.end()) {
    const auto old_previous = it->second->response.find("previous_response_id");
    const auto new_previous = response.response.find("previous_response_id");
    if (old_previous != it->second->response.end() && new_previous != response.response.end() &&
        *old_previous == *new_previous) {
      replay_prefix = it->second->replay_prefix;
    }

    entries_.erase(it->second);
    index_.erase(it);
  }

  // Insert at front (most recently used)
  entries_.push_front(Entry{.id = response.id,
                            .model_id = std::move(response.model_id),
                            .response = std::move(response.response),
                            .input_items = std::move(response.input_items),
                            .replay_prefix = std::move(replay_prefix)});
  index_[response.id] = entries_.begin();

  Evict();
}

ResponseStore::ContinuationResult ResponseStore::BeginResponse(const std::string& previous_response_id,
                                                               const std::string& model_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  PendingResponse pending;

  if (!previous_response_id.empty()) {
    auto chain = WalkChainLocked(previous_response_id);
    if (chain.empty()) {
      ContinuationResult unavailable;
      unavailable.status = ContinuationStatus::kChainUnavailable;
      return unavailable;
    }

    // The chain belongs to the model that produced it: its cached session holds that model's KV cache, and its
    // replayed transcript was written by that model's template. Refuse before anything is reused, so warm and cold
    // continuation agree and no answer is labelled with a model that did not produce it.
    const std::string& parent_model_id = chain.front()->model_id;
    if (!parent_model_id.empty() && !model_id.empty() && parent_model_id != model_id) {
      ContinuationResult mismatch;
      mismatch.status = ContinuationStatus::kModelMismatch;
      mismatch.parent_model_id = parent_model_id;
      return mismatch;
    }

    // Keep the whole chain warm even when the caller continues from a live session and never rebuilds it: without
    // this the conversation's own early hops would age out while it is still active, and a later session-cache miss
    // could no longer reconstruct it. Touch oldest-first so the requested endpoint ends up most recent.
    for (auto hop = chain.rbegin(); hop != chain.rend(); ++hop) {
      TouchLocked(*hop);
    }

    pending.ancestors = ChainIdsLocked(chain);
  }

  const uint64_t lease_id = next_lease_id_++;
  pending_.emplace(lease_id, std::move(pending));

  ContinuationResult accepted;
  accepted.lease = ResponseLease(*this, lease_id);
  return accepted;
}

bool ResponseStore::Commit(ResponseLease& lease, StoredResponse response, IResponseAdmission* admission) {
  std::lock_guard<std::mutex> lock(mutex_);

  // A lease issued by a different store is not ours to consume: clearing it here would orphan that store's
  // registration forever. Refuse and leave it to its owner.
  if (lease.store_ != this) {
    return false;
  }

  // Our own lease is consumed either way: a rejected commit is final, and the caller must not retry into a
  // conversation whose ancestor is gone.
  auto pending = pending_.find(lease.id_);
  const bool valid = pending != pending_.end() && !pending->second.invalidated;

  if (pending != pending_.end()) {
    pending_.erase(pending);
  }

  lease.store_ = nullptr;
  lease.id_ = 0;

  if (!valid) {
    return false;
  }

  const std::string response_id = response.id;
  StoreLocked(std::move(response));

  // Admit under the same lock that published the metadata. A delete that arrives after this point sees the stored
  // entry, so it removes the metadata and drops the cached session together; one that arrived earlier invalidated
  // the lease and never reaches here. There is no ordering in between for a session to be resurrected in.
  if (admission != nullptr) {
    admission->Admit(response_id);
  }

  return true;
}

size_t ResponseStore::InFlightResponses() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return pending_.size();
}

void ResponseStore::ReleaseLease(uint64_t lease_id) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  pending_.erase(lease_id);
}

std::optional<nlohmann::json> ResponseStore::Get(const std::string& response_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = index_.find(response_id);
  if (it == index_.end()) {
    return std::nullopt;
  }

  TouchLocked(it->second);
  return WireView(it->second->response);
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

  if (deleted_ids.empty()) {
    return deleted_ids;
  }

  // Requests already generating cannot be recalled, but their results can be refused: any conversation that contains
  // a deleted response may no longer produce a stored or cached child.
  const std::unordered_set<std::string> deleted(deleted_ids.begin(), deleted_ids.end());
  for (auto& [lease_id, pending] : pending_) {
    if (pending.invalidated) {
      continue;
    }

    pending.invalidated = std::any_of(pending.ancestors.begin(), pending.ancestors.end(),
                                      [&deleted](const std::string& id) { return deleted.count(id) != 0; });
  }

  // Drop live sessions inside this critical section. Doing it after the lock would leave a window in which a session
  // is still checked out under an id whose metadata is already gone.
  if (cache_ != nullptr) {
    for (const auto& deleted_id : deleted_ids) {
      cache_->Drop(deleted_id);
    }
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
    page.data.push_back(WireView((*it)->response));
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

std::unordered_set<std::string> ResponseStore::ChainIdsLocked(
    const std::vector<std::list<Entry>::iterator>& chain) {
  std::unordered_set<std::string> ids;
  for (const auto& hop : chain) {
    ids.insert(hop->id);
  }

  // The oldest retained hop carries the compacted prefix, so the ids of already-evicted hops are still reachable —
  // deleting one of those must invalidate this conversation too.
  for (auto node = chain.back()->replay_prefix.get(); node != nullptr; node = node->previous.get()) {
    ids.insert(node->id);
  }

  return ids;
}

void ResponseStore::TouchLocked(std::list<Entry>::iterator it) {
  if (it != entries_.begin()) {
    entries_.splice(entries_.begin(), entries_, it);
  }
}

}  // namespace fl
