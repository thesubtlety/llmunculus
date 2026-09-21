#!/usr/bin/env python3
"""run eval/tasks.txt through the agent. results append to a json file so runs can be chunked and resumed.
usage: eval.py <jb binary> <model.gguf|-> <results.json> [--only id,id] [--limit N] [--report]"""
import json, os, re, subprocess, sys, time
jb, model, out = sys.argv[1:4]
only = set(); limit = 99; report = "--report" in sys.argv
if "--only" in sys.argv: only = set(sys.argv[sys.argv.index("--only") + 1].split(","))
if "--limit" in sys.argv: limit = int(sys.argv[sys.argv.index("--limit") + 1])
results = json.load(open(out)) if os.path.exists(out) else {}

def expected(cmd): return subprocess.run(cmd, shell=True, capture_output=True, text=True).stdout.strip()
def numbers(s): return [float(x.replace(",", "")) for x in re.findall(r"\d[\d,]*\.?\d*", s)]
def check(mode, exp, answer):
    # the expected command may print several acceptable values, one per line. any of them passes.
    for exp in exp.split():
        if mode == "exact" and (exp in re.findall(r"\d+", answer) or exp.lower() in answer.lower()): return True
        if mode == "has" and exp.lower() in answer.lower(): return True
        if mode.startswith("num"):
            pct = float(mode.split(":")[1]); e = float(exp)
            for n in numbers(answer):
                for scale in (1, 1024, 1024**2, 1024**3, 1000, 1000**2, 1000**3):
                    if abs(n * scale - e) <= e * pct / 100 + 0.5: return True
    return False

tasks = [l.split(" :: ") for l in open("eval/tasks.txt") if l.strip() and not l.startswith("#")]
done = 0
for tid, task, cmd, mode in (map(str.strip, t) for t in tasks):
    if (only and tid not in only) or (not only and tid in results) or done >= limit: continue
    exp = expected(cmd)
    t0 = time.time()
    args = ["sh", jb, "-v"] + ([model] if model != "-" else []) + [task]   # an APE needs sh to start from python
    p = subprocess.run(args, capture_output=True, text=True, timeout=1200)
    wall = time.time() - t0
    log = p.stderr
    os.makedirs(out + ".logs", exist_ok=True)
    open(f"{out}.logs/{tid}.log", "w").write(log)   # the step log, for failure analysis
    r = dict(task=task, expected=exp, answer=p.stdout.strip(), mode=mode, wall=round(wall, 1),
             steps=len(re.findall(r"^\[(plan|code|observe|answer|squash)\]", log, re.M)),
             programs=len(re.findall(r"^\[run\]", log, re.M)),
             compile_errors=len(re.findall(r"^\[run\] status=2", log, re.M)),
             verbatim=len(re.findall(r"\(verbatim\)", log)),
             facts=len(re.findall(r"^\[fact\] (?!\(none\))", log, re.M)))
    r["pass"] = check(mode, exp, r["answer"])
    results[tid] = r; done += 1
    json.dump(results, open(out, "w"), indent=1)
    print(f"{'PASS' if r['pass'] else 'FAIL'} {tid:10} {wall:6.1f}s steps={r['steps']:2} programs={r['programs']} cerr={r['compile_errors']} exp={exp[:30]!r} got={r['answer'][:70]!r}")

if report or not done:
    n = len(results); ok = sum(r["pass"] for r in results.values())
    # prose pass: the sentence alone, without the "also learned" footer, holds the right value
    prose = sum(check(r["mode"], r["expected"], r["answer"].split("also learned:")[0]) for r in results.values())
    if n:
        print(f"\n{ok}/{n} pass, {prose}/{n} in the sentence itself. mean wall {sum(r['wall'] for r in results.values())/n:.0f}s, "
              f"mean programs {sum(r['programs'] for r in results.values())/n:.1f}, "
              f"compile errors {sum(r['compile_errors'] for r in results.values())}, "
              f"verbatim facts {sum(r['verbatim'] for r in results.values())}")
        fails = [t for t, r in results.items() if not r["pass"]]
        print("fails:", " ".join(fails) if fails else "none")
