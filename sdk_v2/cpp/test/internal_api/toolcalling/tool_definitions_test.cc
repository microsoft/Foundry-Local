// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Tests for the shared wire-to-core tool definition layer (contracts/tool_definitions.*) and for
// the property that matters most about it: every surface — the C ABI, Chat Completions and
// Responses — turns the same declared tool inventory into the same core definitions.
//
#include "contracts/tool_definitions.h"

#include "contracts/chat_completions.h"
#include "contracts/chat_completions_converter.h"
#include "exception.h"
#include "inferencing/generative/openresponses/response_converter.h"
#include "inferencing/session/tool_registry.h"
#include "internal_api/toolcalling/coding_agent_tools_fixture.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace fl;
using json = nlohmann::json;

namespace {

#define EXPECT_INVALID_ARGUMENT(expr)                                    \
  try {                                                                  \
    expr;                                                                \
    ADD_FAILURE() << "expected fl::Exception from: " #expr;              \
  } catch (const fl::Exception& ex) {                                    \
    EXPECT_EQ(ex.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT) << ex.what(); \
  }

ChatCompletionRequest ChatRequestWithTools(const std::string& tools_array_json) {
  auto j = json::parse(R"({"model": "m", "messages": [{"role": "user", "content": "hi"}]})");
  j["tools"] = json::parse(tools_array_json);
  return j.get<ChatCompletionRequest>();
}

responses::ResponseCreateParams ResponsesParamsWithTools(const json& tools_array) {
  json j{{"model", "m"}, {"input", "hi"}};
  j["tools"] = tools_array;
  return j.get<responses::ResponseCreateParams>();
}

std::vector<std::string> NamesOf(const std::vector<ToolDefinition>& definitions) {
  std::vector<std::string> names;
  names.reserve(definitions.size());
  for (const auto& definition : definitions) {
    names.push_back(definition.name);
  }
  return names;
}

}  // namespace

// ========================================================================
// ValidateCustomToolFormat — text is the only shape the runtime can honour
// ========================================================================

TEST(CustomToolFormatTest, AcceptsAbsentFormat) {
  EXPECT_NO_THROW(tools::ValidateCustomToolFormat(json(), "apply_patch"));
}

TEST(CustomToolFormatTest, AcceptsTextFormat) {
  EXPECT_NO_THROW(tools::ValidateCustomToolFormat(json::parse(R"({"type":"text"})"), "apply_patch"));
}

TEST(CustomToolFormatTest, RejectsGrammarFormat) {
  EXPECT_INVALID_ARGUMENT(tools::ValidateCustomToolFormat(
      json::parse(R"({"type":"grammar","syntax":"lark","definition":"start: X"})"), "apply_patch"));
}

TEST(CustomToolFormatTest, RejectsUnknownFormatType) {
  EXPECT_INVALID_ARGUMENT(tools::ValidateCustomToolFormat(json::parse(R"({"type":"binary"})"), "apply_patch"));
}

TEST(CustomToolFormatTest, RejectsNonObjectFormat) {
  EXPECT_INVALID_ARGUMENT(tools::ValidateCustomToolFormat(json("text"), "apply_patch"));
  EXPECT_INVALID_ARGUMENT(tools::ValidateCustomToolFormat(json::array({"text"}), "apply_patch"));
}

TEST(CustomToolFormatTest, RejectsFormatWithoutType) {
  EXPECT_INVALID_ARGUMENT(tools::ValidateCustomToolFormat(json::object(), "apply_patch"));
  EXPECT_INVALID_ARGUMENT(tools::ValidateCustomToolFormat(json::parse(R"({"type":7})"), "apply_patch"));
}

TEST(CustomToolFormatTest, RejectsTextFormatCarryingConflictingMembers) {
  // A text format that also carries grammar members describes a constraint that is never applied.
  EXPECT_INVALID_ARGUMENT(tools::ValidateCustomToolFormat(
      json::parse(R"({"type":"text","grammar":{"syntax":"lark","definition":"start: X"}})"), "apply_patch"));
  EXPECT_INVALID_ARGUMENT(
      tools::ValidateCustomToolFormat(json::parse(R"({"type":"text","syntax":"lark"})"), "apply_patch"));
}

TEST(CustomToolFormatTest, ErrorNamesTheOffendingTool) {
  try {
    tools::ValidateCustomToolFormat(json::parse(R"({"type":"grammar"})"), "apply_patch");
    ADD_FAILURE() << "expected a rejection";
  } catch (const fl::Exception& ex) {
    EXPECT_NE(std::string(ex.what()).find("apply_patch"), std::string::npos) << ex.what();
  }
}

// ========================================================================
// MakeFunctionTool / MakeCustomTool
// ========================================================================

TEST(ToolDefinitionFactoryTest, FunctionToolKeepsItsSchema) {
  auto definition = tools::MakeFunctionTool("get_weather", "Get weather", R"({"type":"object"})");

  EXPECT_EQ(definition.name, "get_weather");
  EXPECT_EQ(definition.description, "Get weather");
  EXPECT_EQ(definition.json_schema, R"({"type":"object"})");
  EXPECT_EQ(definition.kind, ToolKind::kFunction);
}

TEST(ToolDefinitionFactoryTest, FunctionToolWithoutParametersGetsNeutralSchema) {
  // The registry requires a schema for every function tool; `{}` is the JSON Schema spelling of
  // "declares no parameters" and keeps a parameterless tool registrable.
  EXPECT_EQ(tools::MakeFunctionTool("ping", "", "").json_schema, "{}");
}

TEST(ToolDefinitionFactoryTest, WirePresenceIsPreservedForPromptProjection) {
  auto function = tools::MakeFunctionTool("ping", "", "", false, false);
  auto custom = tools::MakeCustomTool("apply_patch", "", false);

  EXPECT_FALSE(function.include_description_in_prompt);
  EXPECT_FALSE(function.include_parameters_in_prompt);
  EXPECT_FALSE(custom.include_description_in_prompt);
  EXPECT_TRUE(custom.include_parameters_in_prompt);
}

TEST(ToolDefinitionFactoryTest, RejectsStrictFunctionToolsThatTheRuntimeCannotEnforce) {
  EXPECT_INVALID_ARGUMENT(tools::MakeFunctionTool("strict_fn", "", "{}", true, true, true));
  EXPECT_NO_THROW(tools::MakeFunctionTool("non_strict_fn", "", "{}", true, true, false));
}

TEST(ToolDefinitionFactoryTest, CustomToolCarriesNoSchema) {
  auto definition = tools::MakeCustomTool("apply_patch", "Apply a patch");

  EXPECT_EQ(definition.name, "apply_patch");
  EXPECT_EQ(definition.kind, ToolKind::kCustom);
  EXPECT_TRUE(definition.json_schema.empty()) << "the registry synthesizes the custom schema";
}

TEST(ToolDefinitionFactoryTest, RegisteredCustomToolGetsExactlyTheSynthesizedSchema) {
  ToolRegistry registry;
  registry.Add(tools::MakeCustomTool("apply_patch", "Apply a patch"));

  auto registered = registry.Definitions();
  ASSERT_EQ(registered.size(), 1u);
  EXPECT_EQ(registered[0].json_schema, kCustomToolInputSchema);
}

// ========================================================================
// NarrowToForcedTool / RetainAllowedTools
// ========================================================================

TEST(ToolDefinitionFilterTest, NarrowToForcedToolKeepsOnlyTheNamedToolOfThatKind) {
  std::vector<ToolDefinition> definitions{tools::MakeFunctionTool("a", "", "{}"),
                                          tools::MakeFunctionTool("b", "", "{}"),
                                          tools::MakeCustomTool("c", "")};

  tools::NarrowToForcedTool(definitions, "b", ToolKind::kFunction);

  ASSERT_EQ(definitions.size(), 1u);
  EXPECT_EQ(definitions[0].name, "b");
}

TEST(ToolDefinitionFilterTest, NarrowToForcedToolMatchesOnKindAsWellAsName) {
  // A function and a custom tool can never be swapped for one another, even under the same name.
  std::vector<ToolDefinition> definitions{tools::MakeFunctionTool("apply_patch", "", "{}")};

  EXPECT_THROW(tools::NarrowToForcedTool(definitions, "apply_patch", ToolKind::kCustom), fl::Exception);
}

TEST(ToolDefinitionFilterTest, NarrowToForcedToolRejectsUndeclaredTools) {
  std::vector<ToolDefinition> definitions{tools::MakeFunctionTool("a", "", "{}"),
                                          tools::MakeFunctionTool("b", "", "{}")};

  EXPECT_THROW(tools::NarrowToForcedTool(definitions, "never_declared", ToolKind::kFunction), fl::Exception);
}

TEST(ToolDefinitionFilterTest, RetainAllowedToolsFiltersCaseInsensitivelyAndKeepsOrder) {
  std::vector<ToolDefinition> definitions{tools::MakeFunctionTool("Alpha", "", "{}"),
                                          tools::MakeFunctionTool("beta", "", "{}"),
                                          tools::MakeCustomTool("Gamma", "")};

  tools::RetainAllowedTools(definitions, {"gamma", "ALPHA"});

  EXPECT_EQ(NamesOf(definitions), (std::vector<std::string>{"Alpha", "Gamma"}));
}

TEST(ToolDefinitionFilterTest, RetainAllowedToolsIgnoresDuplicateAndUnknownEntries) {
  std::vector<ToolDefinition> definitions{tools::MakeFunctionTool("a", "", "{}"),
                                          tools::MakeFunctionTool("b", "", "{}")};

  tools::RetainAllowedTools(definitions, {"a", "a", "a", "never_declared"});

  EXPECT_EQ(NamesOf(definitions), (std::vector<std::string>{"a"}));
}

TEST(ToolDefinitionFilterTest, RetainAllowedToolsWithAnEmptyListKeepsNothing) {
  std::vector<ToolDefinition> definitions{tools::MakeFunctionTool("a", "", "{}")};

  tools::RetainAllowedTools(definitions, {});

  EXPECT_TRUE(definitions.empty());
}

// ========================================================================
// Frozen coding-agent inventory: 18 function tools + 1 custom tool
// ========================================================================

class CodingAgentToolInventoryTest : public ::testing::Test {
 protected:
  static std::vector<ToolDefinition> ChatDefinitions() {
    auto req = ChatRequestWithTools(test::kCodingAgentChatToolsJson);
    Request session_request;
    return chat_completions::ExtractToolDefinitions(req, session_request);
  }

  static std::vector<ToolDefinition> ResponsesDefinitions() {
    auto params = ResponsesParamsWithTools(test::CodingAgentResponsesToolsJson());
    Request session_request;
    return ResponseConverter::ExtractResponsesToolDefinitions(params, session_request);
  }
};

TEST_F(CodingAgentToolInventoryTest, ChatRequestParsesEveryToolInOrder) {
  auto req = ChatRequestWithTools(test::kCodingAgentChatToolsJson);

  ASSERT_TRUE(req.tools.has_value());
  ASSERT_EQ(req.tools->size(), test::CodingAgentToolNames().size());
  EXPECT_EQ(req.tools->size(), 19u);

  for (size_t i = 0; i < req.tools->size(); ++i) {
    const auto& tool = (*req.tools)[i];
    EXPECT_EQ(tool.Name(), test::CodingAgentToolNames()[i]) << "at index " << i;
    EXPECT_EQ(tool.IsCustom(), tool.Name() == test::kCodingAgentCustomToolName) << "at index " << i;
  }
}

TEST_F(CodingAgentToolInventoryTest, EveryDefinitionIsRetainedInOrderWithItsDeclaredKind) {
  auto definitions = ChatDefinitions();

  ASSERT_EQ(definitions.size(), 19u);
  EXPECT_EQ(NamesOf(definitions), test::CodingAgentToolNames());

  int custom_count = 0;
  for (const auto& definition : definitions) {
    if (definition.kind == ToolKind::kCustom) {
      ++custom_count;
      EXPECT_EQ(definition.name, test::kCodingAgentCustomToolName);
    }
  }

  EXPECT_EQ(custom_count, 1) << "exactly one tool was declared custom; no cross-kind conversion";
}

TEST_F(CodingAgentToolInventoryTest, FunctionSchemasAreJsonEquivalentToWhatWasDeclared) {
  auto declared = json::parse(test::kCodingAgentChatToolsJson);
  auto definitions = ChatDefinitions();

  ASSERT_EQ(declared.size(), definitions.size());

  for (size_t i = 0; i < definitions.size(); ++i) {
    if (definitions[i].kind == ToolKind::kCustom) {
      continue;
    }

    const auto& function = declared[i].at("function");
    const auto expected = function.contains("parameters") ? function.at("parameters") : json::object();
    EXPECT_EQ(json::parse(definitions[i].json_schema), expected) << "tool: " << definitions[i].name;
    EXPECT_EQ(definitions[i].description, function.value("description", ""));
  }
}

TEST_F(CodingAgentToolInventoryTest, CustomToolGetsExactlyTheSynthesizedInputSchema) {
  ToolRegistry registry;
  for (auto& definition : ChatDefinitions()) {
    registry.Add(std::move(definition));
  }

  auto registered = registry.Definitions();
  const auto custom = std::find_if(registered.begin(), registered.end(), [](const ToolDefinition& definition) {
    return definition.name == test::kCodingAgentCustomToolName;
  });
  ASSERT_NE(custom, registered.end());
  EXPECT_EQ(custom->kind, ToolKind::kCustom);
  EXPECT_EQ(custom->json_schema, kCustomToolInputSchema);
  EXPECT_EQ(custom->json_schema,
            R"({"type":"object","properties":{"input":{"type":"string"}},"required":["input"],)"
            R"("additionalProperties":false})");
}

TEST_F(CodingAgentToolInventoryTest, RegisteringTheWholeInventoryKeepsEveryNameDistinct) {
  ToolRegistry registry;
  for (auto& definition : ChatDefinitions()) {
    registry.Add(std::move(definition));
  }

  auto registered = registry.Definitions();
  auto kinds = tools::KindsByName(registered);
  EXPECT_EQ(registered.size(), 19u);
  EXPECT_EQ(kinds.size(), 19u);
  EXPECT_EQ(kinds.at(test::kCodingAgentCustomToolName), ToolKind::kCustom);
  EXPECT_EQ(kinds.at("bash"), ToolKind::kFunction);
}

TEST_F(CodingAgentToolInventoryTest, ChatAndResponsesProduceTheSameDefinitions) {
  auto chat = ChatDefinitions();
  auto responses = ResponsesDefinitions();

  ASSERT_EQ(chat.size(), responses.size());

  for (size_t i = 0; i < chat.size(); ++i) {
    EXPECT_EQ(chat[i].name, responses[i].name) << "at index " << i;
    EXPECT_EQ(chat[i].description, responses[i].description) << "tool: " << chat[i].name;
    EXPECT_EQ(chat[i].kind, responses[i].kind) << "tool: " << chat[i].name;
    EXPECT_EQ(json::parse(chat[i].json_schema.empty() ? "null" : chat[i].json_schema),
              json::parse(responses[i].json_schema.empty() ? "null" : responses[i].json_schema))
        << "tool: " << chat[i].name;
  }
}

TEST_F(CodingAgentToolInventoryTest, DefinitionsMatchWhatTheSameToolsRegisteredThroughTheCAbiProduce) {
  // The C ABI describes a tool with a name, a description, a schema and a kind. Registering the
  // inventory that way must land on the same definitions the HTTP surfaces produce, since all three
  // feed one registry and one prompt serializer.
  auto declared = json::parse(test::kCodingAgentChatToolsJson);
  auto chat = ChatDefinitions();
  ASSERT_EQ(declared.size(), chat.size());

  ToolRegistry from_abi;
  ToolRegistry from_chat;

  for (size_t i = 0; i < declared.size(); ++i) {
    const bool is_custom = declared[i].at("type") == "custom";
    const auto& declaration = is_custom ? declared[i].at("custom") : declared[i].at("function");
    const std::string name = declaration.at("name").get<std::string>();
    const std::string description = declaration.value("description", "");
    const std::string schema =
        declaration.contains("parameters") ? declaration.at("parameters").dump() : std::string{"{}"};

    flToolDefinition abi{};
    abi.version = FOUNDRY_LOCAL_API_VERSION;
    abi.name = name.c_str();
    abi.description = description.c_str();
    abi.json_schema = is_custom ? nullptr : schema.c_str();
    abi.kind = is_custom ? FOUNDRY_LOCAL_TOOL_KIND_CUSTOM : FOUNDRY_LOCAL_TOOL_KIND_FUNCTION;

    from_abi.Add(ToolDefinitionFromC(abi));
    from_chat.Add(chat[i]);
  }

  auto abi_definitions = from_abi.Definitions();
  auto chat_definitions = from_chat.Definitions();
  ASSERT_EQ(abi_definitions.size(), chat_definitions.size());

  for (size_t i = 0; i < abi_definitions.size(); ++i) {
    EXPECT_EQ(abi_definitions[i].name, chat_definitions[i].name) << "at index " << i;
    EXPECT_EQ(abi_definitions[i].description, chat_definitions[i].description) << "at index " << i;
    EXPECT_EQ(abi_definitions[i].kind, chat_definitions[i].kind) << "at index " << i;
    EXPECT_EQ(json::parse(abi_definitions[i].json_schema), json::parse(chat_definitions[i].json_schema))
        << "tool: " << abi_definitions[i].name;
  }
}

TEST_F(CodingAgentToolInventoryTest, ExplicitNonStrictToolsRemainSupported) {
  auto declared = json::parse(test::kCodingAgentChatToolsJson);
  ASSERT_TRUE(declared[17].at("function").contains("strict"));
  ASSERT_FALSE(declared[17].at("function").at("strict").get<bool>());

  auto definitions = ChatDefinitions();
  EXPECT_EQ(definitions[17].name, "report_progress");
  EXPECT_EQ(json::parse(definitions[17].json_schema), declared[17].at("function").at("parameters"));
}
