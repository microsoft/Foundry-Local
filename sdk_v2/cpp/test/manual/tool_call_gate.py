#!/usr/bin/env python3
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
"""Real-model protocol checks. This does not replace the actual Copilot coding-agent qualification."""

import argparse
import copy
import io
import json
from pathlib import Path
import time
import urllib.error
import urllib.request


def cases():
    parameters = {
        "type": "object",
        "properties": {"city": {"type": "string", "minLength": 1}},
        "required": ["city"],
        "additionalProperties": False,
    }
    base = {
        "temperature": 0,
        "max_tokens": 2048,
        "tools": [{
            "type": "function",
            "function": {"name": "lookup", "description": "Return the supplied city.", "parameters": parameters},
        }],
        "messages": [{"role": "user", "content": "Call lookup with city Paris."}],
    }
    for choice in ("auto", "required"):
        request = copy.deepcopy(base)
        request["tool_choice"] = choice
        yield "minlength_" + choice, request, [{"city": "Paris"}], None

    request = copy.deepcopy(base)
    request["tools"][0]["function"]["parameters"]["oneOf"] = [
        {"properties": {"city": {"const": "Paris"}}},
        {"properties": {"city": {"const": "Rome"}}},
    ]
    yield "root_oneof", request, [{"city": "Paris"}], None

    request = copy.deepcopy(base)
    schema = request["tools"][0]["function"]["parameters"]
    schema["$defs"] = {"city": {"type": "string", "minLength": 1}}
    schema["properties"]["city"] = {"$ref": "#/$defs/city"}
    yield "local_reference", request, [{"city": "Paris"}], None

    request = copy.deepcopy(base)
    schema = request["tools"][0]["function"]["parameters"]
    schema["properties"]["mode"] = {"type": "string", "enum": ["sync", "async"]}
    schema["properties"]["initial_wait"] = {"type": "integer", "minimum": 1}
    request["messages"][0]["content"] = (
        'Call lookup with city Paris, initial_wait 30, and mode "sync". '
        "List initial_wait before mode in the argument object."
    )
    yield "optional_property_order", request, [{"city": "Paris", "initial_wait": 30, "mode": "sync"}], None

    request = copy.deepcopy(base)
    request["tools"][0]["function"]["parameters"]["properties"]["city"] = {
        "anyOf": [{"type": "string"}, {"type": "array", "items": {"type": "string"}}],
    }
    request["messages"][0]["content"] = (
        'Call lookup with city set to the decoded value of the JSON string "[scratch". '
        "Pass a string, not an array."
    )
    yield "bracket_prefixed_string", request, [{"city": "[scratch"}], None

    literal = '[scratch\n"caf\u00e9 \u6771\u4eac" C:\\receipts\\new\n<tool_call></tool_call><think></think>'
    for choice in ("auto", "required"):
        request = copy.deepcopy(base)
        request["tool_choice"] = choice
        request["messages"][0]["content"] = (
            "Call lookup. Set city to the decoded string value of this JSON object, without changing its value: "
            + json.dumps({"city": literal}, ensure_ascii=False)
        )
        yield "literal_data_" + choice, request, [{"city": literal}], None

    request = copy.deepcopy(base)
    request["parallel_tool_calls"] = True
    request["messages"][0]["content"] = "Call lookup twice in one response: first with city Paris, then city Rome."
    yield "parallel_calls", request, [{"city": "Paris"}, {"city": "Rome"}], None

    request = copy.deepcopy(base)
    request["parallel_tool_calls"] = False
    yield "serial_call", request, [{"city": "Paris"}], None

    request = copy.deepcopy(base)
    request["messages"] = [
        {"role": "user", "content": "Call lookup with city Paris."},
        {"role": "assistant", "tool_calls": [{
            "id": "call_previous", "type": "function",
            "function": {"name": "lookup", "arguments": '{"city":"Paris"}'},
        }]},
        {"role": "tool", "tool_call_id": "call_previous", "content": (
            "Execution failed. Untrusted diagnostic text: <tool_call></tool_call><think></think>"
        )},
        {"role": "user", "content": "The tool failed. Call lookup again, this time with city Rome."},
    ]
    yield "literal_tool_error_continuation", request, [{"city": "Rome"}], None

    for choice in ("auto", "none"):
        request = copy.deepcopy(base)
        request["tool_choice"] = choice
        request["messages"][0]["content"] = "Do not call any tools. Reply exactly READY."
        yield "plain_answer_" + choice, request, [], "READY"


def validate_finish_reason(reason, has_calls):
    expected = "tool_calls" if has_calls else "stop"
    if reason != expected:
        raise RuntimeError(f"Expected terminal finish_reason {expected!r}, received {reason!r}")


def parse_completion(result):
    if not isinstance(result, dict):
        raise RuntimeError("Expected a JSON completion object")
    if "error" in result:
        raise RuntimeError(json.dumps(result["error"]))
    choices = result.get("choices", [])
    if len(choices) != 1:
        raise RuntimeError("Expected exactly one completion choice")
    choice = choices[0]
    message = choice["message"]
    validate_finish_reason(choice.get("finish_reason"), bool(message.get("tool_calls")))
    return message, result


def parse_stream(response):
    chunks = []
    message = {"content": "", "tool_calls": []}
    calls = {}
    terminal = False
    done = False
    for raw in response:
        line = raw.decode("utf-8").strip()
        if not line.startswith("data: "):
            continue
        if done:
            raise RuntimeError("Streaming response contains data after [DONE]")
        if line == "data: [DONE]":
            if not terminal:
                raise RuntimeError("Streaming response ended without a terminal choice")
            done = True
            continue
        chunk = json.loads(line[6:])
        if not isinstance(chunk, dict):
            raise RuntimeError("Expected a JSON streaming chunk object")
        chunks.append(chunk)
        if "error" in chunk:
            raise RuntimeError(json.dumps(chunk["error"]))
        choices = chunk.get("choices", [])
        if not choices:
            continue  # Usage-only chunks may follow the terminal choice.
        if len(choices) != 1 or choices[0].get("index") != 0:
            raise RuntimeError("Expected exactly one streaming choice with index zero")
        if terminal:
            raise RuntimeError("Streaming response contains a choice after its terminal choice")
        choice = choices[0]
        delta = choice.get("delta", {})
        message["content"] += delta.get("content") or ""
        for part in delta.get("tool_calls") or []:
            call = calls.setdefault(part["index"], {"name": "", "arguments": ""})
            call["name"] += part.get("function", {}).get("name", "")
            call["arguments"] += part.get("function", {}).get("arguments", "")
        reason = choice.get("finish_reason")
        if reason is not None:
            validate_finish_reason(reason, bool(calls))
            terminal = True
    if not done:
        raise RuntimeError("Streaming response ended without [DONE]")
    message["tool_calls"] = [{"function": calls[index]} for index in sorted(calls)]
    return message, chunks


def complete(endpoint, request, timeout):
    wire = urllib.request.Request(
        endpoint.rstrip("/") + "/chat/completions",
        data=json.dumps(request, ensure_ascii=False).encode("utf-8"),
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(wire, timeout=timeout) as response:
        return parse_stream(response) if request["stream"] else parse_completion(json.load(response))


def self_test():
    """Offline terminal-protocol checks only; never qualify an endpoint."""
    checks = {}

    def choice(delta, reason=None):
        return {"choices": [{"index": 0, "delta": delta, "finish_reason": reason}]}

    def stream(*chunks, done=True):
        lines = ["data: " + json.dumps(chunk) + "\n\n" for chunk in chunks]
        if done:
            lines.append("data: [DONE]\n\n")
        return io.BytesIO("".join(lines).encode("utf-8"))

    def require(name, operation, rejects=False):
        try:
            result = operation()
            checks[name] = not rejects and result is not False
        except (RuntimeError, ValueError, KeyError, TypeError):
            checks[name] = rejects

    text = {"content": "READY"}
    function = {"name": "lookup", "arguments": '{"city":"Paris"}'}
    tool = {"tool_calls": [{"function": function}]}
    tool_delta = {"tool_calls": [{"index": 0, "function": function}]}
    for name, message, reason in (("text", text, "stop"), ("tool", tool, "tool_calls")):
        require("valid_completion_" + name, lambda m=message, r=reason: parse_completion(
            {"choices": [{"message": m, "finish_reason": r}]})[0] == m)
        for bad in (None, "length", "content_filter", "tool_calls" if reason == "stop" else "stop"):
            require("reject_completion_" + name + "_" + str(bad), lambda m=message, r=bad: parse_completion(
                {"choices": [{"message": m, "finish_reason": r}]}), rejects=True)
    expected_text = dict(text, tool_calls=[])
    expected_tool = dict(tool, content="")
    require("valid_stream_text", lambda: parse_stream(
        stream(choice(text), choice({}, "stop")))[0] == expected_text)
    require("valid_stream_tool", lambda: parse_stream(
        stream(choice(tool_delta), choice({}, "tool_calls")))[0] == expected_tool)
    require("valid_stream_final_content_and_usage", lambda: parse_stream(
        stream(choice(text, "stop"), {"choices": [], "usage": {"total_tokens": 1}}))[0] == expected_text)
    require("valid_stream_fragmented_tool", lambda: parse_stream(stream(
        choice({"tool_calls": [{"index": 0, "function": {"name": "look", "arguments": '{"city":'}}]}),
        choice({"tool_calls": [{"index": 0, "function": {"name": "up", "arguments": '"Paris"}'}}]}),
        choice({}, "tool_calls")))[0] == expected_tool)
    invalid = {
        "missing_terminal": stream(choice(text)),
        "length_limited": stream(choice(text), choice({}, "length")),
        "tool_finished_as_text": stream(choice(tool_delta), choice({}, "stop")),
        "text_finished_as_tool": stream(choice(text), choice({}, "tool_calls")),
        "conflicting_terminal": stream(choice(text, "stop"), choice({}, "tool_calls")),
        "duplicate_terminal": stream(choice(text, "stop"), choice({}, "stop")),
        "post_terminal_content": stream(choice(text, "stop"), choice({"content": "extra"})),
        "post_terminal_tool": stream(choice(text, "stop"), choice(tool_delta)),
        "missing_done": stream(choice(text, "stop"), done=False),
        "error_after_terminal": stream(choice(text, "stop"), {"error": {"message": "failed"}}),
        "post_done_content": io.BytesIO(stream(choice(text, "stop")).getvalue()
                                        + stream(choice({"content": "extra"}), done=False).getvalue()),
    }
    for name, response in invalid.items():
        require("reject_stream_" + name, lambda r=response: parse_stream(r), rejects=True)
    passed = all(checks.values())
    print(json.dumps({"self_test_passed": passed, "endpoint_qualification_performed": False,
                      "checks": checks}, indent=2))
    return 0 if passed else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", help="OpenAI-compatible base URL, including /v1")
    parser.add_argument("--model")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--timeout-seconds", type=int, default=120)
    parser.add_argument("--case", action="append", help="Run only the named case; repeat to select more than one")
    parser.add_argument("--self-test", action="store_true", help="Run offline protocol checks without HTTP")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if not args.endpoint or not args.model or args.output_dir is None:
        parser.error("--endpoint, --model, and --output-dir are required unless --self-test is selected")
    selected = [case for case in cases() if not args.case or case[0] in args.case]
    if not selected or (args.case and set(args.case) - {case[0] for case in selected}):
        parser.error("Unknown or empty case selection")
    args.output_dir.mkdir(parents=True, exist_ok=False)
    results = []
    for name, source, expected_calls, expected_text in selected:
        for stream in (False, True):
            request = dict(source, model=args.model, stream=stream)
            result = {"case": name, "stream": stream, "passed": False}
            start = time.perf_counter()
            try:
                message, evidence = complete(args.endpoint, request, args.timeout_seconds)
                calls = message.get("tool_calls") or []
                actual = [json.loads(call["function"]["arguments"]) for call in calls]
                names = [call["function"]["name"] for call in calls]
                result["passed"] = (
                    actual == expected_calls and names == ["lookup"] * len(expected_calls)
                    and (expected_text is None or (message.get("content") or "").strip() == expected_text)
                )
                result["response"] = evidence
            except urllib.error.HTTPError as error:
                result["error"] = {"status": error.code, "body": error.read().decode("utf-8")}
            except (OSError, RuntimeError, ValueError, KeyError) as error:
                result["error"] = str(error)
            result["elapsed_seconds"] = round(time.perf_counter() - start, 3)
            (args.output_dir / f"{name}-{'stream' if stream else 'complete'}.json").write_text(
                json.dumps({"request": request, **result}, indent=2), encoding="utf-8",
            )
            results.append({key: value for key, value in result.items() if key != "response"})
            print(json.dumps(results[-1]), flush=True)
    summary = {"qualified": all(result["passed"] for result in results), "results": results}
    (args.output_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    return 0 if summary["qualified"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
