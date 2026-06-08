// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "items/item.h"
#include "util/key_value_pairs.h"

#include <atomic>
#include <memory>
#include <vector>

namespace fl {

/// Generic inference request — pure input data.
/// Items are stored as borrowed pointers. Owned items are kept alive in owned_items.
struct Request {
  std::vector<Item*> items;  // all items (borrowed pointers)
  KeyValuePairs options;

  /// Cancellation flag — set by the C API or streaming callback handler to cancel
  /// an in-flight request. Checked in generation loops. Atomic because it is written
  /// by one thread (callback worker or C API) and read by another (generator loop).
  /// Uses relaxed ordering since it is a one-way flag and exact timing doesn't matter.
  mutable std::atomic<bool> canceled{false};

  Request() = default;

  Request(Request&& other) noexcept
      : items(std::move(other.items)),
        options(std::move(other.options)),
        canceled(other.canceled.load(std::memory_order_relaxed)),
        owned_items(std::move(other.owned_items)) {}

  Request& operator=(Request&& other) noexcept {
    items = std::move(other.items);
    options = std::move(other.options);
    canceled.store(other.canceled.load(std::memory_order_relaxed), std::memory_order_relaxed);
    owned_items = std::move(other.owned_items);
    return *this;
  }

  Request(const Request&) = delete;
  Request& operator=(const Request&) = delete;

  /// Add a pre-allocated owned item.
  void AddOwnedItem(std::unique_ptr<Item> item) {
    items.push_back(item.get());
    owned_items.push_back(std::move(item));
  }

  /// Add a borrowed item (caller must keep it alive).
  void AddBorrowedItem(Item* item) {
    items.push_back(item);
  }

 private:
  std::vector<std::unique_ptr<Item>> owned_items;  // owned items (lifetime)
};

}  // namespace fl
