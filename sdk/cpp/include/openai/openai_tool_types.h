// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

namespace FoundryLocal {

    /// JSON Schema property definition used to describe tool function parameters.
    struct PropertyDefinition {
        std::string type;
        std::optional<std::string> description;
        std::optional<std::unordered_map<std::string, PropertyDefinition>> properties;
        std::optional<std::vector<std::string>> required;
    };

    /// Describes a function that a model may call.
    struct FunctionDefinition {
        std::string name;
        std::optional<std::string> description;
        std::optional<PropertyDefinition> parameters;
    };

    /// A tool definition following the OpenAI tool calling spec.
    struct ToolDefinition {
        std::string type = "function";
        FunctionDefinition function;
    };

    /// A parsed function call returned by the model.
    struct FunctionCall {
        std::string name;
        std::string arguments;  ///< JSON string of the arguments
    };

    /// A tool call returned by the model in a chat completion response.
    struct ToolCall {
        std::string id;
        std::string type;
        std::optional<FunctionCall> function_call;
    };

    /// Controls whether and how the model calls tools.
    enum class ToolChoiceKind {
        Auto,
        None,
        Required
    };

} // namespace FoundryLocal
