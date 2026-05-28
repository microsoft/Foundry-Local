// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "inferencing/session/session_manager.h"

#include <cassert>
#include <utility>

namespace fl {

class Session;

/// RAII guard that registers a Session with a SessionManager for its lifetime.
/// Use at the call site where the session's address is final (no further moves).
///
/// Move-only (source is nulled on move). Non-copyable.
/// Destruction order matters: destroy the guard before the session.
class SessionRegistration {
 public:
  SessionRegistration(ISessionManager& manager, Session& session)
      : manager_(&manager), session_(&session) {
    manager_->Register(*session_);
  }

  ~SessionRegistration() {
    if (manager_) {
      manager_->Deregister(*session_);
    }
  }

  // Non-copyable
  SessionRegistration(const SessionRegistration&) = delete;
  SessionRegistration& operator=(const SessionRegistration&) = delete;

  // Movable (transfers ownership, nulls source)
  SessionRegistration(SessionRegistration&& other) noexcept
      : manager_(std::exchange(other.manager_, nullptr)),
        session_(std::exchange(other.session_, nullptr)) {}

  SessionRegistration& operator=(SessionRegistration&&) = delete;

  /// Explicitly deregister and disarm the guard.
  /// Call before CheckIn to avoid a race where another thread checks out
  /// the cached session while this guard still holds the registration.
  void Release() {
    if (manager_) {
      manager_->Deregister(*session_);
      manager_ = nullptr;
    }
  }

 private:
  ISessionManager* manager_;
  Session* session_;
};

}  // namespace fl
