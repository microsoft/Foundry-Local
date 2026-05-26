// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "model.h"

#include "addon_data.h"
#include "errors.h"
#include "promise_worker.h"

#include <foundry_local/foundry_local_c.h>
#include <foundry_local/foundry_local_cpp.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace foundry_local_node {

namespace {

const char* DeviceTypeToString(flDeviceType dt) {
  switch (dt) {
    case FOUNDRY_LOCAL_DEVICE_CPU:
      return "CPU";
    case FOUNDRY_LOCAL_DEVICE_GPU:
      return "GPU";
    case FOUNDRY_LOCAL_DEVICE_NPU:
      return "NPU";
    case FOUNDRY_LOCAL_DEVICE_NOTSET:
    default:
      return "Invalid";
  }
}

// Set `obj[key] = value` from a std::optional<std::string_view>. Omits the
// property when the optional is empty so the JS shape matches `?: T`.
void SetOptionalString(Napi::Env env, Napi::Object obj, const char* key,
                       const std::optional<std::string_view>& value) {
  if (value.has_value()) {
    obj.Set(key, Napi::String::New(env, std::string(*value)));
  }
}

void SetOptionalNumber(Napi::Env env, Napi::Object obj, const char* key,
                       const std::optional<int64_t>& value) {
  if (value.has_value()) {
    obj.Set(key, Napi::Number::New(env, static_cast<double>(*value)));
  }
}

void SetOptionalBool(Napi::Env env, Napi::Object obj, const char* key,
                     const std::optional<bool>& value) {
  if (value.has_value()) {
    obj.Set(key, Napi::Boolean::New(env, *value));
  }
}

Napi::Object SnapshotModelInfo(Napi::Env env, const foundry_local::ModelInfo& info) {
  Napi::Object out = Napi::Object::New(env);

  // Required fields.
  out.Set("id", Napi::String::New(env, std::string(info.Id())));
  out.Set("name", Napi::String::New(env, std::string(info.Name())));
  out.Set("version", Napi::Number::New(env, info.Version()));
  out.Set("alias", Napi::String::New(env, std::string(info.Alias())));
  out.Set("uri", Napi::String::New(env, std::string(info.Uri())));
  out.Set("deviceType", Napi::String::New(env, DeviceTypeToString(info.DeviceType())));
  out.Set("createdAtUnix", Napi::Number::New(env, static_cast<double>(info.CreatedAtUnix())));
  out.Set("isTestModel", Napi::Boolean::New(env, info.IsTestModel()));

  // Optional fields.
  SetOptionalString(env, out, "executionProvider", info.ExecutionProvider());
  SetOptionalString(env, out, "displayName", info.DisplayName());
  SetOptionalString(env, out, "modelType", info.ModelType());
  SetOptionalString(env, out, "publisher", info.Publisher());
  SetOptionalString(env, out, "license", info.License());
  SetOptionalString(env, out, "licenseDescription", info.LicenseDescription());
  SetOptionalString(env, out, "task", info.Task());
  SetOptionalString(env, out, "modelProvider", info.ModelProvider());
  SetOptionalString(env, out, "minFlVersion", info.MinFlVersion());
  SetOptionalString(env, out, "parentUri", info.ParentUri());
  SetOptionalBool(env, out, "supportsToolCalling", info.SupportsToolCalling());
  SetOptionalNumber(env, out, "filesizeMb", info.FilesizeMb());
  SetOptionalNumber(env, out, "maxOutputTokens", info.MaxOutputTokens());
  SetOptionalNumber(env, out, "contextLength", info.ContextLength());
  SetOptionalString(env, out, "inputModalities", info.InputModalities());
  SetOptionalString(env, out, "outputModalities", info.OutputModalities());
  SetOptionalString(env, out, "capabilities", info.Capabilities());

  return out;
}

// Drain a ModelList into a JS array, with each entry wrapped as a JS Model
// whose keepalive holds the shared ModelList.
Napi::Array WrapModelList(Napi::Env env, std::shared_ptr<foundry_local::ModelList> list,
                          Napi::ObjectReference manager) {
  auto models = list->Models();
  Napi::Array arr = Napi::Array::New(env, models.size());
  for (size_t i = 0; i < models.size(); ++i) {
    ModelCtorToken token;
    token.impl = models[i].get();
    token.keepalive = list;  // shared_ptr copy keeps the ModelList alive
    // Cloning the manager ObjectReference per Model so each entry pins it.
    token.manager = Napi::Reference<Napi::Object>::New(manager.Value(), 1);
    arr.Set(static_cast<uint32_t>(i), Model::NewInstance(env, std::move(token)));
  }
  return arr;
}

}  // namespace

Napi::Function Model::Init(Napi::Env env) {
  return DefineClass(env, "Model",
                     {
                         InstanceMethod("getInfo", &Model::GetInfo),
                         InstanceMethod("isCached", &Model::IsCached),
                         InstanceMethod("isLoaded", &Model::IsLoaded),
                         InstanceMethod("getPath", &Model::GetPath),
                         InstanceMethod("getVariants", &Model::GetVariants),
                         InstanceMethod("load", &Model::Load),
                         InstanceMethod("unload", &Model::Unload),
                         InstanceMethod("download", &Model::Download),
                     });
}

Napi::Object Model::NewInstance(Napi::Env env, ModelCtorToken token) {
  auto* heap = new ModelCtorToken(std::move(token));
  auto ext = Napi::External<ModelCtorToken>::New(
      env, heap, [](Napi::Env /*env*/, ModelCtorToken* t) { delete t; });
  auto* data = env.GetInstanceData<AddonData>();
  return data->model_ctor.New({ext});
}

Model::Model(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Model>(info) {
  Napi::Env env = info.Env();
  if (info.Length() != 1 || !info[0].IsExternal()) {
    Napi::TypeError::New(env, "Model is internal — obtain instances via Catalog/Manager methods")
        .ThrowAsJavaScriptException();
    return;
  }
  auto* token = info[0].As<Napi::External<ModelCtorToken>>().Data();
  if (token == nullptr || token->impl == nullptr) {
    Napi::TypeError::New(env, "Model: invalid internal construction token").ThrowAsJavaScriptException();
    return;
  }
  impl_ = token->impl;
  keepalive_ = std::move(token->keepalive);
  manager_ = std::move(token->manager);
}

Napi::Value Model::GetInfo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  return CallChecked<Napi::Value>(env, [&]() -> Napi::Value {
    foundry_local::ModelInfo mi = impl_->GetInfo();
    return SnapshotModelInfo(env, mi);
  });
}

Napi::Value Model::IsCached(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  return CallChecked<Napi::Value>(env, [&]() -> Napi::Value {
    return Napi::Boolean::New(env, impl_->IsCached());
  });
}

Napi::Value Model::IsLoaded(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  return CallChecked<Napi::Value>(env, [&]() -> Napi::Value {
    return Napi::Boolean::New(env, impl_->IsLoaded());
  });
}

Napi::Value Model::GetPath(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  return CallChecked<Napi::Value>(env, [&]() -> Napi::Value {
    std::string_view p = impl_->GetPath();
    return Napi::String::New(env, std::string(p));
  });
}

Napi::Value Model::GetVariants(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::ObjectReference owner_clone = Napi::Reference<Napi::Object>::New(manager_.Value(), 1);
  return CallChecked<Napi::Value>(env, [&]() -> Napi::Value {
    auto list = std::make_shared<foundry_local::ModelList>(impl_->GetVariants());
    return WrapModelList(env, std::move(list), std::move(owner_clone));
  });
}

// ── Async lifecycle ─────────────────────────────────────────────────────────
//
// Load/Unload/Download dispatch the underlying virtual call onto a libuv
// worker so the event loop stays responsive. The Model itself is pinned
// against GC for the duration of the worker via an ObjectReference to the
// parent Manager (the Manager owns the catalog whose ModelList views the
// IModel*).

Napi::Value Model::Load(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (impl_ == nullptr) {
    Napi::Error::New(env, "Model: not initialized").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  Napi::ObjectReference owner = Napi::Reference<Napi::Object>::New(manager_.Value(), 1);
  foundry_local::IModel* m = impl_;
  return PromiseWorkerVoid::Run(
      env, [m]() { m->Load(); }, std::move(owner));
}

Napi::Value Model::Unload(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (impl_ == nullptr) {
    Napi::Error::New(env, "Model: not initialized").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  Napi::ObjectReference owner = Napi::Reference<Napi::Object>::New(manager_.Value(), 1);
  foundry_local::IModel* m = impl_;
  return PromiseWorkerVoid::Run(
      env, [m]() { m->Unload(); }, std::move(owner));
}

Napi::Value Model::Download(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (impl_ == nullptr) {
    Napi::Error::New(env, "Model: not initialized").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  // Progress-callback variant is not yet wired (needs ThreadSafeFunction
  // plumbing identical to the streaming bridge); pass nullptr for now.
  Napi::ObjectReference owner = Napi::Reference<Napi::Object>::New(manager_.Value(), 1);
  foundry_local::IModel* m = impl_;
  return PromiseWorkerVoid::Run(
      env, [m]() { m->Download(nullptr); }, std::move(owner));
}

}  // namespace foundry_local_node
