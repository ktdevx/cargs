"""Tests for check_docs.py (modes `check` and `extract`)."""
import sys

# Keep the spec directory free of __pycache__, even if this file is imported.
sys.dont_write_bytecode = True

import os  # noqa: E402
import subprocess  # noqa: E402
import tempfile
import textwrap
import unittest

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "check_docs.py")


def write(root, rel, text, binary=False):
    path = os.path.join(root, rel)
    os.makedirs(os.path.dirname(path) or root, exist_ok=True)
    mode = "wb" if binary else "w"
    with open(path, mode, **({} if binary else {"encoding": "utf-8"})) as f:
        f.write(text if binary else textwrap.dedent(text))


def make_repo(files, untracked=()):
    root = tempfile.mkdtemp(prefix="checkdocs-")
    for rel, text in files.items():
        write(root, rel, text, binary=isinstance(text, bytes))
    for rel, text in untracked:
        write(root, rel, text)
    subprocess.run(["git", "init", "-q", root], check=True)
    subprocess.run(["git", "-C", root, "add"] + list(files), check=True)
    return root


def run(root, *args):
    p = subprocess.run([sys.executable, SCRIPT, "check", "--root", root] + list(args),
                       capture_output=True, text=True)
    return p


def findings(stdout):
    out = []
    for line in stdout.splitlines():
        parts = line.split(": ", 2)
        if len(parts) == 3 and ":" in parts[0]:
            path, lineno = parts[0].rsplit(":", 1)
            if lineno.isdigit():
                out.append((path, int(lineno), parts[1]))
    return out


CLEAN = {
    "README.md": """\
        # Title

        See [Usage](#usage) and [parse](docs/parse.md#the-parse-function).
        The specification is specific. Specifications too.

        ## Usage

        Text.
        """,
    "docs/parse.md": """\
        # Parsing

        ## The parse function

        Back to [the README](../README.md).
        """,
    "src/a.c": "/* ascii only ── box drawing */\nint x;\n",
}


class CheckTests(unittest.TestCase):
    def test_clean_repo_exits_zero(self):
        root = make_repo(CLEAN)
        p = run(root)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(findings(p.stdout), [])
        self.assertIn("summary", p.stdout.lower())

    def test_japanese_lines_all_files_not_box_drawing(self):
        files = dict(CLEAN)
        files["src/b.c"] = "int y; /* 日本語 */\n/* ─│┌ */\n"
        files["docs/schema.md"] = "# S\n\nfull width！\nkatakana カ\n"
        root = make_repo(files)
        p = run(root)
        self.assertEqual(p.returncode, 1)
        f = findings(p.stdout)
        self.assertIn(("src/b.c", 1, "japanese"), f)
        self.assertNotIn(("src/b.c", 2, "japanese"), f)
        self.assertNotIn(("src/a.c", 1, "japanese"), f)
        self.assertIn(("docs/schema.md", 3, "japanese"), f)
        self.assertIn(("docs/schema.md", 4, "japanese"), f)

    def test_kiro_in_any_file_and_excluded_dirs(self):
        files = dict(CLEAN)
        files["src/c.c"] = "/* see .KIRO/x */\n"
        files[".kiro/specs/x/tasks.md"] = "日本 .kiro spec\n"
        files[".claude/skills/a/SKILL.md"] = "日本 .kiro\n"
        files["CLAUDE.md"] = "日本 .kiro\n"
        root = make_repo(files)
        f = findings(run(root).stdout)
        self.assertIn(("src/c.c", 1, "kiro"), f)
        for path, _, _ in f:
            self.assertFalse(path.startswith((".kiro/", ".claude/")) or path == "CLAUDE.md", path)

    def test_spec_term_word_boundary(self):
        files = dict(CLEAN)
        files["docs/x.md"] = textwrap.dedent("""\
            # X
            the specification
            specifications and specific
            a spec here
            the specs
            build-foundation
            steering
            Kiro steps
            src/spec.c is not mentioned
            """)
        files["src/d.c"] = "/* spec in source is not a doc */\n"
        root = make_repo(files)
        f = [x for x in findings(run(root).stdout) if x[2] == "spec-term"]
        lines = sorted(l for p, l, _ in f if p == "docs/x.md")
        self.assertEqual(lines, [4, 5, 6, 7, 8, 9])
        self.assertFalse([x for x in f if x[0] != "docs/x.md"])

    def test_missing_file_and_pending(self):
        files = dict(CLEAN)
        files["docs/y.md"] = textwrap.dedent("""\
            # Y
            [a](nope.md) [b](../CHANGELOG.md) [c](versioning.md#x)
            [d](https://example.com/nope) [e](mailto:a@b.c)
            [f](../src/) [g](parse.md)
            [h]: ../missing/dir/
            `[i](inline-code.md)`
            """)
        root = make_repo(files)
        p = run(root)
        f = findings(p.stdout)
        self.assertIn(("docs/y.md", 2, "missing-file"), f)
        self.assertIn(("docs/y.md", 5, "missing-file"), f)
        pend = [x for x in f if x[2] == "pending"]
        self.assertEqual(len(pend), 2, p.stdout)
        self.assertFalse([x for x in f if x[1] in (3, 4, 6)], p.stdout)
        self.assertEqual(p.returncode, 1)

    def test_pending_only_exits_zero(self):
        files = dict(CLEAN)
        files["docs/z.md"] = "# Z\n[r](releasing.md)\n"
        root = make_repo(files)
        p = run(root)
        self.assertEqual(p.returncode, 0, p.stdout)
        self.assertEqual([x[2] for x in findings(p.stdout)], ["pending"])

    def test_anchors(self):
        files = dict(CLEAN)
        files["docs/a.md"] = textwrap.dedent("""\
            # Anchors

            ## Usage
            ## Usage
            ## `--help` and errors
            ## 確保関数の検査 `cline.allocations`
            ## 6. Whoever allocates, frees

            ```c
            # Hidden heading
            ```

            ~~~
            ## Also hidden
            ~~~

            [ok1](#usage) [ok2](#usage-1) [ok3](#--help-and-errors)
            [ok4](#確保関数の検査-clineallocations)
            [ok5](#6-whoever-allocates-frees) [ok6](a.md#usage-1)
            [bad1](#usage-2) [bad2](#hidden-heading) [bad3](#also-hidden)
            [ok7](#%E7%A2%BA%E4%BF%9D%E9%96%A2%E6%95%B0%E3%81%AE%E6%A4%9C%E6%9F%BB-clineallocations)
            [bad4](../README.md#nope) [ok8](../README.md#usage)
            [ok9](../src/a.c#L1)
            """)
        root = make_repo(files)
        p = run(root)
        f = [x for x in findings(p.stdout) if x[0] == "docs/a.md"]
        anchors = [x for x in f if x[2] == "missing-anchor"]
        self.assertEqual(sorted(l for _, l, _ in anchors), [20, 20, 20, 22], p.stdout)
        # line 6 and 18 contain Japanese characters; they must be reported as japanese
        self.assertIn(("docs/a.md", 6, "japanese"), f)

    def test_files_option_limits_reports(self):
        files = dict(CLEAN)
        files["docs/p.md"] = "# P\n日本\n[x](parse.md#the-parse-function)\n"
        files["docs/q.md"] = "# Q\n日本\n[x](p.md#nope)\n"
        root = make_repo(files)
        p = run(root, "--files", "docs/p.md")
        f = findings(p.stdout)
        self.assertEqual({x[0] for x in f}, {"docs/p.md"})
        self.assertEqual(p.returncode, 1)
        p = run(root, "--files", "src/a.c")
        self.assertEqual(p.returncode, 0, p.stdout)
        self.assertEqual(findings(p.stdout), [])

    def test_summary_split_release_automation(self):
        files = dict(CLEAN)
        files["docs/versioning.md"] = "# V\n日本\n"
        files["tools/release/x.sh"] = "# .kiro\n"
        files[".github/workflows/ci.yml"] = "# 日\n"
        files["tests/release/t.cmake"] = "# 日\n"
        files["CHANGELOG.md"] = "# C\nspec\n"
        files["docs/help.md"] = "# H\n日本\n"
        root = make_repo(files)
        p = run(root)
        self.assertEqual(p.returncode, 1)
        out = p.stdout.lower()
        # Expect a line counting release-automation findings (5) separately from 1 for this spec
        ra = [l for l in out.splitlines() if "release-automation" in l and "total" in l]
        es = [l for l in out.splitlines() if "english-docs" in l and "total" in l]
        self.assertTrue(ra and es, p.stdout)
        # CHANGELOG.md is not a human-facing document, so no spec-term there
        self.assertIn("total: 4 ", ra[0])
        self.assertIn("total: 1 ", es[0])

    def test_binary_skipped_and_untracked_ignored(self):
        files = dict(CLEAN)
        files["img.bin"] = b"\xff\xfe\x00\x81"
        root = make_repo(files, untracked=[("docs/untracked.md", "日本\n")])
        p = run(root)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertIn("img.bin", p.stdout + p.stderr)

    def test_idempotent(self):
        files = dict(CLEAN)
        files["docs/x.md"] = "# X\n日\n[a](#nope)\n"
        root = make_repo(files)
        self.assertEqual(run(root).stdout, run(root).stdout)

    def test_read_only(self):
        root = make_repo(CLEAN)
        before = subprocess.run(["git", "-C", root, "status", "--porcelain", "--ignored"],
                                capture_output=True, text=True).stdout
        run(root)
        after = subprocess.run(["git", "-C", root, "status", "--porcelain", "--ignored"],
                               capture_output=True, text=True).stdout
        self.assertEqual(before, after)

    def test_files_outside_target_set_exit_two(self):
        root = make_repo(CLEAN, untracked=[("CONTRIBUTING.md", "\u65e5\u672c\n")])
        for path in ("CONTRIBUTING.md", "README.mdd", ".kiro/x.md"):
            p = run(root, "--files", path)
            self.assertEqual(p.returncode, 2, path + p.stdout + p.stderr)
            self.assertIn(path, p.stderr)
            self.assertIn("git add -N", p.stderr)
        p = run(root, "--files", "README.md", "README.mdd")
        self.assertEqual(p.returncode, 2)
        subprocess.run(["git", "-C", root, "add", "-N", "CONTRIBUTING.md"], check=True)
        p = run(root, "--files", "CONTRIBUTING.md")
        self.assertEqual(p.returncode, 1, p.stdout + p.stderr)
        self.assertIn(("CONTRIBUTING.md", 1, "japanese"), findings(p.stdout))

    def test_fragment_case_and_code_span_heading(self):
        files = dict(CLEAN)
        files["docs/c.md"] = textwrap.dedent("""\
            # C
            ## Sec One
            ## `f(<name>)` and <b>bold</b>
            [bad](#Sec-One) [ok](#sec-one)
            [ok2](#fname-and-bold)
            """)
        root = make_repo(files)
        p = run(root)
        f = [x for x in findings(p.stdout) if x[0] == "docs/c.md"]
        self.assertEqual(f, [("docs/c.md", 4, "missing-anchor")], p.stdout)

    def test_release_automation_classification(self):
        files = dict(CLEAN)
        for path in ("cmake/x.cmake", "CMakeLists.txt", "tests/CMakeLists.txt",
                     "tests/consumer/a.c", "tests/public/version_test.c",
                     "include/cline/version.h"):
            files[path] = "# \u65e5\n"
        files["tests/public/other_test.c"] = "/* \u65e5 */\n"
        files["src/CMakeLists.txt"] = "# \u65e5\n"
        root = make_repo(files)
        p = run(root)
        self.assertEqual(p.returncode, 1)
        lines = p.stdout.lower().splitlines()
        ra = [l for l in lines if "release-automation total" in l]
        es = [l for l in lines if "english-docs total" in l]
        self.assertIn("total: 6 ", ra[0], p.stdout)
        self.assertIn("total: 2 ", es[0], p.stdout)

    def test_pending_targets_unchanged(self):
        files = dict(CLEAN)
        files["docs/m.md"] = "# M\n[a](../CMakeLists.txt) [b](../cmake/)\n"
        root = make_repo(files)
        f = findings(run(root).stdout)
        self.assertEqual([x[2] for x in f if x[0] == "docs/m.md"], ["missing-file", "missing-file"])


def run_extract(root, outdir, *args):
    return subprocess.run([sys.executable, SCRIPT, "extract", outdir, "--root", root] + list(args),
                          capture_output=True, text=True)


EXTRACT_DOCS = {
    "README.md": """\
        # T

        ```sh
        cc main.c
        ```

        ```c
        #include <stdio.h>

        int main(int argc, char **argv)
        {
            (void)argc; (void)argv;
            return 0;
        }
        ```
        """,
    "docs/parse.md": """\
        # P

        ```c
        /* a fragment that mentions main() in a comment only */
        int f(void);
        ```

        ```c
        int wmain(int argc, wchar_t **argv)
        {
            return 0;
        }
        ```

        - item

          ```c
          int main(void)
          {
              return 1;
          }
          ```

        ```cmake
        # int main(void) {
        ```

        ~~~c
        static int helper(void) { return 0; }
        int
        main(void)
        {
            return helper();
        }
        ~~~

        ```
        int main(void) { return 0; }
        ```
        """,
    "docs/versioning.md": """\
        # V

        ```c
        int main(void) { return 0; }
        ```
        """,
    "src/notes.md": "```c\nint main(void) { return 0; }\n```\n",
    ".kiro/specs/x/design.md": "```c\nint main(void) { return 0; }\n```\n",
    "CLAUDE.md": "```c\nint main(void) { return 0; }\n```\n",
}


class ExtractTests(unittest.TestCase):
    def setUp(self):
        self.root = make_repo(EXTRACT_DOCS, untracked=[
            ("docs/untracked.md", "```c\nint main(void) { return 0; }\n```\n")])
        self.out = tempfile.mkdtemp(prefix="extract-")

    def manifest(self):
        with open(os.path.join(self.out, "manifest.txt"), encoding="utf-8") as f:
            return f.read().splitlines()

    def test_extracts_main_and_wmain_blocks_with_manifest(self):
        p = run_extract(self.root, self.out)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(self.manifest(), [
            "README.md-1.c main README.md:7",
            "docs_parse.md-2.c wmain docs/parse.md:8",
            "docs_parse.md-3.c main docs/parse.md:17",
            "docs_parse.md-4.c main docs/parse.md:28",
            "docs_versioning.md-1.c main docs/versioning.md:3",
        ])
        files = sorted(f for f in os.listdir(self.out) if f.endswith(".c"))
        self.assertEqual(files, ["README.md-1.c", "docs_parse.md-2.c", "docs_parse.md-3.c",
                                 "docs_parse.md-4.c", "docs_versioning.md-1.c"])
        self.assertIn("5", p.stdout)

    def test_block_contents_are_exact_and_list_indent_is_removed(self):
        run_extract(self.root, self.out)
        with open(os.path.join(self.out, "README.md-1.c"), encoding="utf-8") as f:
            self.assertEqual(f.read(), "#include <stdio.h>\n\nint main(int argc, char **argv)\n"
                             "{\n    (void)argc; (void)argv;\n    return 0;\n}\n")
        with open(os.path.join(self.out, "docs_parse.md-3.c"), encoding="utf-8") as f:
            self.assertEqual(f.read(), "int main(void)\n{\n    return 1;\n}\n")

    def test_creates_outdir_and_overwrites_idempotently(self):
        out = os.path.join(self.out, "nested", "dir")
        p1 = run_extract(self.root, out)
        self.assertEqual(p1.returncode, 0, p1.stderr)
        with open(os.path.join(out, "README.md-1.c"), "w") as f:
            f.write("garbage")
        p2 = run_extract(self.root, out)
        self.assertEqual(p2.returncode, 0)
        self.assertEqual(p1.stdout, p2.stdout)
        with open(os.path.join(out, "README.md-1.c")) as f:
            self.assertIn("int main(int argc", f.read())

    def test_extract_does_not_modify_repository(self):
        before = subprocess.run(["git", "-C", self.root, "status", "--porcelain", "--ignored"],
                                capture_output=True, text=True).stdout
        run_extract(self.root, self.out)
        after = subprocess.run(["git", "-C", self.root, "status", "--porcelain", "--ignored"],
                               capture_output=True, text=True).stdout
        self.assertEqual(before, after)

    def test_declarations_calls_and_comments_are_not_definitions(self):
        root = make_repo({"docs/d.md": textwrap.dedent("""\
            ```c
            int main(void);
            ```

            ```c
            void f(int argc, char **argv) {
                main(argc, argv);
                /* as main() received it */
            }
            ```

            ```c
            int main(int argc,
                     char **argv)
            {
                return 0;
            }
            ```
            """)})
        p = run_extract(root, self.out)
        self.assertEqual(p.returncode, 0)
        self.assertEqual(self.manifest(), ["docs_d.md-3.c main docs/d.md:12"])

    def test_check_still_works(self):
        p = run(make_repo(CLEAN))
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)


if __name__ == "__main__":
    unittest.main()
