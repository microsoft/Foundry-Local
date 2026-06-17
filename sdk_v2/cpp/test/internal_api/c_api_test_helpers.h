// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Shared helpers for tests that exercise the C ABI (foundry_local_c.h).
#pragma once

#include <foundry_local/foundry_local_c.h>
#include <gtest/gtest.h>

namespace fl::test {

// Returns true if the status indicates success (nullptr).
inline bool IsOk(flStatus* s) { return s == nullptr; }

// RAII helper to release a status via the API vtable.
struct StatusGuard {
  flStatus* s = nullptr;
  const flApi* api = nullptr;
  ~StatusGuard() {
    if (s && api) {
      api->Status_Release(s);
    }
  }
};

// Assert that a C API call succeeded. On failure, prints the error code and message then aborts the
// current test (FAIL is fatal like ASSERT_*). Usage: ASSERT_FL_OK(api, api->Manager_Create(config, &mgr));
#define ASSERT_FL_OK(api_ptr, status_expr)                                      \
  do {                                                                          \
    ::fl::test::StatusGuard _sg{(status_expr), (api_ptr)};                      \
    if (_sg.s != nullptr) {                                                     \
      FAIL() << "C API error " << (api_ptr)->Status_GetErrorCode(_sg.s) << ": " \
             << (api_ptr)->Status_GetErrorMessage(_sg.s);                       \
    }                                                                           \
  } while (0)

// Convenience: get the API table or abort the test.
inline const flApi* GetApi() {
  const flApi* api = FoundryLocalGetApi(FOUNDRY_LOCAL_API_VERSION);
  return api;
}

// Create a Configuration suitable for testing. On Android, sets app_data_dir to a writable
// path so tests work without depending on $HOME (which may not be set for the shell user).
inline flConfiguration* CreateTestConfig(const flApi* api, const char* app_name = "test-app") {
  const flConfigurationApi* config_api = api->GetConfigurationApi();
  flConfiguration* config = nullptr;
  flStatus* s = config_api->Create(app_name, &config);
  if (s != nullptr) {
    api->Status_Release(s);
    return nullptr;
  }
#ifdef __ANDROID__
  s = config_api->SetAppDataDir(config, "./test-app-data");
  if (s != nullptr) {
    api->Status_Release(s);
  }
#endif
  return config;
}

}  // namespace fl::test
