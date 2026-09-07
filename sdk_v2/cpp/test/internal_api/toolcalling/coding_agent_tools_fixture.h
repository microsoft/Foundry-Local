// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Frozen tool inventory from a coding-agent CLI: eighteen ordinary function tools plus one custom
// (free-form text) tool, exactly as such a client sends them on /v1/chat/completions.
//
// The point of freezing it is that a real client's request is large, mixed and order-sensitive: it
// is the case where a definition being dropped, renamed, deduplicated or converted to the wrong
// kind is easy to miss and expensive to hit. Tests assert this inventory survives parsing intact on
// both HTTP surfaces, so any change to tool handling has to keep a real request working.
//
// Do not edit the payload to make a test pass. It is a captured client shape, not a fixture that
// exists to be convenient.
#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace fl {
namespace test {

/// The `tools` array as sent on /v1/chat/completions: function tools nested under "function",
/// the custom tool nested under "custom" with an explicit text format.
inline constexpr const char* kCodingAgentChatToolsJson = R"([
  {
    "type": "function",
    "function": {
      "name": "bash",
      "description": "Run a bash command in the workspace.",
      "parameters": {
        "type": "object",
        "properties": {
          "command": {"type": "string", "description": "The command to run."},
          "timeout_ms": {"type": "integer", "minimum": 0},
          "cwd": {"type": "string"}
        },
        "required": ["command"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "read_bash",
      "description": "Read output from a running bash session.",
      "parameters": {
        "type": "object",
        "properties": {
          "session_id": {"type": "string"},
          "delay_ms": {"type": "integer"}
        },
        "required": ["session_id"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "stop_bash",
      "description": "Stop a running bash session.",
      "parameters": {
        "type": "object",
        "properties": {"session_id": {"type": "string"}},
        "required": ["session_id"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "list_bash",
      "description": "List the active bash sessions.",
      "parameters": {"type": "object", "properties": {}, "additionalProperties": false}
    }
  },
  {
    "type": "function",
    "function": {
      "name": "view",
      "description": "View a file or directory.",
      "parameters": {
        "type": "object",
        "properties": {
          "path": {"type": "string"},
          "view_range": {"type": "array", "items": {"type": "integer"}}
        },
        "required": ["path"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "create",
      "description": "Create a new file.",
      "parameters": {
        "type": "object",
        "properties": {
          "path": {"type": "string"},
          "file_text": {"type": "string"}
        },
        "required": ["path", "file_text"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "str_replace",
      "description": "Replace an exact string in a file.",
      "parameters": {
        "type": "object",
        "properties": {
          "path": {"type": "string"},
          "old_str": {"type": "string"},
          "new_str": {"type": "string"}
        },
        "required": ["path", "old_str", "new_str"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "glob",
      "description": "Find files matching a glob pattern.",
      "parameters": {
        "type": "object",
        "properties": {
          "pattern": {"type": "string"},
          "path": {"type": "string"}
        },
        "required": ["pattern"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "grep",
      "description": "Search file contents with a regular expression.",
      "parameters": {
        "type": "object",
        "properties": {
          "pattern": {"type": "string"},
          "path": {"type": "string"},
          "case_sensitive": {"type": "boolean"}
        },
        "required": ["pattern"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "list_dir",
      "description": "List the entries of a directory.",
      "parameters": {
        "type": "object",
        "properties": {"path": {"type": "string"}},
        "required": ["path"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "fetch",
      "description": "Fetch a URL and return its contents.",
      "parameters": {
        "type": "object",
        "properties": {
          "url": {"type": "string"},
          "raw": {"type": "boolean"}
        },
        "required": ["url"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "web_search",
      "description": "Search the web.",
      "parameters": {
        "type": "object",
        "properties": {"query": {"type": "string"}},
        "required": ["query"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "github_repo",
      "description": "Search a GitHub repository for code snippets.",
      "parameters": {
        "type": "object",
        "properties": {
          "repo": {"type": "string"},
          "query": {"type": "string"}
        },
        "required": ["repo", "query"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "run_tests",
      "description": "Run the project's tests.",
      "parameters": {
        "type": "object",
        "properties": {
          "filter": {"type": "string"},
          "verbose": {"type": "boolean"}
        },
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "edit_notebook",
      "description": "Edit a notebook cell.",
      "parameters": {
        "type": "object",
        "properties": {
          "path": {"type": "string"},
          "cell_id": {"type": "string"},
          "source": {"type": "string"}
        },
        "required": ["path", "cell_id", "source"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "todo_write",
      "description": "Record the task list for the current session.",
      "parameters": {
        "type": "object",
        "properties": {
          "todos": {
            "type": "array",
            "items": {
              "type": "object",
              "properties": {
                "id": {"type": "string"},
                "title": {"type": "string"},
                "status": {"type": "string", "enum": ["pending", "in_progress", "completed"]}
              },
              "required": ["id", "title", "status"]
            }
          }
        },
        "required": ["todos"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "task",
      "description": "Delegate a sub-task to another agent.",
      "parameters": {
        "type": "object",
        "properties": {
          "agent_type": {"type": "string"},
          "prompt": {"type": "string"}
        },
        "required": ["agent_type", "prompt"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "report_progress",
      "description": "Report progress back to the user.",
      "parameters": {
        "type": "object",
        "properties": {
          "summary": {"type": "string"},
          "done": {"type": "boolean"}
        },
        "required": ["summary"],
        "additionalProperties": false
      },
      "strict": false
    }
  },
  {
    "type": "custom",
    "custom": {
      "name": "apply_patch",
      "description": "Apply a unified patch to the workspace.",
      "format": {"type": "text"}
    }
  }
])";

/// Every declared tool name, in the order the request declares them.
inline const std::vector<std::string>& CodingAgentToolNames() {
  static const std::vector<std::string> names = {
      "bash",       "read_bash",  "stop_bash", "list_bash",  "view",     "create",     "str_replace",
      "glob",       "grep",       "list_dir",  "fetch",      "web_search", "github_repo", "run_tests",
      "edit_notebook", "todo_write", "task",   "report_progress", "apply_patch"};
  return names;
}

/// The one tool the inventory declares as custom.
inline constexpr const char* kCodingAgentCustomToolName = "apply_patch";

/// The same inventory in the Responses flat shape: a function tool inlines name/description/
/// parameters next to "type", and a custom tool inlines name/description/format.
///
/// Derived from the chat payload rather than written out a second time, so the two surfaces are
/// provably given the same tools when their parse results are compared.
inline nlohmann::json CodingAgentResponsesToolsJson() {
  auto chat_tools = nlohmann::json::parse(kCodingAgentChatToolsJson);
  auto responses_tools = nlohmann::json::array();

  for (const auto& tool : chat_tools) {
    nlohmann::json flat{{"type", tool.at("type")}};
    const auto& declaration = tool.contains("custom") ? tool.at("custom") : tool.at("function");

    for (const auto& [key, value] : declaration.items()) {
      flat[key] = value;
    }

    responses_tools.push_back(std::move(flat));
  }

  return responses_tools;
}

}  // namespace test
}  // namespace fl
