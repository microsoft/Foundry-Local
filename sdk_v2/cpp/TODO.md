TODO:

- ~~create static catalog implementation and test~~
EP as runtime
- ~~check Linux builds~~
- ~~check macOS builds~~
- check Android builds
- add CI to GH build

- ~~limit model download in CI~~
- add new things to C# SDK (e.g. embeddings)
- check C# SDK for breaking changes and fix
- add extra model info types for parity with existing C++ SDK (e.g. Runtime)

- wire up GenAI logging or figure out where it's going
  - maybe set log level to info in ORT Env and see what happens (a la OrtEnvHelper in FLC)

- documentation

- validate vision model generator cache usage is correct
  - not sure if this was hallucinated
  - saves output from image input but resets generator
  - follow up turns use that state but lose the image-derived tokens

- address sanitizer build (and maybe binskim flags as well)

- investigate chat session issue with token with qwen3
  - need to not use the qwen2.5 model from test-shared-data


==========
Main review areas

- Ensure we don't box ourselves in anywhere with the new API
  - Are the Item types flexible enough?
    - TextItem has a type field
    - MessageItem has parts
    - I _think_ that handles reasoning and multimodal models. is that correct? are there other scenarios that need more?
  - Are the Item types comprehensive enough?

- Is the generator caching in a session correct (chat_session.cc)
- Is the GenAI usage correct (onnx_chat_generator.cc)
- Is the tool calling handling correct?
- Is the reasoning / chain-of-thought handling correct?

- Describe current setup
  - build C++ locally
  - C# references Debug output from that
  - Python references???
  - CI that can be used to create nuget package is...
