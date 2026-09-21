// GBNF. one grammar per step. the sampler masks any token that cannot continue a valid parse.
// trailing newlines are optional everywhere: some models end a reply with end-of-turn directly, and a grammar that
// insists on "\n" first makes them pad with spaces to the length cap instead. newlines only separate items.
#include "grammar.h"

// NEED learns something. DO changes something or reports somewhere. no backticks: it stops quoted shell commands.
const char *G_PLAN =
    "root ::= item (\"\\n\" item){0,2} \"\\n\"?\n"
    "item ::= (\"NEED: \" | \"DO: \") line\n"
    "line ::= [^\\n`]{3,120}\n";

// exactly one step. the first thing tried for every task: no room to pad, no room to split a compound action.
const char *G_PLAN_ONE =
    "root ::= (\"NEED: \" | \"DO: \") line \"\\n\"?\n"
    "line ::= [^\\n`]{3,160}\n";

// same as G_PLAN, but NONE is allowed. only used when facts exist, so the lazy answer is an informed one.
const char *G_PLAN_NONE =
    "root ::= item (\"\\n\" item){0,2} \"\\n\"? | \"NONE\" \"\\n\"?\n"
    "item ::= (\"NEED: \" | \"DO: \") line\n"
    "line ::= [^\\n`]{3,120}\n";

// a body char is anything but a backtick, or 1-2 backticks followed by a non-backtick.
// so the only place three backticks can appear is the closing fence.
const char *G_CODE =
    "root ::= \"```c\\n\" body \"```\" \"\\n\"?\n"
    "body ::= ( [^`] | \"`\" [^`] | \"``\" [^`] )*\n";

// for short program output. the model only judges. the fact text is the raw output, so a value cannot be paraphrased.
const char *G_VERDICT =
    "root ::= (\"FACT\" | \"FIX: \" line) \"\\n\"?\n"
    "line ::= [^\\n]{2,80}\n";

// for long output the model must summarize. short on purpose: the fact travels in every later prompt.
const char *G_OBSERVE =
    "root ::= (\"FACT: \" | \"FIX: \") line \"\\n\"?\n"
    "line ::= [^\\n]{2,80}\n";

// 1 to 12 short lines. for squashing the facts list.
const char *G_LINES =
    "root ::= \"- \" line (\"\\n- \" line){0,11} \"\\n\"?\n"
    "line ::= [^\\n]{2,100}\n";

// one line. given slots, a small model fills every slot with a variant of the same answer.
// it may not start with "DO:" or "NEED:", a small model echoes the plan otherwise, nor with '<': a thinking model opens <think>.
const char *G_ANSWER =
    "root ::= start [^\\n]{0,199} \"\\n\"?\n"
    "start ::= [^DN<\\n] | \"D\" [^O\\n] | \"DO\" [^:\\n] | \"N\" [^E\\n] | \"NE\" [^E\\n] | \"NEE\" [^D\\n] | \"NEED\" [^:\\n]\n";

// a condition. the exit status follows it: 2 for yes.
const char *G_YESNO =
    "root ::= (\"yes\" | \"no\" | \"unknown\") \"\\n\"?\n";
