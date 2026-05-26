// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Conversion helpers between JS-side discriminated-union Item objects and
// `foundry_local::Item` from the C++ wrapper.
//
// The JS surface intentionally uses plain JS objects rather than ObjectWrap
// classes for items:
//   * inputs (Request.addItem) — caller passes `{ type: 'message', ... }`
//   * outputs (Response output items) — addon returns the same shape
// This keeps the API ergonomic and avoids per-item native handle lifetime
// management on the JS heap. The cost is one full copy per item across the
// JS<->C++ boundary; acceptable for chat workloads.
//
// Supported input subtypes:
//   text, message (with optional typed parts), toolCall, toolResult,
//   image (uri only), audio (uri only).
// Bytes/Tensor + image-from-data + audio-from-data input are not yet wired
// — they need a raw-bytes pinning contract on the addItem boundary.
//
// Supported output subtypes: full read dispatch for all 8 types
// (text, message, bytes, tensor, image, audio, toolCall, toolResult).
#pragma once

#include <napi.h>

#include <foundry_local/foundry_local_cpp.h>

namespace foundry_local_node {

/// Convert a C++ Item (non-owning view, typically obtained from a Response)
/// to a JS plain object. The returned object is fully owned by the JS GC and
/// has no native backing — safe to retain past the source Item's lifetime.
Napi::Value ItemToJs(Napi::Env env, const foundry_local::Item& item);

/// Convert a JS plain object describing an input item into an owning
/// `foundry_local::Item`. Throws `Napi::TypeError` for an invalid shape.
/// The returned Item is move-only and owns its underlying flItem.
foundry_local::Item JsToItem(Napi::Env env, const Napi::Value& value);

}  // namespace foundry_local_node
