# Orchestration Patterns for Multi-Agent Systems

This document describes common orchestration patterns used to coordinate
multiple AI agents in a workflow.

## 1. Sequential Pipeline

Agents run in a fixed order. Each agent receives the output of the previous one.

```
User Question → Planner → Retriever → Critic → Writer → Final Report
```

**When to use:** Research tasks where each step depends on the previous result.

**Trade-offs:** Simple to reason about, but total latency equals the sum of all
agent execution times.

## 2. Concurrent Fan-Out

Multiple agents process the same input simultaneously using `asyncio.gather()`.

```
                ┌─ Retriever  ──┐
Plan Text ──────┤               ├─► Merge
                └─ ToolAgent   ──┘
```

**When to use:** Independent sub-tasks that can run in parallel, such as
document retrieval and text analysis.

**Trade-offs:** Faster than sequential for independent work, but merging results
requires careful design.

## 3. Critic Feedback Loop

A Critic agent iteratively reviews outputs and may trigger re-processing.

```
Retriever output ──► Critic ──► Gap found? ──► Yes ──► Re-retrieve ──┐
                        ▲                                             │
                        └─────────────────────────────────────────────┘
                                              │
                                          No gaps ──► Continue
```

**When to use:** Tasks requiring quality assurance or iterative refinement.

**Trade-offs:** Improves output quality but adds latency. Always set a maximum
loop count (e.g. `MAX_CRITIC_LOOPS = 2`) to prevent infinite loops.

## 4. Hybrid Orchestration

Combines sequential, concurrent, and feedback patterns into a single workflow.

```
Question ──► Planner (seq) ──► Retriever ‖ ToolAgent (concurrent) ──► Critic Loop ──► Writer (seq)
```

This is the pattern used by the agent-framework sample's `run_full_workflow()`.

## Key Implementation Notes

- Use `async`/`await` throughout for non-blocking execution.
- Wrap agent calls with timing (`time.perf_counter()`) for observability.
- Emit structured step events (JSON) for UI streaming via SSE.
- Handle agent errors gracefully — a single agent failure shouldn't crash the workflow.
