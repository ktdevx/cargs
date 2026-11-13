#!/usr/bin/env bash
# Build and run the complete C examples of the human-facing documents.
#
# Usage: build_examples.sh <repo-root> <outdir>
#
#  1. Configures, builds and installs the library with the tests disabled
#     (CLARGS_BUILD_TESTS=OFF, so Naos is not fetched) into <outdir>/prefix.
#  2. Extracts the examples with `check_docs.py extract <outdir>/examples`.
#  3. Compiles and links every `main` example with gcc and
#     -std=c99 -Wall -Wextra -Wpedantic -Werror; compiles every `wmain`
#     example only (-c). The allocator example of docs/ownership.md is linked
#     with <outdir>/user_alloc.c, which defines my_malloc, my_realloc,
#     my_calloc and my_free.
#  4. Runs every `main` example and compares it with its document:
#     - the next fenced block after the example (before the next heading and
#       the next C block), if it has no info string:
#         * lines starting with "$ " -> a transcript: each "$ <command line>"
#           is run (argv split like a shell would), and its output must equal
#           the lines up to the next "$ " line (one blank separator line
#           removed). An output whose first line starts with "<program>: " is
#           an error: it must be on stderr with exit status 1 and nothing on
#           stdout. Any other output must be on stdout with exit status 0 and
#           nothing on stderr.
#         * no "$ " lines -> the output of running with no arguments; it must
#           be on stdout, with exit status 0 and nothing on stderr.
#     - otherwise the example is run with no arguments and must exit with 0 and print nothing,
#       except docs/schema.md, which must print exactly
#       "the schema of pack is valid".
#
#  5. Checks that nothing went missing, each as a PASS/FAIL line:
#     - the counted examples (all but those of docs/versioning.md and
#       docs/releasing.md) are exactly: README.md 1 main, docs/schema.md
#       1 main, docs/parse.md 1 main and 1 wmain, docs/help.md 2 main,
#       docs/ownership.md 1 main (7 in all);
#     - the transcript after the README example has exactly the command lines
#       `greet --loud -n 2 world`, `greet`, `greet --count=many world` and
#       `greet --help`, in this order;
#     - every example that has an output block ran at least one comparison.
#
# Prints one PASS/FAIL line per check (with a diff on FAIL) and exits with 1
# if any check failed. Examples extracted from docs/versioning.md or
# docs/releasing.md are built and run the same way but reported as
# release-automation findings and not counted as failures.
set -u

if [ $# -ne 2 ]; then
    echo "usage: $0 <repo-root> <outdir>" >&2
    exit 2
fi
root=$(cd "$1" && pwd) || exit 2
mkdir -p "$2" || exit 2
out=$(cd "$2" && pwd) || exit 2

set -e
cmake -S "$root" -B "$out/build" -DCMAKE_BUILD_TYPE=Release -DCLARGS_BUILD_TESTS=OFF \
    > "$out/configure.log"
cmake --build "$out/build" -j8 > "$out/build.log"
cmake --install "$out/build" --prefix "$out/prefix" > "$out/install.log"
rm -rf "$out/examples" "$out/bin"
python3 "$root/.kiro/specs/english-docs/check_docs.py" extract "$out/examples" --root "$root"
set +e

cat > "$out/user_alloc.c" <<'EOF'
#include <stdlib.h>

void *my_malloc(size_t size);
void *my_realloc(void *ptr, size_t size);
void *my_calloc(size_t count, size_t size);
void my_free(void *ptr);

void *my_malloc(size_t size) { return malloc(size); }
void *my_realloc(void *ptr, size_t size) { return realloc(ptr, size); }
void *my_calloc(size_t count, size_t size) { return calloc(count, size); }
void my_free(void *ptr) { free(ptr); }
EOF

PYTHONDONTWRITEBYTECODE=1 python3 - "$root" "$out" <<'PY'
import collections, difflib, os, re, shlex, subprocess, sys

root, out = sys.argv[1], sys.argv[2]
sys.path.insert(0, os.path.join(root, ".kiro/specs/english-docs"))
import check_docs  # noqa: E402

prefix = os.path.join(out, "prefix")
libdir = next(os.path.join(prefix, d) for d in ("lib", "lib64")
              if os.path.isdir(os.path.join(prefix, d)))
cflags = ["-std=c99", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
          "-I", os.path.join(prefix, "include")]
bindir = os.path.join(out, "bin")
os.makedirs(bindir, exist_ok=True)
RELEASE_DOCS = ("docs/versioning.md", "docs/releasing.md")
EXPECT_NO_ARGS_TEXT = {"docs/schema.md": "the schema of pack is valid\n"}
EXPECTED_EXAMPLES = collections.Counter({
    ("README.md", "main"): 1, ("docs/schema.md", "main"): 1,
    ("docs/parse.md", "main"): 1, ("docs/parse.md", "wmain"): 1,
    ("docs/help.md", "main"): 2, ("docs/ownership.md", "main"): 1,
})
README_COMMANDS = ["greet --loud -n 2 world", "greet", "greet --count=many world",
                   "greet --help"]

failures, ra_findings, passes = [], [], 0


def report(ok, doc, what, detail=""):
    global passes
    tag = "PASS" if ok else ("FAIL (release-automation)" if doc in RELEASE_DOCS else "FAIL")
    print("%s: %s" % (tag, what))
    if not ok:
        if detail:
            print(detail.rstrip("\n"))
        (ra_findings if doc in RELEASE_DOCS else failures).append(what)
    else:
        passes += 1


def output_block(doc, fence_line):
    """The untagged block that shows the output of the example at fence_line."""
    with open(os.path.join(root, doc), encoding="utf-8") as f:
        lines = f.read().splitlines()
    # The example block starts at fence_line; take the block after it.
    nxt = [b for b in check_docs.code_blocks(lines) if b[0] > fence_line]
    if not nxt:
        return None
    start, info, body = nxt[0]
    between = lines[fence_line:start - 1]
    if info or any(check_docs.HEADING_RE.match(l) for l in between):
        return None
    return body


def transcript(body):
    runs, cur = [], None
    for line in body:
        if line.startswith("$ "):
            cur = (line[2:], [])
            runs.append(cur)
        elif cur is not None:
            cur[1].append(line)
    for _, lines in runs:
        if lines and lines[-1] == "":
            lines.pop()
    return [(cmd, "".join(l + "\n" for l in lines)) for cmd, lines in runs]


def diff(expected, actual):
    return "".join(difflib.unified_diff(expected.splitlines(True), actual.splitlines(True),
                                        "documented", "actual"))


def check_run(doc, name, exe, args, expected, error, label):
    global comparisons
    comparisons += 1
    p = subprocess.run([exe] + args, capture_output=True, text=True)
    want_status, want_stream = (1, "stderr") if error else (0, "stdout")
    got = p.stderr if error else p.stdout
    other = p.stdout if error else p.stderr
    ok = (expected is None or got == expected) and p.returncode == want_status and other == ""
    detail = "  exit status %d (want %d); %s:\n%s  other stream: %r\n%s" % (
        p.returncode, want_status, want_stream, got, other,
        diff(expected, got) if expected is not None else "")
    report(ok, doc, "%s run `%s` (exit %d on %s)" % (name, label, p.returncode, want_stream),
           detail)


with open(os.path.join(out, "examples", "manifest.txt"), encoding="utf-8") as f:
    manifest = [l.split() for l in f if l.strip()]

counted = [m for m in manifest if m[2].rsplit(":", 1)[0] not in RELEASE_DOCS]
print("examples: %d (counted: %d)" % (len(manifest), len(counted)))

found = collections.Counter((m[2].rsplit(":", 1)[0], m[1]) for m in counted)
report(found == EXPECTED_EXAMPLES, "", "the counted examples are exactly the expected 7",
       "  expected: %s\n  found:    %s" % (sorted(EXPECTED_EXAMPLES.items()),
                                          sorted(found.items())))
readme_commands = None

for name, entry, source in manifest:
    doc, fence_line = source.rsplit(":", 1)
    fence_line = int(fence_line)
    src = os.path.join(out, "examples", name)
    if entry == "wmain":
        obj = os.path.join(bindir, name[:-2] + ".o")
        p = subprocess.run(["gcc"] + cflags + ["-c", src, "-o", obj],
                           capture_output=True, text=True)
        report(p.returncode == 0, doc, "%s (%s) compiles (wmain, -c)" % (name, source),
               p.stderr)
        continue
    body = output_block(doc, fence_line)
    runs = transcript(body) if body is not None else []
    if doc == "README.md":
        readme_commands = [cmd for cmd, _ in runs]
    exe = os.path.join(bindir, name[:-2])
    extra = [os.path.join(out, "user_alloc.c")] if doc == "docs/ownership.md" else []
    p = subprocess.run(["gcc"] + cflags + [src] + extra +
                       ["-L", libdir, "-lclargs", "-o", exe], capture_output=True, text=True)
    report(p.returncode == 0, doc, "%s (%s) compiles and links" % (name, source), p.stderr)
    if p.returncode != 0:
        continue
    comparisons = 0
    if runs:
        for cmd, expected in runs:
            words = shlex.split(cmd)
            error = expected.startswith(words[0] + ": ")
            check_run(doc, name, exe, words[1:], expected, error, cmd)
    elif body is not None:
        check_run(doc, name, exe, [], "".join(l + "\n" for l in body), False, "(no arguments)")
    else:
        check_run(doc, name, exe, [], EXPECT_NO_ARGS_TEXT.get(doc, ""), False, "(no arguments)")
    if body is not None:
        report(comparisons > 0, doc, "%s (%s) ran %d comparison(s) with its output block"
               % (name, source, comparisons))

report(readme_commands == README_COMMANDS, "README.md",
       "the README transcript has exactly the 4 documented command lines",
       "  expected: %r\n  found:    %r" % (README_COMMANDS, readme_commands))

print("\nchecks passed: %d, failed: %d, release-automation findings: %d"
      % (passes, len(failures), len(ra_findings)))
for f in failures:
    print("  FAILED: " + f)
for f in ra_findings:
    print("  release-automation: " + f)
sys.exit(1 if failures else 0)
PY
