// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

/// @file c_api_types.h
/// Concrete definitions for the opaque C API types that are empty derivations
/// of internal fl:: types. These exist so the C API can hand out typed pointers
/// while the internal code works with fl:: types directly.
///
/// Keeping these in a shared header (rather than buried in c_api.cc) lets
/// internal code static_cast between fl::Type and flType without reinterpret_cast.

#include "configuration.h"
#include "inferencing/session/request.h"
#include "inferencing/session/response.h"
#include "inferencing/session/session.h"
#include "items/item.h"
#include "items/item_queue.h"
#include "model.h"
#include "model_info.h"
#include "util/key_value_pairs.h"

// --- KeyValuePairs ---
struct flKeyValuePairs : fl::KeyValuePairs {
  using fl::KeyValuePairs::KeyValuePairs;
};

// --- Configuration ---
struct flConfiguration : fl::Configuration {
  using fl::Configuration::Configuration;
};

// --- ModelInfo ---
struct flModelInfo : fl::ModelInfo {
  using fl::ModelInfo::ModelInfo;
};

// --- Model ---
struct flModel : fl::Model {
  using fl::Model::Model;
};

// --- Item ---
struct flItem : fl::Item {
  using fl::Item::Item;
};

// --- ItemQueue ---
struct flItemQueue : fl::ItemQueue {
  using fl::ItemQueue::ItemQueue;
};

// --- Request ---
struct flRequest : fl::Request {
  using fl::Request::Request;
};

// --- Response ---
struct flResponse : fl::Response {
  using fl::Response::Response;
};

// --- Session ---
struct flSession : fl::Session {
  using fl::Session::Session;
};
