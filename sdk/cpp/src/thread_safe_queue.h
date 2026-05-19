// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <string>
#include <algorithm>

namespace foundry_local::Internal {

    enum class DequeueStatus {
        Item,
        Timeout,
        Closed,
        Error
    };

    /// A bounded, thread-safe queue with graceful close/error semantics.
    template <typename T> class ThreadSafeQueue final {
    public:
        explicit ThreadSafeQueue(size_t capacity) : capacity_(capacity) {}

        /// Blocking push. Waits until space is available or the queue is closed.
        /// Returns true if the item was enqueued, false if the queue was closed.
        bool Push(T item) {
            std::unique_lock<std::mutex> lock(mutex_);
            notFull_.wait(lock, [this] { return queue_.size() < capacity_ || closed_; });
            if (closed_) {
                return false;
            }
            queue_.push(std::move(item));
            notEmpty_.notify_one();
            return true;
        }

        /// Non-blocking push. Returns true if the item was enqueued.
        bool TryPush(T item) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_ || queue_.size() >= capacity_) {
                return false;
            }
            queue_.push(std::move(item));
            notEmpty_.notify_one();
            return true;
        }

        /// Timed push. Returns true if the item was enqueued within the timeout.
        bool TryPushFor(T item, std::chrono::milliseconds timeout) {
            std::unique_lock<std::mutex> lock(mutex_);
            if (!notFull_.wait_for(lock, timeout, [this] { return queue_.size() < capacity_ || closed_; })) {
                return false;
            }
            if (closed_) {
                return false;
            }
            queue_.push(std::move(item));
            notEmpty_.notify_one();
            return true;
        }

        /// Blocking pop. Waits until an item is available or the queue is closed/errored.
        DequeueStatus Pop(T& item) {
            std::unique_lock<std::mutex> lock(mutex_);
            notEmpty_.wait(lock, [this] { return !queue_.empty() || closed_ || hasError_; });
            if (hasError_ && queue_.empty()) {
                return DequeueStatus::Error;
            }
            if (queue_.empty()) {
                return DequeueStatus::Closed;
            }
            item = std::move(queue_.front());
            queue_.pop();
            notFull_.notify_one();
            return DequeueStatus::Item;
        }

        /// Timed pop. Returns the dequeue status.
        DequeueStatus TryPop(T& item, std::chrono::milliseconds timeout) {
            std::unique_lock<std::mutex> lock(mutex_);
            if (!notEmpty_.wait_for(lock, timeout, [this] { return !queue_.empty() || closed_ || hasError_; })) {
                return DequeueStatus::Timeout;
            }
            if (hasError_ && queue_.empty()) {
                return DequeueStatus::Error;
            }
            if (queue_.empty()) {
                return DequeueStatus::Closed;
            }
            item = std::move(queue_.front());
            queue_.pop();
            notFull_.notify_one();
            return DequeueStatus::Item;
        }

        /// Close the queue gracefully. No more items can be pushed.
        void Close() {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
            notEmpty_.notify_all();
            notFull_.notify_all();
        }

        /// Close the queue with an error message.
        void CloseWithError(std::string errorMessage) {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
            hasError_ = true;
            errorMessage_ = std::move(errorMessage);
            notEmpty_.notify_all();
            notFull_.notify_all();
        }

        bool IsClosed() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return closed_;
        }

        bool HasError() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return hasError_;
        }

        std::string GetErrorMessage() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return errorMessage_;
        }

    private:
        const size_t capacity_;
        std::queue<T> queue_;
        mutable std::mutex mutex_;
        std::condition_variable notEmpty_;
        std::condition_variable notFull_;
        bool closed_ = false;
        bool hasError_ = false;
        std::string errorMessage_;
    };

} // namespace foundry_local::Internal
