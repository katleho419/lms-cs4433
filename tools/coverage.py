#!/usr/bin/env python3
"""Line-coverage report from gcov, grouped by layer.

Usage:  python3 tools/coverage.py BUILD_DIR [SOURCE_DIR]
Needs a build made with -DLMS_ENABLE_COVERAGE=ON and the tests already run.
Prints a table and writes BUILD_DIR/coverage_report.md.
"""
import gzip, json, os, subprocess, sys, glob, collections

build = os.path.abspath(sys.argv[1])
src_root = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), ".."))
src_dir = os.path.join(src_root, "src") + os.sep

# file -> line -> execution count (merged over all object files that include it)
lines = collections.defaultdict(lambda: collections.defaultdict(int))
for gcda in glob.glob(os.path.join(build, "**", "*.gcda"), recursive=True):
    out = subprocess.run(["gcov", "--json-format", "--stdout", gcda], cwd=os.path.dirname(gcda),
                         capture_output=True, text=True)
    for chunk in out.stdout.splitlines():
        if not chunk.strip().startswith("{"):
            continue
        data = json.loads(chunk)
        for f in data.get("files", []):
            path = os.path.normpath(os.path.join(os.path.dirname(gcda), f["file"])) if not os.path.isabs(f["file"]) else f["file"]
            path = os.path.realpath(path)
            if not path.startswith(src_dir):
                continue
            for ln in f["lines"]:
                lines[path][ln["line_number"]] += ln["count"]

layers = collections.OrderedDict([
    ("util", "util"), ("security", "security"), ("domain", "domain"), ("service", "service"),
    ("data", "data"), ("ui", "ui"), ("app", "app")])
business = {"util", "security", "domain", "service"}

def layer_of(path):
    return os.path.relpath(path, src_dir).split(os.sep)[0]

per_file = []
for path, ls in sorted(lines.items()):
    total = len(ls)
    hit = sum(1 for c in ls.values() if c > 0)
    per_file.append((os.path.relpath(path, src_dir), layer_of(path), hit, total))

def pct(h, t):
    return 100.0 * h / t if t else 100.0

rows = ["| File | Lines | Covered | Coverage |", "|---|---:|---:|---:|"]
for name, layer, h, t in per_file:
    rows.append(f"| src/{name} | {t} | {h} | {pct(h, t):.1f}% |")

summary = ["| Layer | Lines | Covered | Coverage |", "|---|---:|---:|---:|"]
for layer in layers:
    h = sum(x[2] for x in per_file if x[1] == layer)
    t = sum(x[3] for x in per_file if x[1] == layer)
    if t:
        summary.append(f"| {layer} | {t} | {h} | {pct(h, t):.1f}% |")
bh = sum(x[2] for x in per_file if x[1] in business)
bt = sum(x[3] for x in per_file if x[1] in business)
ah = sum(x[2] for x in per_file)
at = sum(x[3] for x in per_file)
summary.append(f"| **Business logic (util, security, domain, service)** | {bt} | {bh} | **{pct(bh, bt):.1f}%** |")
summary.append(f"| All measured source | {at} | {ah} | {pct(ah, at):.1f}% |")

report = "# Line coverage\n\n## By layer\n\n" + "\n".join(summary) + "\n\n## By file\n\n" + "\n".join(rows) + "\n"
open(os.path.join(build, "coverage_report.md"), "w").write(report)
print(report)
status = pct(bh, bt) >= 70
print("Target: business-logic line coverage >= 70%%.  Result: %.1f%% -> %s" % (pct(bh, bt), "MET" if pct(bh, bt) >= 70 else "NOT MET"))
sys.exit(0 if status else 1)
