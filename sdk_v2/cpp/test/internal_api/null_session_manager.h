// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "inferencing/session/session_manager.h"

namespace fl {
namespace test {

/// No-op session manager for unit tests that don't need tracking.
class NullSessionManager : public ISessionManager {
 public:
  void Register(Session&) override {}
  void Deregister(Session&) override {}
};

}  // namespace test
}  // namespace fl
