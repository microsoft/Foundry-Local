// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Stub implementation of MakeLiveCatalogClient. Compiled in builds where
// the live Azure catalog client source files (azure_catalog_client.{h,cc})
// are not present — typically the public-repo build that ships only the
// embedded snapshot.
//
// The stub causes any non-"static" base_url to throw, which surfaces a
// clear error message instead of silently falling back to something
// unexpected.
#include "catalog/catalog_client.h"
#include "exception.h"

#include <foundry_local/foundry_local_c.h>

#include <fmt/format.h>

#include <memory>
#include <string>

namespace fl {

std::unique_ptr<ICatalogClient> MakeLiveCatalogClient(
    const std::string& base_url,
    const std::string& /*filter_override*/,
    const IEpDetector& /*ep_detector*/,
    ILogger& /*logger*/,
    const std::string& /*cache_directory*/) {
  FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
           fmt::format("Live Azure catalog client is not available in this build "
                       "(requested url='{}'). Configure the catalog url to \"static\" "
                       "to use the embedded snapshot.",
                       base_url));
}

}  // namespace fl
