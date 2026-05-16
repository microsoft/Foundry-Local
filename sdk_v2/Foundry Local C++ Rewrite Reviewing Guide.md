# Foundry Local C++ Rewrite Reviewing Guide

## Background

- The **old setup** lives in `sdk/` (C#, Python, JS, Rust). All of those SDKs were thin
  wrappers over an **AOT-compiled C# native library** (`Foundry.Local.Core`, source in
  the `dev/FoundryLocalCore/main` branch of the `neutron` repo). Communication with that native library was via a
  **JSON-based request/response interface with a limited surface area** — each SDK had
  to parse JSON, maintain its own catalog representation, and largely duplicate the
  same higher-level logic.
- The **new setup** lives in `sdk_v2/` (C++, C#, Python). The new C++ SDK
  (`sdk_v2/cpp/`) **replaces the AOT-compiled C# native library** at the lower level
  and exposes a **comprehensive C ABI** plus a header-only C++ wrapper. The C# and
  Python SDKs in `sdk_v2/` are thin bindings over that C ABI, with shared logic
  (catalog parsing, session management, item types) pushed down into C++ instead of
  duplicated per language. JS and Rust are not yet ported to v2.

## General Tips

- Checkout the branch locally and use AI for any questions
  - you don't need to try and absorb the entire setup into memory as that would be near impossible
  - Ask it to summarize the structure of the code or a specific part of it
  - Ask it to explain how/where something is used
  - Ask it if a potential alternative way of doing things would be better
  - Ask it if a particular edge case you can think of is handled or not
  - "Would it be better if X used Y approach instead of the current implementation?"
  - "Could/should we decouple X from Y?"
  - "X looks fragile. Is that the case? If so what can we do about it?"
  - etc. etc.  

- There are AI planning and design docs for various areas in sdk_v2/cpp/docs
  - Overall mapping from original C# implementations (Core and C# SDK) to the new C++ implementations is in sdk_v2/cpp/docs/CppPortGuide.md
  - Note: These captured a moment in time when the set of changes was being planned and implemented. Things have naturally evolved since then, so whilst they provide background and rationale for various decisions, they are not expected to be a 100% accurate representation of the current code. Ask AI to evaluate current implementation first if you have queries about why something is the way it is or if it could be done differently.

- AI has reviewed all implementations multiple times
  - reviewed during development at various points with Opus 4.6
  - reviewed with Copilot `/review` command this week with Opus 4.6 and 4.7
    - see 'docs' folder for each language implementation for the 'copilot*review-report.md' files with the results of those reviews
    - all items were addressed
  - given that there should be very few low level issues

- Have run memory tests on linux with sdk_v2/cpp/scripts/run_sanitizer_tests.py and fixed all issues found there
  - no known memory issues from new code

## Focus Areas
- higher level design and architecture questions
  - Are the APIs designed in a way that will allow us to easily add features in the future?
  - Are there any potential performance bottlenecks or memory issues?
  - Is the code organized in a way that is easy to understand and maintain?
- functionality gaps
  - are we missing features from the existing implementation, esp. recently added ones?
- check what the integration tests do and don't cover as these should be comprehensive
  - are there gaps or edge cases that aren't being tested?
- areas you have specific knowledge of
  - if you have implemented something previously make sure the new implementation is correct

## C++ implementation

### Overall Setup

- ABI C API
  - sdk_v2\cpp\include\foundry_local\foundry_local_c.h
  - API is broken up into some sub APIs for better grouping and long term growth
    - Catalog, Configuration, Item, Inference, Model
  - Some versioned data structs for input/output data
  - Other types are opaque pointers to internal C++ classes

- C++ wrapper around C API
  - sdk_v2\cpp\include\foundry_local\foundry_local_cpp.h
  - header-only
  - Provides more user-friendly C++ interface for users of the SDK
    - but as it's wrapping the C API there are some slight differences to how a pure C++ API would typically look
      - tried to minimize these as much as possible so that the usage feels natural in C++
  - Handles memory management and conversions between C and C++ types

- Supports all existing usage
  - Direct usage with OpenAI request/response json
  - OpenAI web service
    - chat completions, Responses API, transcriptions, embeddings

- Catalog logic moves down from the various SDKs to the C++ layer
  - previous: SDK got catalog as JSON, parsed and maintained its own in-memory representation
  - new: C++ layer has in-memory representation and SDKs query that directly

- New Item/Session/Request/Response based API that is hopefully more flexible
  - Should support all model types including predictive
  - Session contains the state
    - generator, history it we need to recreate a generator
    - can specify tools/settings at the session level
      - per-request will override session level
    - can rewind
    - Session caching for Responses API
        - LRU eviction
  - Create Item/s for input, get Item/s as output
    - TextItem, MessageItem, ToolCallItem, AudioItem, etc.
    - MessageItem has 'parts' so can handle represent multi-modal inputs/outputs
    - ItemQueue is used for streaming (input and output)
      - queue with synchronization built in
      - push/pop items as they are generated/processed

### Building and testing

- `sdk_v2/cpp> ./build.bat` will build RelWithDebInfo and run tests.
  - the C# and python tests will use the developer build by default for local testing
- point it to test-data-shared if you have that to avoid some model downloads
    - set the `TEST_MODEL_CACHE_DIR` environment variable to the full path of your test-data-shared directory
      - all new SDKs will check this value for consistency
- tests are split into unit tests (sdk_v2/cpp/test/internal_api) and integration tests (sdk_v2/cpp/test/sdk_api)
  - unit tests are low level and need minimal review
  - integration tests use the real catalog and real models and should cover all scenarios we care about
    - we need to ensure these are comprehensive for the use cases we support
  - integration tests are setup to run suite-by-suite
    - limits loaded models to just those required by the suite
    - suites are separated by feature
      - e.g. chat, vision, audio, etc.
    - downside is you don't get per-test output when running via build.py
      - can run sdk_integration_tests[.exe] directly to get that output when needed

### Key review areas

- Ensure we don't box ourselves in anywhere with the new API
  - Are the Item types flexible enough?
    - TextItem has a type field
    - MessageItem has parts
    - I _think_ that handles reasoning and multimodal models. 
      - is that correct? 
      - are there other scenarios that need more?
  - Are the Item types comprehensive enough? Too specific or too generic?
- Is the GenAI usage correct (onnx_chat_generator.cc)
  - Is the generator caching in a session correct (chat_session.cc)
- Is the tool calling handling correct?
- Is the reasoning / chain-of-thought handling correct?
- Are recent changes to FL Core or the C# SDK reflected in the new C++ implementation?

### Code Coverage

Currently > 80% when unit tests and integration tests are run together. 
You can run the coverage script locally to see which lines aren't being hit by tests:
`sdk_v2/cpp/run_coverage.ps1`

Note: Uses a Debug build by default for accuracy so you'll need to run `./build.bat --Config Debug --skip_tests` first. Can also use a `RelWithDebInfo` build but line annotations will be less accurate.

## C# and Python SDK implementation

Both should be similar in review needs
- Should have same API as the current SDK and be backwards compatible.
- Adds the new Item/Session/Request/Response based setup to that.

### Key review areas
- Wiring up of the far more extensive C API
- Memory management of native interop
- API Compatibility with existing C# SDK
  - Are there any breaking changes that would affect existing users?
- Is the new API intuitive and easy to use for C# developers?
  - Item types/Session types/Request/Response
- Loading of ORT and ORT GenAI libraries

#### C# SDK specific

Native bindings
- sdk_v2\cs\src\Detail\NativeMethods.cs
  - low level mapping to C API
- sdk_v2\cs\src\Detail\FoundryLocalApi.cs
  - higher level wrapper around native methods, handles memory management and conversions
  - types with ownership are IDisposable

Should we be using SafeHandle?
  - Considered and decided against. Two reasons:
    1. The biggest ergonomic win of `SafeHandle` is automatic add-ref around
       `[LibraryImport]`/`[DllImport]` boundaries. Our calls don't go through
       those — they go through delegates retrieved at runtime from the vtable
       returned by `FoundryLocalGetApi`. The marshaller never sees our handles,
       so the integration benefit doesn't apply.
    2. The vtable shape (separate area-specific vtables under `FlApi`) is a
       deliberate ABI choice for long-term growth, so we won't be moving to flat
       `[LibraryImport]` entrypoints in the future either.
  - The one real benefit `SafeHandle` would have given us — finalizer-backed leak
    protection if a consumer forgets `Dispose()` — has been addressed by adding
    explicit finalizers to the five native-handle owners:
    `Detail.Native.Configuration`, `Detail.Native.Manager`, `Detail.Native.ModelList`,
    `Detail.Native.Session`, and `Items.Item`. They share a tiny
    `Api.FinalizeRelease(IntPtr, Action<IntPtr>)` helper that no-ops on runtime
    shutdown and swallows exceptions (finalizers must not throw).

#### Python SDK specific

Native bindings
- Use cffi in ABI mode
  - pre-compiled for performance
  - compiled against C API header to ensure compatibility
  - adds some complexity to packaging setup 
- See src/foundry_local_sdk/_native

## CI and Packaging infrastructure

Currently running in parallel to existing SDK
Setup is under .pipelines/sdk_v2
Produces packages for C++, C# and python
- packages have 'sdk2' in the name to differentiate from existing ones for now
  - that will go away when we're ready to switch over to the new setup