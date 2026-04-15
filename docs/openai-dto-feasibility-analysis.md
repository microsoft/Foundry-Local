# Feasibility Analysis: OpenAI DTO Packages for Foundry-Local SDK

## Executive Summary

This document evaluates the feasibility of replacing the current 3rd-party OpenAI DTO libraries used by the Foundry-Local SDK with (a) official OpenAI packages, (b) custom-built DTOs, or (c) auto-generated types from the OpenAI OpenAPI specification. The analysis covers all five SDK languages: C#, Rust, Python, JavaScript/TypeScript, and C++.

**Key Finding:** The current approach is inconsistent across languages — Python already uses the official `openai` package, JS and C++ use fully custom types, while C# and Rust depend on 3rd-party community libraries. A unified strategy is needed, but no single option is a silver bullet. The recommended path is a **hybrid approach**: adopt official packages where available and well-suited (Python, C#), and maintain custom types where no official package exists or the integration is minimal (Rust, JS, C++).

---

## 1. Current State Analysis

### 1.1 SDK-by-SDK Dependency Map

| SDK | OpenAI DTO Source | Package/Approach | Version | Types Exposed in Public API |
|-----|-------------------|------------------|---------|-----------------------------|
| **C#** | 3rd-party community | `Betalgo.Ranul.OpenAI` | 9.1.0 | `ChatMessage`, `ChatCompletionCreateResponse`, `ToolDefinition`, `ToolChoice`, `ResponseFormat` directly in method signatures |
| **Rust** | 3rd-party community | `async-openai` | 0.33 (feature: `chat-completion-types` only) | 25 types re-exported: `ChatCompletionRequestMessage`, `CreateChatCompletionResponse`, `ChatCompletionTools`, etc. |
| **Python** | Official OpenAI | `openai` | ≥2.24.0 | `ChatCompletion`, `ChatCompletionChunk`, `ChatCompletionMessageParam` in method signatures |
| **JS/TS** | Custom-defined | Self-contained in `types.ts` | N/A | All types custom: `ResponseCreateParams`, `ResponseObject`, `StreamingEvent`, `MessageItem`, etc. |
| **C++** | Custom-defined | Self-contained headers | N/A | `ChatMessage`, `ChatCompletionCreateResponse`, `ToolDefinition`, `ChatSettings`, etc. — all custom structs |

### 1.2 Integration Depth

**C# (HIGH integration with Betalgo)**
- Betalgo types are directly exposed in all public method signatures (`CompleteChatAsync` accepts `IEnumerable<ChatMessage>`, returns `ChatCompletionCreateResponse`)
- SDK extends Betalgo types: `ChatCompletionCreateRequestExtended` inherits from `ChatCompletionCreateRequest`; `ResponseFormatExtended` inherits from `ResponseFormat`
- JSON serialization uses System.Text.Json source-generated context that directly references Betalgo types
- Foundry-specific extensions (LARK grammar, metadata for `top_k`/`random_seed`) are built on top of Betalgo base classes

**Rust (MODERATE integration with async-openai)**
- 25 types re-exported from `async-openai::types::chat` in `lib.rs`
- Uses only the `chat-completion-types` feature flag — no HTTP client dependency
- Custom enums `ChatResponseFormat` and `ChatToolChoice` wrap Foundry-specific behavior
- Serialization is manually handled via `serde_json::Value`, not through async-openai's client

**Python (MODERATE integration with official openai)**
- Uses `openai.types.chat` types for method signatures and response parsing
- Leverages Pydantic `model_validate_json()` for response deserialization
- Audio client uses custom types, not OpenAI types
- Already using the official package — the pattern this analysis evaluates for other SDKs

**JS/TS (ZERO external dependency)**
- All ~50+ OpenAI-compatible types are custom-defined in `types.ts`
- Includes full Responses API types, streaming events, content parts, tool calling
- No external OpenAI dependency at all — fully self-contained

**C++ (ZERO external dependency)**
- All types are custom structs in header files
- JSON serialization uses nlohmann::json with ADL overloads
- Complete control over the type surface

### 1.3 Identified Risks with Current 3rd-Party Libraries

| Risk | C# (Betalgo) | Rust (async-openai) |
|------|--------------|---------------------|
| **No official support** | ✅ Community-maintained, single primary developer | ✅ Community-maintained, single primary developer |
| **Maintenance continuity** | Medium risk — active but has had package rename (Betalgo.OpenAI → Betalgo.Ranul.OpenAI), open issues with dependency resolution | Medium risk — actively maintained (v0.34.0, April 2026), but sole maintainer |
| **Breaking changes** | High — namespace/package rename already occurred; extends request types that may change without notice | Medium — feature flags limit exposure, but type definition updates can break compilation |
| **API coverage gaps** | Yes — missing `metadata` field on `ChatCompletionCreateRequest` (SDK had to extend); `ResponseFormat` needed extension for LARK grammar | Minimal — only using type definitions, not the full API surface |
| **Reliability** | Open issues with iOS compatibility, missing endpoints (v1/responses), LM Studio compatibility | Generally reliable for type definitions; WASM limitations noted |
| **Licensing** | MIT License | MIT License |

---

## 2. Official OpenAI Packages Assessment

### 2.1 Available Official Packages

| Language | Package | Latest Version | Maintained By | Custom Endpoint Support |
|----------|---------|---------------|---------------|------------------------|
| **C# (.NET)** | `OpenAI` (NuGet) | 2.10.0 | OpenAI + Microsoft | ✅ `OpenAIClientOptions.Endpoint` |
| **Python** | `openai` (PyPI) | 2.31.0 | OpenAI | ✅ `base_url` parameter |
| **JavaScript** | `openai` (npm) | 6.33.0 | OpenAI | ✅ `baseURL` parameter |
| **Rust** | ❌ None | N/A | N/A | N/A |
| **C++** | ❌ None | N/A | N/A | N/A |
| **Go** | `openai-go` | Available | OpenAI | ✅ |
| **Java** | `openai-java` | Available | OpenAI | ✅ |

### 2.2 Detailed Analysis: Official OpenAI C# SDK (`OpenAI` NuGet v2.10.0)

**Pros:**
- ✅ Officially maintained by OpenAI with Microsoft collaboration
- ✅ Stable, production-ready (v2.x since October 2024)
- ✅ Comprehensive type coverage — all OpenAI endpoints, including Responses API
- ✅ Supports custom endpoint via `OpenAIClientOptions.Endpoint`
- ✅ .NET Standard 2.0 and .NET 8.0+ compatible
- ✅ Source-generated JSON serialization for AOT compatibility
- ✅ Strongly typed with comprehensive models

**Cons and Concerns:**
- ⚠️ **Foundry-Local doesn't use HTTP** — The SDK is designed around an HTTP client. Foundry-Local communicates with a native core via FFI/interop, not HTTP endpoints. Using the full SDK would mean taking a dependency on types that are deeply coupled to an HTTP client pipeline.
- ⚠️ **Type extraction complexity** — The official SDK types are not cleanly separated from the client infrastructure. Types live in `OpenAI.Chat`, `OpenAI.Audio`, etc. and may have internal dependencies on the SDK's pipeline types (e.g., `ClientResult<T>`, `BinaryContent`).
- ⚠️ **Foundry-specific extensions are harder** — Betalgo allows inheritance (`ChatCompletionCreateRequestExtended : ChatCompletionCreateRequest`). The official SDK uses `sealed` or record types that may not be as extensible for adding custom fields like `lark_grammar` or Foundry-specific `metadata`.
- ⚠️ **Dependency weight** — The official SDK brings in `System.ClientModel` and other Azure SDK infrastructure packages. For a lightweight native-FFI SDK, this may be excessive.
- ⚠️ **Serialization format** — Official SDK types may use different JSON serialization strategies than what Foundry's native core expects. Subtle differences in property naming, null handling, or enum serialization could cause compatibility issues.
- ⚠️ **Breaking changes from OpenAI** — As OpenAI evolves the API (e.g., transition from Chat Completions to Responses API), the official SDK's types will change accordingly, potentially breaking Foundry-Local's integration.
- ⚠️ **Update velocity mismatch** — Foundry-Local runs local models and supports a subset of the OpenAI API. The official SDK may add types and features faster than Foundry-Local can support them, creating confusion about what is/isn't supported.

### 2.3 Detailed Analysis: Official OpenAI Python SDK (`openai` PyPI v2.31.0)

**Current Status:** Already in use by Foundry-Local Python SDK.

**Pros:**
- ✅ Already adopted — validates the approach works
- ✅ Pydantic-based types enable easy JSON validation (`model_validate_json()`)
- ✅ Clean type-only imports possible (`from openai.types.chat import ChatCompletion`)
- ✅ Types are well-documented with type hints

**Cons and Lessons Learned:**
- ⚠️ **Full package dependency** — The Python SDK depends on `openai>=2.24.0` which includes the full HTTP client, authentication machinery, and other features not used by Foundry-Local
- ⚠️ **Minimum Python version coupling** — The openai package requires Python 3.8+ and may increase this, potentially conflicting with Foundry-Local's own requirements (≥3.11)
- ⚠️ **Heavyweight for type-only usage** — The `openai` package installs `httpx`, `pydantic`, `typing_extensions`, `distro`, etc. Only the Pydantic models are actually used
- ⚠️ **Version sensitivity** — As OpenAI adds new API features, the types may grow or change. The SDK pins `>=2.24.0` which means any breaking change in a minor version could affect Foundry-Local

### 2.4 Detailed Analysis: Official OpenAI JavaScript SDK (`openai` npm v6.33.0)

**Current Status:** NOT used. JS SDK has all types custom-defined.

**Pros:**
- ✅ Comprehensive TypeScript types for all OpenAI endpoints
- ✅ Supports `baseURL` for custom endpoints
- ✅ Regular updates with latest API features

**Cons and Concerns:**
- ⚠️ **JS SDK already has custom types** — Migrating would mean replacing a well-tested, self-contained type system with an external dependency for no clear benefit
- ⚠️ **Types may not be cleanly extractable** — The npm `openai` package couples types with the client infrastructure
- ⚠️ **Current JS SDK uses `any` for chat responses** — The chat client currently returns loosely-typed responses, so the types aren't heavily used in that path. Only the Responses API uses the rich custom types
- ⚠️ **Package size** — The full `openai` npm package is significantly larger than the current `types.ts` approach

### 2.5 Rust and C++ — No Official Package Available

**Rust:**
- OpenAI has no official Rust SDK. Community options: `async-openai` (current, actively maintained), `openai_rust_sdk`, `openai_api_rust`
- The current `async-openai` dependency is limited to the `chat-completion-types` feature flag, meaning only type definitions are pulled in — no runtime/HTTP client dependency
- This is the most pragmatic current approach for Rust

**C++:**
- OpenAI has no official C++ SDK
- The C++ SDK already uses fully custom types, which is the correct approach
- No change needed

---

## 3. Alternative Approaches

### 3.1 Option A: Build Custom DTOs for All Languages

**Description:** Define all OpenAI-compatible types natively in each SDK, matching the JS/TS and C++ approach.

**Pros:**
- ✅ Zero external dependency — complete control over types, serialization, and versioning
- ✅ Can precisely match what Foundry-Local supports (no unused fields or unsupported features)
- ✅ Foundry-specific extensions (LARK grammar, metadata, top_k, random_seed) are first-class citizens
- ✅ No risk of upstream breaking changes
- ✅ Consistent approach across all languages

**Cons:**
- ❌ **Significant upfront effort** — Need to define, test, and maintain ~25-50 types per language for C# and Rust (Python and JS already have patterns)
- ❌ **Ongoing maintenance** — As the OpenAI API evolves, types must be manually updated
- ❌ **Risk of spec drift** — Custom types may diverge from the OpenAI spec over time if not carefully maintained
- ❌ **Developer experience** — Users familiar with official OpenAI SDK types would need to learn Foundry-Local's variants

**Effort Estimate:** Medium-High. C# and Rust require the most work. JS and C++ are already done. Python would need to be reworked.

### 3.2 Option B: Auto-Generate Types from OpenAI OpenAPI Specification

**Description:** Use the [OpenAI OpenAPI specification](https://github.com/openai/openai-openapi) to auto-generate type definitions for each language using code generation tools (NSwag for C#, openapi-generator for Rust/Python/JS).

**Pros:**
- ✅ Single source of truth — OpenAI's own API specification
- ✅ Automated — reduces manual maintenance burden
- ✅ Can generate types-only (no HTTP client) with most generators
- ✅ Consistent type definitions across languages
- ✅ Easy to update — re-run generator when spec changes

**Cons:**
- ❌ **Generated code quality** — Auto-generated code often needs cleanup and may not match idiomatic patterns for each language
- ❌ **Spec may include types Foundry-Local doesn't support** — Would need filtering/subsetting
- ❌ **Foundry-specific extensions** — Would need manual additions on top of generated types
- ❌ **Generator maintenance** — Each language needs a different generator with specific configurations
- ❌ **Build complexity** — Adds a code generation step to the build pipeline

**Effort Estimate:** Medium. Initial setup is significant, but ongoing maintenance is lower.

### 3.3 Option C: Adopt Official OpenAI Packages Where Available (Hybrid)

**Description:** Use official OpenAI packages for type imports where they exist and are well-suited. Use custom types for languages without official packages or where official packages are poorly suited.

| SDK | Approach |
|-----|----------|
| **Python** | Keep official `openai` package (current approach) |
| **C#** | Migrate from `Betalgo.Ranul.OpenAI` to official `OpenAI` NuGet package |
| **JS/TS** | Keep custom types (already working well) |
| **Rust** | Keep `async-openai` types-only dependency OR build custom types |
| **C++** | Keep custom types (already working well) |

**Pros:**
- ✅ Reduces 3rd-party risk for C# (biggest concern area)
- ✅ Python is already validated
- ✅ Minimal disruption to JS and C++ (no changes)
- ✅ Official packages are well-maintained and less likely to disappear

**Cons:**
- ❌ **Inconsistency** — Different languages use different strategies
- ❌ **C# migration complexity** — All public API signatures change; breaking change for consumers
- ❌ **Official C# SDK types may not be easily extensible** for Foundry-specific features
- ❌ **Rust still relies on 3rd-party** (no official option available)

**Effort Estimate:** Low-Medium. Main effort is C# migration.

### 3.4 Option D: Types-Only Internal Package (Custom DTOs in a Shared Spec)

**Description:** Create a lightweight, internal types-only package for each language that defines the OpenAI-compatible DTOs Foundry-Local supports. These packages would:
1. Be maintained in the Foundry-Local repo
2. Define only the types relevant to Foundry-Local's supported API surface
3. Include Foundry-specific extensions as first-class types
4. Be versioned independently from the SDK

**Pros:**
- ✅ Complete control — no external dependency
- ✅ Foundry-specific extensions built-in from the start
- ✅ Can support only the subset of OpenAI API that Foundry-Local implements
- ✅ Clear, explicit contract about what's supported
- ✅ Types can be tested against the native core's expectations

**Cons:**
- ❌ Most effort upfront — requires defining types for C#, Rust, and potentially Python
- ❌ Ongoing maintenance when OpenAI adds features Foundry-Local wants to support
- ❌ May confuse developers who expect standard OpenAI types

---

## 4. Risk Assessment Matrix

| Risk | Option A (Custom) | Option B (AutoGen) | Option C (Hybrid/Official) | Option D (Internal Pkg) |
|------|-------------------|--------------------|---------------------------|------------------------|
| 3rd-party abandonment | ✅ None | ✅ None | ⚠️ Reduced (C# official, Rust still 3rd-party) | ✅ None |
| Upstream breaking changes | ✅ None | ⚠️ Spec changes | ⚠️ Official pkg changes | ✅ None |
| Maintenance burden | ⚠️ High | ⚠️ Medium | ✅ Low | ⚠️ High |
| Developer familiarity | ⚠️ Custom types | ⚠️ Generated types | ✅ Standard types | ⚠️ Custom types |
| Foundry extensions | ✅ Native | ⚠️ Manual layer | ⚠️ Extension layer | ✅ Native |
| Build complexity | ✅ Simple | ❌ Code gen step | ✅ Simple | ✅ Simple |
| API surface accuracy | ⚠️ May drift | ✅ Spec-aligned | ✅ Spec-aligned | ⚠️ May drift |
| Initial effort | ❌ High | ⚠️ Medium | ✅ Low | ❌ High |

---

## 5. Foundry-Local Specific Considerations

### 5.1 The SDK Doesn't Make HTTP Calls

A critical difference between Foundry-Local and typical OpenAI SDK usage: **Foundry-Local SDKs do not make HTTP calls to an OpenAI endpoint**. They communicate with a native core library via FFI (Foreign Function Interface). The flow is:

```
User Code → SDK (serialize to JSON) → Native Core (FFI) → Local Model Inference → Native Core → SDK (deserialize JSON) → User Code
```

This means:
1. **The HTTP client in official SDKs is completely unused** — adding a dependency on a package that includes an HTTP client is wasteful
2. **Only the type definitions matter** — we need POCOs/structs/dataclasses for serialization/deserialization
3. **Serialization must be compatible** with what the native core expects, which follows the OpenAI specification but may have subtle differences

### 5.2 Foundry-Local Supports a Subset of OpenAI API

Foundry-Local currently supports:
- Chat completions (with streaming)
- Audio transcription (with streaming)
- Live audio transcription
- Tool calling / function calling
- Structured outputs (JSON schema)
- LARK grammar (Foundry-specific extension)
- Responses API (JS SDK only)

The official OpenAI packages cover **far more** — embeddings, image generation, fine-tuning, assistants, vector stores, batching, etc. Taking a dependency on the full official package means:
- Larger package size
- Users may try unsupported features
- Confusion about what's available locally vs. cloud-only

### 5.3 Foundry-Specific Extensions

The SDK needs to support non-standard extensions:
- **LARK grammar** in `response_format` (C#, Rust)
- **`top_k` and `random_seed`** via metadata (all SDKs)
- **Transcription-specific types** not in OpenAI spec (audio streaming, live transcription)
- **Custom error responses** from the native core

These extensions mean the SDK can never be a pure passthrough of official types — there will always be a wrapper or extension layer.

---

## 6. Recommendation

### Primary Recommendation: Hybrid Approach (Option C) with Targeted Custom Types

**Short-term (next release):**

1. **C# — Migrate from Betalgo to official `OpenAI` NuGet package**
   - This addresses the primary partner concern about 3rd-party risk
   - The official C# SDK (`OpenAI` v2.x) supports custom endpoints and has well-defined types
   - Import only the type namespaces needed (`OpenAI.Chat`, etc.)
   - Define Foundry-specific wrapper types (LARK grammar, metadata extensions) as local classes
   - **Risk mitigation:** Prototype first to validate that the official types serialize/deserialize correctly with the native core's expectations
   - **Breaking change impact:** All public API signatures will change (e.g., `ChatMessage` → different namespace/type). This requires a major version bump.

2. **Rust — Keep `async-openai` types-only dependency for now**
   - No official Rust SDK exists
   - `async-openai` is actively maintained (v0.34.0, April 2026) with clean feature flags
   - The `chat-completion-types` feature flag ensures minimal dependency footprint
   - **Monitor:** If `async-openai` maintenance declines, switch to custom types (relatively easy since only 25 types are used)

3. **Python — Keep official `openai` package (no change)**
   - Already working and validated
   - Consider extracting type-only imports to minimize coupling surface

4. **JS/TS — Keep custom types (no change)**
   - The self-contained approach is working well
   - No benefit to adding the `openai` npm package given the existing custom type system

5. **C++ — Keep custom types (no change)**
   - No official package available; custom types are well-suited

**Medium-term (6-12 months):**

6. **Evaluate Option D (Internal Types Package)** for C# and Rust once the team has bandwidth
   - This provides the strongest long-term independence
   - Can be informed by the OpenAI OpenAPI spec to stay aligned
   - Use the current C++/JS custom types as a template

7. **Consider auto-generation (Option B) as a tooling investment**
   - Set up a pipeline that generates types from the OpenAI spec
   - Use as a reference/validation tool, not as the direct source of SDK types
   - Helps catch spec drift and ensures compatibility

### Decision Matrix by Language

| Language | Recommended Action | Timeline | Breaking Change? |
|----------|-------------------|----------|-----------------|
| **C#** | Migrate to official `OpenAI` NuGet package | Next release | Yes — major version bump |
| **Rust** | Keep `async-openai` (types-only); plan migration to custom types | Keep for now; re-evaluate in 6 months | No (for now) |
| **Python** | No change (already using official `openai`) | N/A | No |
| **JS/TS** | No change (custom types working well) | N/A | No |
| **C++** | No change (custom types working well) | N/A | No |

---

## 7. C# Migration Plan (Highest Priority)

### 7.1 Scope

Replace `Betalgo.Ranul.OpenAI` with the official `OpenAI` NuGet package (v2.x).

### 7.2 Steps

1. **Prototype / Spike**
   - Add `OpenAI` NuGet package reference alongside `Betalgo.Ranul.OpenAI`
   - Create a test branch that maps Foundry-Local's usage to the official types
   - Validate JSON serialization/deserialization round-trips with the native core
   - Verify that tool calling, streaming, and audio types work correctly

2. **Type Mapping**

   | Current (Betalgo) | Replacement (Official OpenAI) |
   |-------------------|-------------------------------|
   | `ChatMessage` | `ChatCompletionMessageParam` variants (`SystemChatMessage`, `UserChatMessage`, `AssistantChatMessage`, `ToolChatMessage`) |
   | `ChatCompletionCreateRequest` | `ChatCompletionOptions` |
   | `ChatCompletionCreateResponse` | `ChatCompletion` |
   | `ToolDefinition` | `ChatTool` |
   | `ToolChoice` | `ChatToolChoice` |
   | `ResponseFormat` | `ChatResponseFormat` |
   | `AudioCreateTranscriptionRequest` | `AudioTranscriptionOptions` |
   | `AudioCreateTranscriptionResponse` | `AudioTranscription` |

3. **Handle Foundry Extensions**
   - Create `FoundryResponseFormat : ChatResponseFormat` (or wrapper) for LARK grammar
   - Create metadata wrapper for `top_k` / `random_seed`
   - Validate that inheritance/extension patterns work with official types

4. **Update JSON Serialization Context**
   - Modify `JsonSerializationContext.cs` to reference official types
   - Ensure source-generated serialization is compatible

5. **Update Public API**
   - Update all method signatures in `OpenAIChatClient`, `OpenAIAudioClient`
   - Update samples and tests
   - Document the breaking change

6. **Remove Betalgo Dependency**
   - Remove `Betalgo.Ranul.OpenAI` from `.csproj`
   - Clean up using statements

### 7.3 Risk Mitigations

- **Serialization incompatibility:** Test every request/response path with the native core before merging
- **Type extensibility:** If official types are sealed/non-extensible, use composition (wrapper classes) instead of inheritance
- **Performance:** Benchmark serialization performance (official SDK may use different JSON strategies)

---

## 8. Summary Table

| Criteria | Custom DTOs | Auto-Generated | Official Packages | Current (3rd-party) |
|----------|------------|----------------|-------------------|-------------------|
| **3rd-party risk** | ✅ Eliminated | ✅ Eliminated | ✅ Minimal (official) | ❌ High |
| **Maintenance cost** | ❌ High | ⚠️ Medium | ✅ Low | ⚠️ Medium |
| **Foundry extensions** | ✅ Native | ⚠️ Manual layer | ⚠️ Extension layer | ⚠️ Extension layer |
| **Developer experience** | ⚠️ Non-standard | ⚠️ Generated code | ✅ Familiar types | ⚠️ Varies |
| **Consistency across SDKs** | ✅ Possible | ✅ Possible | ❌ Not available for all languages | ❌ Inconsistent |
| **Time to implement** | ❌ High (C#/Rust) | ⚠️ Medium | ✅ Low (C# only) | ✅ Already done |
| **Long-term sustainability** | ✅ Best | ✅ Good | ✅ Good (where available) | ❌ Uncertain |

---

## 9. Appendix: Official OpenAI Package Details

### A. OpenAI C# NuGet Package (`OpenAI` v2.10.0)
- **Repository:** https://github.com/openai/openai-dotnet
- **NuGet:** https://www.nuget.org/packages/OpenAI/
- **License:** MIT
- **Target:** .NET Standard 2.0, .NET 8.0+
- **Dependencies:** `System.ClientModel`, `System.Text.Json`, `Microsoft.Bcl.AsyncInterfaces`

### B. Azure.AI.OpenAI NuGet Package (v2.1.0)
- **Repository:** https://github.com/Azure/azure-sdk-for-net
- **NuGet:** https://www.nuget.org/packages/Azure.AI.OpenAI/
- **Note:** Companion package to the official OpenAI SDK; adds Azure-specific authentication and features
- **Relevance:** Low for Foundry-Local (not using Azure endpoints)

### C. OpenAI Python Package (`openai` v2.31.0)
- **Repository:** https://github.com/openai/openai-python
- **PyPI:** https://pypi.org/project/openai/
- **License:** Apache 2.0
- **Dependencies:** `httpx`, `pydantic`, `typing_extensions`, `distro`, `sniffio`, `anyio`

### D. OpenAI JavaScript Package (`openai` v6.33.0)
- **Repository:** https://github.com/openai/openai-node
- **npm:** https://www.npmjs.com/package/openai
- **License:** Apache 2.0

### E. Rust — No Official Package
- **Community option:** `async-openai` v0.34.0 (https://github.com/64bit/async-openai)
- **License:** MIT

### F. C++ — No Official Package
- **No community alternatives widely adopted for types-only usage**

### G. OpenAI OpenAPI Specification
- **Repository:** https://github.com/openai/openai-openapi
- **Format:** YAML (OpenAPI 3.x)
- **Use case:** Source of truth for auto-generating types
