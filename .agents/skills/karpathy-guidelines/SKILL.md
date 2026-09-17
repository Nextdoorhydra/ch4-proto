---
name: karpathy-guidelines
description: Apply disciplined coding behavior when writing, reviewing, debugging, or refactoring code. Use to surface assumptions, avoid overcomplication, keep changes surgical, and define verifiable success criteria; use judgment for trivial tasks.
---

# Karpathy Guidelines

Apply these behavioral guidelines to reduce common coding-agent mistakes. They bias toward caution over speed; use judgment for trivial tasks.

## 1. Think Before Coding

Do not assume or hide confusion. Surface tradeoffs.

Before implementing:

- State material assumptions explicitly. Ask when uncertainty would materially change the result.
- Present distinct interpretations instead of silently choosing one.
- Point out a meaningfully simpler approach or a warranted concern.
- Stop and identify what is unclear when safe progress is not possible.

## 2. Prefer Simplicity

Write the minimum code that solves the requested problem.

- Do not add unrequested features.
- Do not create abstractions for a single use.
- Do not add speculative flexibility or configurability.
- Do not handle impossible scenarios.
- If the implementation is substantially longer than necessary, simplify it.

Ask: "Would a senior engineer consider this overcomplicated?" If so, simplify.

## 3. Make Surgical Changes

Touch only what the task requires and clean up only consequences of the current change.

When editing existing code:

- Do not improve adjacent code, comments, or formatting.
- Do not refactor unrelated behavior.
- Match the existing local style.
- Mention unrelated dead code without deleting it.
- Remove imports, variables, or functions only when the current change makes them unused.

Every changed line should trace directly to the user's request.

## 4. Execute Toward Verifiable Goals

Translate the request into observable success criteria and continue until they are verified.

Examples:

- "Add validation" becomes "Add a failing invalid-input test, implement validation, and make the test pass."
- "Fix the bug" becomes "Reproduce the bug with a test, implement the fix, and make the test pass."
- "Refactor X" becomes "Verify relevant behavior before and after the refactor."

For multi-step tasks, state a brief plan in this form:

```text
1. [Step] - verify: [check]
2. [Step] - verify: [check]
3. [Step] - verify: [check]
```

Source attribution: adapted from Andrej Karpathy's observations on common LLM coding pitfalls and provided by the user under the MIT license.
