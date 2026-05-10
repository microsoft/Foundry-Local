// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "items/item.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>

namespace fl {

/// A queue of sub-items for streaming or batched delivery.
struct ItemQueue : Item {
  ItemQueue() : Item(FOUNDRY_LOCAL_ITEM_QUEUE) {}

  ItemQueue(const ItemQueue& other) = delete;
  ItemQueue& operator=(const ItemQueue& other) = delete;

  /// Push an item into the queue. Transfers ownership.
  void Push(std::unique_ptr<Item> item) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      items.push_back(std::move(item));
    }
    cv.notify_one();
  }

  /// Try to pop the front item. Returns nullptr if the queue is empty.
  std::unique_ptr<Item> TryPop() {
    std::lock_guard<std::mutex> lock(mutex);
    if (items.empty()) {
      return nullptr;
    }

    auto front = std::move(items.front());
    items.pop_front();
    return front;
  }

  /// Get the number of items currently in the queue.
  size_t Size() const {
    std::lock_guard<std::mutex> lock(mutex);
    return items.size();
  }

  /// Mark the queue as finished (no more items will be pushed).
  void MarkFinished() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      finished = true;
    }
    cv.notify_all();
  }

  /// Check whether the queue has been marked finished.
  bool IsFinished() const {
    std::lock_guard<std::mutex> lock(mutex);
    return finished;
  }

  /// Wait up to `timeout` for an item to become available.
  /// Returns the item if one was available (immediately or after waiting).
  /// Returns nullptr if the timeout expired or the queue is finished and empty.
  /// No loop — the caller decides what to do on nullptr (check cancellation, retry, etc.).
  std::unique_ptr<Item> WaitAndPop(std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
    std::unique_lock<std::mutex> lock(mutex);

    if (items.empty() && !finished) {
      cv.wait_for(lock, timeout);
    }

    if (!items.empty()) {
      auto front = std::move(items.front());
      items.pop_front();
      return front;
    }

    return nullptr;
  }

  flItemQueue* AsApiType() noexcept;

 private:
  mutable std::mutex mutex;
  std::condition_variable cv;
  std::deque<std::unique_ptr<Item>> items;
  bool finished = false;
};

}  // namespace fl
