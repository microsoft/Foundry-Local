// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "manager.h"

#include "addon_data.h"
#include "catalog.h"
#include "errors.h"

#include <foundry_local/foundry_local_c.h>
#include <foundry_local/foundry_local_cpp.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace foundry_local_node {

namespace {

Napi::Value ConvertEndpoints(Napi::Env env, std::vector<std::string>& endpoints) {
  Napi::Array out = Napi::Array::New(env, endpoints.size());
  for (size_t i = 0; i < endpoints.size(); ++i) {
    out.Set(static_cast<uint32_t>(i), Napi::String::New(env, endpoints[i]));
  }
  return out;
}

// Build a tagged FoundryLocalError pending on env. Mirrors errors.cc's
// internal `MakeFoundryLocalError` — duplicated here to avoid exposing the
// helper publicly just for the disposed-manager path.
void ThrowFoundryLocalError(Napi::Env env, int code, const std::string& msg) {
  Napi::Error err = Napi::Error::New(env, msg);
  Napi::Object value = err.Value();
  value.Set("name", Napi::String::New(env, "FoundryLocalError"));
  value.Set("code", Napi::Number::New(env, code));
  err.ThrowAsJavaScriptException();
}

}  // namespace

Napi::Function Manager::Init(Napi::Env env) {
  return DefineClass(env, "Manager",
                     {
                         InstanceMethod("getWebServiceEndpoints", &Manager::GetWebServiceEndpoints),
                         InstanceMethod("getCatalog", &Manager::GetCatalog),
                         InstanceMethod("dispose", &Manager::Dispose),
                         InstanceMethod("isDisposed", &Manager::IsDisposed),
                     });
}

Manager::Manager(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Manager>(info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Manager constructor expects an options object").ThrowAsJavaScriptException();
    return;
  }
  Napi::Object opts = info[0].As<Napi::Object>();
  if (!opts.Has("appName") || !opts.Get("appName").IsString()) {
    Napi::TypeError::New(env, "options.appName must be a string").ThrowAsJavaScriptException();
    return;
  }
  std::string app_name = opts.Get("appName").As<Napi::String>();

  // Optional Configuration setters — type-checked when present.
  std::string model_cache_dir;
  bool has_model_cache_dir = false;
  if (opts.Has("modelCacheDir") && !opts.Get("modelCacheDir").IsUndefined() &&
      !opts.Get("modelCacheDir").IsNull()) {
    if (!opts.Get("modelCacheDir").IsString()) {
      Napi::TypeError::New(env, "options.modelCacheDir must be a string").ThrowAsJavaScriptException();
      return;
    }
    model_cache_dir = opts.Get("modelCacheDir").As<Napi::String>();
    has_model_cache_dir = true;
  }
  std::string external_service_url;
  bool has_external_service_url = false;
  if (opts.Has("externalServiceUrl") && !opts.Get("externalServiceUrl").IsUndefined() &&
      !opts.Get("externalServiceUrl").IsNull()) {
    if (!opts.Get("externalServiceUrl").IsString()) {
      Napi::TypeError::New(env, "options.externalServiceUrl must be a string").ThrowAsJavaScriptException();
      return;
    }
    external_service_url = opts.Get("externalServiceUrl").As<Napi::String>();
    has_external_service_url = true;
  }

  CallCheckedVoid(env, [&]() {
    foundry_local::Configuration config(app_name);
    if (has_model_cache_dir) {
      config.SetModelCacheDir(model_cache_dir);
    }
    if (has_external_service_url) {
      config.SetExternalServiceUrl(external_service_url);
    }
    impl_ = std::make_unique<foundry_local::Manager>(std::move(config));
  });
}

bool Manager::ThrowIfDisposed(Napi::Env env) {
  if (impl_ == nullptr) {
    ThrowFoundryLocalError(env, FOUNDRY_LOCAL_ERROR_INVALID_USAGE, "Manager has been disposed");
    return true;
  }
  return false;
}

Napi::Value Manager::GetWebServiceEndpoints(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (ThrowIfDisposed(env)) {
    return env.Undefined();
  }
  return CallChecked<Napi::Value>(env, [&]() -> Napi::Value {
    std::vector<std::string> endpoints = impl_->GetWebServiceEndpoints();
    return ConvertEndpoints(env, endpoints);
  });
}

Napi::Value Manager::GetCatalog(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (ThrowIfDisposed(env)) {
    return env.Undefined();
  }
  Napi::ObjectReference owner = Napi::Reference<Napi::Object>::New(info.This().As<Napi::Object>(), 1);
  return CallChecked<Napi::Value>(env, [&]() -> Napi::Value {
    foundry_local::ICatalog& cat = impl_->GetCatalog();
    CatalogCtorToken token;
    token.impl = &cat;
    token.manager = std::move(owner);
    return Catalog::NewInstance(env, std::move(token));
  });
}

Napi::Value Manager::Dispose(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  // Idempotent — releasing an already-null unique_ptr is a no-op.
  impl_.reset();
  return env.Undefined();
}

Napi::Value Manager::IsDisposed(const Napi::CallbackInfo& info) {
  return Napi::Boolean::New(info.Env(), impl_ == nullptr);
}

}  // namespace foundry_local_node
