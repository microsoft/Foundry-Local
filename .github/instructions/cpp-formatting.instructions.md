---
description: "Use when writing or modifying C++ source/header files. Covers vertical whitespace conventions for readability."
applyTo: "sdk_v2/cpp/**/*.{h,hpp,cc,cpp}"
---
# C++ Formatting

## Line length

120 chars is the allowed line length limit.
readability is more important than strictly adhering to the limit.
use judgment and consider readability when deciding where to break.

The 120-char limit applies to **comments** as well as code. Do not wrap comments at 80 chars — use the full 120 so prose stays compact and easy to scan. Block comments that explain non-obvious behavior should fill the available width rather than producing tall narrow paragraphs.

```cpp
// Good — comment uses the full 120-char width.
// Reasoning models emit <think>...</think> blocks. Continuous decoding leaves prior <think> tokens in the KV cache,
// which confuses the model on subsequent turns (it produces unterminated reasoning that never closes).

// Bad — wrapped at 80 chars, producing extra lines for no reason.
// Reasoning models emit <think>...</think> blocks. Continuous decoding
// leaves prior <think> tokens in the KV cache, which confuses the model
// on subsequent turns (it produces unterminated reasoning that never
// closes).
```

Prefer keeping function-call-style invocations on a single line when they fit within the line-length limit, even if they have multiple arguments. In particular, `FL_THROW(error_code, "message")` should be on one line whenever the result fits — break only when the call genuinely overflows.

```cpp
// Good — fits on one line.
FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, "MessageItem requires non-empty text");

// Good — too long for one line, broken at the argument boundary.
FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT,
         "flMessageData.content_items entry must be TEXT, IMAGE, or AUDIO");

// Bad — fits on one line; do not break.
FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_USAGE,
         "MessageItem is not a single TextItem");
```

## Vertical Whitespace

Leave a single blank line between unrelated blocks of code so individual parts are easy to scan. In particular:

- After the closing `}` of a `for`, `while`, `do`, or `switch` block, when more code follows in the same scope.
- After the final `}` of an `if` / `else if` / `else` chain, when more code follows in the same scope.
- After the closing `}` of a lambda whose definition spans multiple lines, when more code follows in the same scope.
- Between a multi-line variable declaration / initialization and the next statement that uses it for a distinct purpose.
- Between logical phases inside a function (e.g., validation, setup, work, teardown).

Do not insert a blank line:

- Immediately before a closing `}` (no trailing blank lines inside a block).
- Between tightly-coupled statements that form one conceptual step (e.g., a member assignment followed by its corresponding `reserve` / `clear`).
- Between consecutive single-line guard `if` statements at the top of a function.

### Examples

```cpp
// Good — blank line after the for, before the unrelated assignment.
for (const auto& part : other.content) {
  if (!part.view) {
    continue;
  }
  content.push_back(MessagePart::Own(CloneApiPart(*part.view)));
}

api_part_ptrs_.clear();
return *this;
```

```cpp
// Good — blank line after the if/else chain.
if (img.data && img.data_size > 0) {
  // ... owning path
  return clone;
} else {
  // ... borrowed path
  return clone;
}

FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, "...");
```

```cpp
// Bad — for and the next statement run together.
for (const auto& part : other.content) {
  content.push_back(MessagePart::Own(CloneApiPart(*part.view)));
}
api_part_ptrs_.clear();
```
