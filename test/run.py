#!/usr/bin/env python3
"""Independent bounded owner runner for the R3 terminal foundation.

MODULE: run.py
REGISTRY: execute(argv, timeout) bounded compiler/owner child;
main() compiles real R2+R3 objects, trusted module fixtures, runs mirrored owners
in optimized assertion-enabled strict and ASan/UBSan modes, then bare R2 Console
and intended unsupported-arity diagnostics. No downloads, UI, live provider or
host terminal acquisition. Assertions/diagnostics remain enabled in both modes.
Owned outputs: one disposable directory under host-approved TMPDIR, removed by
TemporaryDirectory. Failures are aggregate nonzero, never retries until green.
Platform: current macOS arm64 lab. No Linux/Windows/macOS14 runtime claim, no
LSan or fully-instrumented dlopen modules (Apple ASan registration gap).
"""
from pathlib import Path
import os
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
APP = ROOT / "suite/ecosystem/determinant/src"
VEX = ROOT / "suite/ecosystem/vexspoke/src"
REL = ROOT / "suite/ecosystem/relational-engine/src"


def execute(argv, timeout=60):
    return subprocess.run(argv, capture_output=True, text=True, timeout=timeout,
                          env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})


def main():
    failures = 0
    sources = [*sorted(APP.rglob("*.c")), VEX / "primitive/vex-string.c",
               VEX / "atomic/spin.c", REL / "nio/mem.c",
               REL / "io/file.c", REL / "io/logparser.c"]
    flags = ["-std=gnu23", "-Wall", "-Wextra", "-Werror", "-O2", "-UNDEBUG",
             "-mcpu=apple-m1", "-mmacosx-version-min=14.0",
             f"-I{APP}", f"-I{VEX}", f"-I{REL}", f"-I{ROOT / 'tests'}"]
    with tempfile.TemporaryDirectory(prefix="determinant-", dir=os.environ.get("TMPDIR")) as tmp:
        tmp = Path(tmp)
        modules = []
        for mark in (65, 66):
            path = tmp / f"view-{mark}.dylib"
            result = execute(["clang", *flags, "-shared", "-fPIC",
                              f"-DDETERMINANT_FIXTURE_MARK={mark}",
                              str(HERE / "fixtures/view_module.c"), "-o", str(path)])
            if result.returncode:
                print(result.stderr)
                return 1
            modules.append(path)
        for mode, extra in (("strict", []), ("asan-ubsan", ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"])):
            objects = []
            for i, source in enumerate(sources):
                obj = tmp / f"{mode}-{i}.o"
                result = execute(["clang", *flags, *extra, "-c", str(source), "-o", str(obj)])
                if result.returncode:
                    print(f"BUILD FAIL {source}\n{result.stderr}")
                    return 1
                objects.append(str(obj))
            for owner in sorted(HERE.rglob("*_test.c")):
                binary = tmp / f"{mode}-{owner.stem}"
                defines = [f'-DDETERMINANT_MODULE_A="{modules[0]}"', f'-DDETERMINANT_MODULE_B="{modules[1]}"']
                result = execute(["clang", *flags, *extra, *defines, str(owner), *objects, "-o", str(binary)])
                if result.returncode:
                    print(f"BUILD FAIL {mode} {owner.name}\n{result.stderr}")
                    failures += 1
                    continue
                result = execute([str(binary)], timeout=15)
                if result.returncode or result.stdout or result.stderr:
                    print(f"FAIL {mode} {owner.name}: {result.returncode}\n{result.stdout}{result.stderr}")
                    failures += 1
                else:
                    print(f"PASS {mode} {owner.relative_to(HERE)}")
            bare = ROOT / "vexspoke/test/cli/console_test.c"
            binary = tmp / f"{mode}-bare-console"
            result = execute(["clang", *flags, *extra, str(bare), str(VEX / "cli/console.c"), *objects, "-o", str(binary)])
            if result.returncode:
                print(f"BUILD FAIL bare Console\n{result.stderr}")
                failures += 1
            else:
                result = execute([str(binary)], timeout=15)
                if result.returncode or result.stdout or result.stderr:
                    print(f"FAIL {mode} bare Console\n{result.stdout}{result.stderr}")
                    failures += 1
                else:
                    print(f"PASS {mode} bare_console_test")
        # These scratch translation units are compile-negative fixtures, not
        # production source. Match intended missing overload, not header errors.
        for cls, header, expression, missing in (
            ("Command", "cli/command.h", "Command(1)", "Command_1"),
            ("Scanner", "cli/scanner.h", "Scanner(1)", "Scanner_1"),
            ("CommandRegistry", "cli/commandregistry.h", "CommandRegistry(1)", "CommandRegistry_1"),
            ("Terminal", "ui/surface/terminal.h", "Terminal(1)", "Terminal_1"),
            ("TerminalButton", "ui/button/terminal_button.h", "TerminalButton(1)", "TerminalButton_1"),
            ("TerminalAnchor", "ui/panel/terminal_anchor.h", "TerminalAnchor(1, 2, 3, 4)", "TerminalAnchor_4"),
            ("TerminalPanel", "ui/panel/terminal_panel.h", "TerminalPanel(1)", "TerminalPanel_1"),
            ("TerminalScrollGlide", "ui/scroll/terminal_scroll_glide.h", "TerminalScrollGlide(1)",
             "TerminalScrollGlide_1"),
            ("TerminalBreadcrumb", "ui/text/terminal_breadcrumb.h", "TerminalBreadcrumb(1, 2, 3, 4, 5)",
             "TerminalBreadcrumb_5"),
        ):
            fixture = tmp / f"negative-{cls}.c"
            fixture.write_text(f'#include "{header}"\nint main(void) {{ (void){expression}; return 0; }}\n')
            result = execute(["clang", *flags, "-fsyntax-only", str(fixture)])
            if result.returncode == 0 or missing not in result.stderr:
                print(f"FAIL intended arity rejection {cls}\n{result.stderr}")
                failures += 1
            else:
                print(f"PASS intended arity rejection {cls}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
