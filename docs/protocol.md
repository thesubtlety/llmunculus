# protocol

Model output is terse text. A grammar per step. The harness picks the grammar. The model cannot break format.

## system prompt (draft, keep under 400 tokens)

```
You are a build agent on a linux x86_64 machine. You learn about it by writing tiny C programs.
Reply in the exact format asked. Short lines.
```

The OS and arch come from `uname` at runtime, see `src/host.c`. Without it the 3B wrote BSD code on Linux. The C rules go in the CODE step's message, not the system prompt.

Plus two worked examples. Measure their token cost. Cut if over budget.

## steps

| step | model says | grammar |
|---|---|---|
| PLAN1 | one `NEED: <question>` or `DO: <action>`, the whole task | plan one |
| PLAN | `NEED:` or `DO:` lines, 1 to 3, no backticks. fallback | plan |
| CODE | one fenced C block | code |
| FIX | one fenced C block | code |
| OBSERVE | `FACT: <answer only>` or `FIX: <what to change>`, 80 chars | observe |
| ANSWER | one line | answer |

Each step is a new generation. Input is: system prompt, facts so far, and the current NEED. Old code and output are not shown once a FACT exists.

## grammars (GBNF, draft)

```
# the grammars live in src/grammar.c and have moved past this doc. current shapes, verbatim intent:
# plan (one-step)  : one  "NEED: <q>" or "DO: <a>", no backticks, optional trailing newline
# plan (fallback)  : 1-3 of those, or "NONE"
# code             : one ```c ... ``` fence, optional trailing newline
# observe (short)  : "FACT" (verbatim path) or "FIX: <line>"
# observe (long)   : "FACT: <line>" or "FIX: <line>", line <= 80 chars
# answer           : one line, not starting with D/N/< (no plan echo, no <think>)
# yes/no verdict   : exactly "yes" | "no" | "unknown"
```c\n" body "```\n"
body ::= ( [^`] | "`" [^`] | "``" [^`] )*

# observe
root ::= ("FACT: " | "FIX: ") line "\n"
line ::= [^\n]{2,80}

# answer
root ::= line "\n"
line ::= [^\n]{1,300}
```

The exact user messages for each step are in `src/loop.c`. They changed six times during M4. See 04-loop.md for why.

Lines are bounded on purpose. An unbounded line let the model skip the newline and run on. Plan has no DONE option. Offered one, the 3B took it two times in three. Deciding done is a later step's job.

## how constrained decoding works

1. Grammar becomes a parser with a stack of possible states.
2. Each token step the engine has a logit per vocab entry.
3. Sampler asks the parser: can this token's text extend a valid parse?
4. Tokens that cannot get logit -inf.
5. Sample from what is left. Advance the parser with the chosen token.

Costs: the vocab is about 151k. Checking all of them every token made generation 3x slower. The engine samples first, asks the grammar about that one token, and only scans the whole vocab on a reject. Rejects are rare, about 1 token in 20. See 02-grammar.md.
Edge cases: one token can cross a grammar boundary. A token can be half a UTF-8 char. The parser must handle partial matches.

## why not JSON tool calls

A 3B model drops quotes, nests wrong, or adds prose. A grammar could force JSON too. Terse lines are fewer tokens and easier to read in a log.
