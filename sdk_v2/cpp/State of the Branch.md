# State of the branch #

## Working ##
- Catalog
  - Azure catalog integration
  - Local model lookup
- EP Download/Registration/Detection
- Model handling
  - download/load/unload/remove from cache
  - local model scanner resolves cached models
- Session
  - Chat session
    - w/ and w/o streaming
  - Audio session
    - transcription from file, w/ and w/o streaming
    - realtime streaming audio (push PCM chunks via ItemQueue)
  - Embeddings session
    - L2-normalized output vectors
    - web service endpoint (OAI-compatible /v1/embeddings)
  - Tool calling
  - Caching
    - generator re-used in Session for continuous decoding
    - session caching in Responses API 
- Web service
  - Same endpoints as FL Core for models
  - OAI Chat Completions endpoint
  - OAI Response API endpoint
    - session caching (heavy due to generator caching but provides continuous decoding. small cache size) 
    - responses caching (lightweight. can create new session with history. larger cache size)
  - Audio transcription endpoint
  - Shutdown endpoint for CLI usage
- OpenAI chat/audio request/response format support for direct SDK usage for backwards compatibility
- Testing
  - 696 unit tests (internal)
    - plus 97 SDK API-level integration tests including web service (separate binary)
  - 82% code coverage (6246/7638 lines)
- Build/CI
  - builds on Windows, Linux and macOS
  - initial OneBuild CI setup for Windows and Linux (build + test)
  - creates nuget package from CI builds (win-x64, win-arm64, linux-x64 currently)
- C# SDK on branch
  - uses new bindings
  - provides same API as before
  - extended with new Item/Session API for direct usage
  - all existing C# SDK tests pass

## Stubbed usage ##
- Telemetry
  - code is in-place using ITelemetry interface with placeholder implementation
  - using TelemetryLogger for telemetry
- ~~EP detector~~
  - ~~using dummy EP detector that returns CPU EP~~
  - done — real EP detection, bootstrapping, download/registration, C ABI, C# bindings, and ModelLoadManager EP guard all implemented. 
    See `docs/EpDetectionPlan.md`.

## TODO ##
- ~~Audio support~~
  - ~~create AudioSession~~ done
  - ~~add transcription from file support~~ done
  - ~~add realtime support~~ done
    - ~~handle input of ItemQueue for streamed bytes~~ done
    - add WebSockets usage in oat++ for the streaming endpoint (deferred — not needed for direct SDK usage)

- Tool Calling validation
  - ~~ported implementation is there but largely unreviewed and untested~~
  - ~~code validation and~~ add e2e tests
  - Sayan is starting on this

- OAI Responses API validation
  - ~~make sure we are hitting in the E2E tests~~ done
  - make sure the implementation is correct and there are meaningful E2E tests covering it
    - we have E2E tests but they could be more comprehensive 
    - ~~should be able to fill gaps from the FL Core tests that were added as they have additional checks around the~~
      ~~content of the responses, not just that we get a response.~~ done

- Session caching
  - cache sessions for continuous decoding and responses API usage
    - ~~session has conversation history and generator~~
    - ~~user keeps the Session valid for direct requests so we shouldn't need to cache those internally~~
    - ~~match session using id from Responses API web request~~
    - ~~match session using input message hash for Chat Completions API  web request~~
      - SKIP. Continuous decoding for chat completions was only supported in Neutron not FL Core.
        SDK users own the Session lifetime for direct usage (session has generator) or Responses API for web usage

- Test Setup
  - review existing tests and check they are validating the correct behavior
  - ~~add usage of shared_test_data for test assets~~ done
    - ~~enables testing with real models in CI without model download~~
    - ~~use C# FL Core and C# SDK tests as examples~~
    - supported via the TEST_MODEL_CACHE_DIR environment variable 
  - ~~address any code coverage gaps in important code~~ done
  - coverage tooling: OpenCppCoverage with run_coverage.ps1/parse_coverage.ps1
  - validate no memory leaks
  - stress testing
    - make sure threading and memory management is solid under load

- Address the 'TODO:' comments in the codebase
  - ~~reminders to review and validate certain assumptions or patterns or gaps to fix that I've noticed~~
  - remaining ones are around tool calling

- Code Quality and Correctness
  - ~~review all AI-generated code for quality and correctness issues~~ done
  - ~~refactoring for clarity/readability~~ done
  - ~~refactor to reduce code duplication~~ done
  - stability/robustness
    - ~~C API functions need try/catch wrapper so it's guaranteed not to leak exceptions~~ done
      - ~~see ORT's API_IMPL_BEGIN/END macros for example of how to do this~~
  - documentation of all public APIs and complex internal functions
    - must be able to generate API documentation using doxygen or similar tool

- Backwards compatibility
  - ~~support current FL SDK usage of using OAI chat request/response format for direct requests~~
    - ~~move logic to create/return this format from the OAI Chat Completions web service handler to a shared location~~
    - ~~update ChatSession to use OAI chat completions json request/response code if the input is a JsonItem~~
    - ~~update web service chat completions handler to use JsonItem for session input~~
  - ~~support current FL SDK usage of OAI audio request/response format~~

- Cross-platform builds
  - ~~build and fix errors on Linux, macOS and Android~~
      - ~~Android specific logic migrated from FL Core.~~
      - Have Windows, Linux, macOS and Android builds.

- CIs
  - Setup CIs for all supported platforms
    - build, test, code coverage, static analysis, etc.
  - Setup triggers for running CIs for PRs
  - Setup packaging CIs

- Packaging
  - figure out the packaging story
    - Native NuGet package with builds for Windows/Linux/macOS/Android?
    - Platform specific packages?    

- ~~Migrate AI agent setup/info to something github/copilot friendly~~
  - ~~used Squad (https://github.com/bradygaster/squad) but that's very hit-and-miss from VS Code~~
  - ~~attempt to migrate the content for the agents/history/skills/etc to a GH based setup~~

- Miscellaneous
  - ~~add mode or make it the default to use cached model info at startup unless expired or new local models are found~~
    - ~~will be required for CLI so we don't hit the catalog for every single command~~
    - **Addressed by** `external_service_url` — when set, catalog reads only from `foundry.modelinfo.json` cache
  - ~~CLI support requires a long running instance~~
    - add static asserts to ensure API structs only ever grow sequentially a la ORT
    - ~~**Decided:** Add `external_service_url` to `Configuration`. When set:~~
      ~~- Catalog switches to cache-only reads (`foundry.modelinfo.json`, no network fetch)~~
      ~~- `StartWebService()` errors (service is external)~~
      ~~- Session/Client creation errors (no local model to bind to)~~
      ~~- C ABI exposes `FlConfigurationApi::SetExternalServiceUrl` (setter only, no getter)~~
    - TBD whether SDK layers (C#, Python, etc.) should add HTTP routing for model load/unload or leave that to user code
      - depends on whether replicating ModelLoadManager logic in each language binding is worthwhile
        - the scenario may be too niche and can be handled where needed (e.g. FL CLI adds this logic)
          - if we think it's going to be niche we may need to structure the C# SDK so that it's easy for a user to
            plugin the model load manager implementation. 
    - TBD if OAI-compatible endpoints on the external service are sufficient for all inference scenarios
