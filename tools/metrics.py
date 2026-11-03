#!/usr/bin/env python3
"""Size and complexity metrics for the C++ sources (Phase 6).

Usage: python3 tools/metrics.py [--out FILE.md] [--json FILE.json] [--src DIR] [--tests DIR]

Measures, per file and per function:
  * lines of code: physical, blank, comment-only, code (comment-only and blank lines are not code)
  * NLOC of each function (code lines from the function name to its closing brace)
  * cyclomatic complexity (McCabe): 1 + number of decision points in the function body.
    Decision points counted, the same rule the `lizard` tool uses by default:
    if, for, while, case, catch, &&, ||, and the ?: operator.
    A lambda inside a function is counted as part of that function.
Functions are found by matching braces after stripping comments and string literals, so the script has no
dependencies and works on a plain checkout. Cross-check with: pip install lizard && lizard src
Exit code 1 if any function exceeds the hard limits (CC > 15 or NLOC > 100).
"""
import argparse
import json
import os
import re
import sys

CC_WARN, CC_FAIL = 10, 15
NLOC_WARN, NLOC_FAIL = 50, 100
CONTROL = {"if", "for", "while", "switch", "catch"}
NOT_FUNCTION_BEFORE_BRACE = {"namespace", "class", "struct", "enum", "union", "else", "do", "try"}
DECISION_WORDS = {"if", "for", "while", "case", "catch"}


def strip_code(text):
    """Remove comments and string/char literals, keeping line breaks. Returns (clean_text, comment_line_flags)."""
    out = []
    i, n = 0, len(text)
    line = 1
    has_comment = {}  # line -> True if a comment touches this line
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            has_comment[line] = True
            while i < n and text[i] != "\n":
                i += 1
        elif c == "/" and nxt == "*":
            has_comment[line] = True
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                if text[i] == "\n":
                    out.append("\n")
                    line += 1
                    has_comment[line] = True
                i += 1
            i += 2
        elif c == '"' or c == "'":
            quote = c
            out.append(quote)
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    i += 1
                if i < n and text[i] == "\n":
                    out.append("\n")
                    line += 1
                i += 1
            out.append(quote)
            i += 1
        else:
            out.append(c)
            if c == "\n":
                line += 1
            i += 1
    return "".join(out), has_comment


TOKEN = re.compile(r"[A-Za-z_~][A-Za-z_0-9]*|::|&&|\|\||[{}()\[\];,:?=<>]|[^\sA-Za-z_0-9]")


def tokenize(clean):
    tokens = []
    line = 1
    pos = 0
    for m in re.finditer(r"\n|" + TOKEN.pattern, clean):
        s = m.group(0)
        if s == "\n":
            line += 1
        else:
            tokens.append((s, line))
    return tokens


def find_functions(tokens):
    """Yield (name, start_line, end_line, cc) for every function body found outside another function."""
    functions = []
    i, n = 0, len(tokens)
    while i < n:
        tok, ln = tokens[i]
        if tok != "{":
            i += 1
            continue
        # look back for the closing parenthesis of a parameter list (skipping const/noexcept/override/final,
        # trailing return types are not used in this code base) or the end of a constructor initialiser list
        j = i - 1
        while j >= 0 and tokens[j][0] in ("const", "noexcept", "override", "final", "&", "&&"):
            j -= 1
        is_function = j >= 0 and tokens[j][0] == ")"
        if is_function:
            # walk back to the matching "(" of this ")"
            depth, k = 0, j
            while k >= 0:
                if tokens[k][0] == ")":
                    depth += 1
                elif tokens[k][0] == "(":
                    depth -= 1
                    if depth == 0:
                        break
                k -= 1
            head = tokens[k - 1][0] if k >= 1 else ""
            if head in CONTROL or head in ("]",) or k < 1:
                is_function = False
        if is_function:
            # constructor with initialiser list: ") : a_(x), b_(y) {" -> the head found above is the last
            # initialiser. Walk back over "name ( ... ) ," groups to the real parameter list.
            while True:
                m = k - 2
                if m >= 0 and tokens[m][0] in (",", ":") and tokens[m][0] != "::":
                    # step over the previous "name(...)" group or the ":" itself
                    if tokens[m][0] == ":":
                        # the group before ":" is the parameter list (possibly followed by const etc.)
                        p = m - 1
                        while p >= 0 and tokens[p][0] in ("const", "noexcept"):
                            p -= 1
                        if p >= 0 and tokens[p][0] == ")":
                            depth, k = 0, p
                            while k >= 0:
                                if tokens[k][0] == ")":
                                    depth += 1
                                elif tokens[k][0] == "(":
                                    depth -= 1
                                    if depth == 0:
                                        break
                                k -= 1
                            head = tokens[k - 1][0]
                        break
                    # "," -> previous initialiser "name(...)" or "name{...}"
                    p = m - 1
                    if tokens[p][0] == ")":
                        depth, k = 0, p
                        while k >= 0:
                            if tokens[k][0] == ")":
                                depth += 1
                            elif tokens[k][0] == "(":
                                depth -= 1
                                if depth == 0:
                                    break
                            k -= 1
                        head = tokens[k - 1][0]
                        continue
                    break
                break
        if not is_function:
            i += 1
            continue
        # match the closing brace
        depth, e = 0, i
        while e < n:
            if tokens[e][0] == "{":
                depth += 1
            elif tokens[e][0] == "}":
                depth -= 1
                if depth == 0:
                    break
            e += 1
        body = tokens[i:e + 1]
        # name: identifier before "(" plus any qualification
        name_idx = k - 1
        parts = [tokens[name_idx][0]]
        q = name_idx - 1
        while q >= 1 and tokens[q][0] == "::":
            parts.insert(0, tokens[q - 1][0])
            q -= 2
        if q >= 0 and tokens[q][0] == "~":
            parts[-1] = "~" + parts[-1]
        name = "::".join(parts)
        start_line = tokens[name_idx][1]
        # the return type can sit on the line above; it is not counted (matches lizard)
        cc = 1
        for t, _ in body:
            if t in DECISION_WORDS or t in ("&&", "||", "?"):
                cc += 1
        # a function head may also contain default member init decision points; ignore
        functions.append((name, start_line, tokens[e][1], cc))
        i = e + 1
    return functions


def analyse_file(path):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    clean, has_comment = strip_code(text)
    lines = clean.split("\n")
    physical = len(text.split("\n")) - (1 if text.endswith("\n") else 0)
    blank = code = comment = 0
    code_lines = set()
    for idx, raw in enumerate(lines[:physical], start=1):
        if raw.strip():
            code += 1
            code_lines.add(idx)
        elif not text.split("\n")[idx - 1].strip():
            blank += 1
        else:
            comment += 1
    funcs = []
    for name, s, e, cc in find_functions(tokenize(clean)):
        nloc = sum(1 for ln in range(s, e + 1) if ln in code_lines)
        funcs.append({"name": name, "start": s, "end": e, "nloc": nloc, "cc": cc})
    return {"physical": physical, "blank": blank, "comment": comment, "code": code, "functions": funcs}


def collect(root):
    results = {}
    for base, _, names in os.walk(root):
        for name in sorted(names):
            if name.endswith((".cpp", ".hpp", ".h", ".cc")):
                path = os.path.join(base, name)
                results[os.path.relpath(path)] = analyse_file(path)
    return dict(sorted(results.items()))


def layer_of(path):
    parts = path.split(os.sep)
    return parts[1] if len(parts) > 2 and parts[0] == "src" else parts[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default="src")
    ap.add_argument("--tests", default="tests")
    ap.add_argument("--out")
    ap.add_argument("--json")
    args = ap.parse_args()

    src = collect(args.src)
    tests = collect(args.tests) if os.path.isdir(args.tests) else {}

    md = []
    emit = md.append
    emit("# Metrics (generated by tools/metrics.py)\n")

    # ---- size by layer
    layers = {}
    for path, r in src.items():
        d = layers.setdefault(layer_of(path), {"files": 0, "code": 0, "comment": 0, "blank": 0, "functions": 0})
        d["files"] += 1
        d["code"] += r["code"]
        d["comment"] += r["comment"]
        d["blank"] += r["blank"]
        d["functions"] += len(r["functions"])
    emit("## Size of the production code (src/)\n")
    emit("| Layer | Files | Code lines | Comment lines | Blank lines | Functions |")
    emit("|---|---:|---:|---:|---:|---:|")
    tot = {"files": 0, "code": 0, "comment": 0, "blank": 0, "functions": 0}
    for name, d in layers.items():
        emit(f"| {name} | {d['files']} | {d['code']} | {d['comment']} | {d['blank']} | {d['functions']} |")
        for k in tot:
            tot[k] += d[k]
    emit(f"| **Total** | **{tot['files']}** | **{tot['code']}** | **{tot['comment']}** | **{tot['blank']}** | **{tot['functions']}** |\n")
    tcode = sum(r["code"] for r in tests.values())
    if tests:
        emit(f"Test code (tests/): {len(tests)} files, {tcode} code lines, "
             f"{sum(len(r['functions']) for r in tests.values())} functions. "
             f"Test code to production code ratio: {tcode / max(tot['code'], 1):.2f}.\n")

    # ---- complexity overall
    allf = [(p, f) for p, r in src.items() for f in r["functions"]]
    ccs = [f["cc"] for _, f in allf]
    emit("## Cyclomatic complexity of the production code\n")
    emit(f"Functions measured: {len(allf)}. Average CC: {sum(ccs) / max(len(ccs), 1):.2f}. Maximum CC: {max(ccs) if ccs else 0}.\n")
    bands = [("1 to 5 (simple)", 1, 5), ("6 to 10 (moderate)", 6, 10), ("11 to 15 (complex)", 11, 15), ("above 15 (too complex)", 16, 10**9)]
    emit("| Complexity band | Functions | Share |")
    emit("|---|---:|---:|")
    for label, lo, hi in bands:
        c = sum(1 for v in ccs if lo <= v <= hi)
        emit(f"| {label} | {c} | {100 * c / max(len(ccs), 1):.1f}% |")
    emit("")

    # ---- per file
    emit("## Per file\n")
    emit("| File | Code lines | Functions | Average CC | Max CC |")
    emit("|---|---:|---:|---:|---:|")
    for path, r in src.items():
        fc = [f["cc"] for f in r["functions"]]
        avg = f"{sum(fc) / len(fc):.1f}" if fc else "-"
        emit(f"| {path} | {r['code']} | {len(fc)} | {avg} | {max(fc) if fc else '-'} |")
    emit("")

    # ---- top functions
    emit("## The 15 most complex functions\n")
    emit("| Function | File | CC | NLOC |")
    emit("|---|---|---:|---:|")
    for p, f in sorted(allf, key=lambda x: (-x[1]["cc"], -x[1]["nloc"]))[:15]:
        emit(f"| {f['name']} | {p}:{f['start']} | {f['cc']} | {f['nloc']} |")
    emit("")
    emit("## The 10 longest functions\n")
    emit("| Function | File | NLOC | CC |")
    emit("|---|---|---:|---:|")
    for p, f in sorted(allf, key=lambda x: (-x[1]["nloc"], -x[1]["cc"]))[:10]:
        emit(f"| {f['name']} | {p}:{f['start']} | {f['nloc']} | {f['cc']} |")
    emit("")

    over_cc = [(p, f) for p, f in allf if f["cc"] > CC_WARN]
    over_nloc = [(p, f) for p, f in allf if f["nloc"] > NLOC_WARN]
    emit(f"## Limits\n")
    emit(f"Warning limits: CC > {CC_WARN}, NLOC > {NLOC_WARN}. Hard limits (fail): CC > {CC_FAIL}, NLOC > {NLOC_FAIL}.\n")
    emit(f"Functions above a warning limit: {len({(p, f['start']) for p, f in over_cc + over_nloc})}.")
    for p, f in over_cc:
        emit(f"- CC {f['cc']} > {CC_WARN}: {f['name']} ({p}:{f['start']})")
    for p, f in over_nloc:
        emit(f"- NLOC {f['nloc']} > {NLOC_WARN}: {f['name']} ({p}:{f['start']})")

    text = "\n".join(md) + "\n"
    print(text)
    if args.out:
        with open(args.out, "w", encoding="utf-8") as fh:
            fh.write(text)
    if args.json:
        with open(args.json, "w", encoding="utf-8") as fh:
            json.dump({"src": src, "tests": tests}, fh, indent=1)
    failed = [x for x in allf if x[1]["cc"] > CC_FAIL or x[1]["nloc"] > NLOC_FAIL]
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
