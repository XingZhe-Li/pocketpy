#!/usr/bin/env python3
"""Quick side-by-side benchmark of pocketpy under three TLS strategies.

Compares three previously-built variants (does NOT rebuild):

    1. GCC   with    emutls  -> build_gcc\\main.exe            (unmodified pocketpy)
    2. GCC   without emutls  -> build_gcc_real_noTLS\\main.exe (built with -D_Thread_local=)
    3. Clang native TLS      -> build_clang\\main.exe          (winlibs Clang)

Wall-clock timing, best-of-N runs per variant. Optionally runs a one-shot
sanity check with objdump to verify the emutls/gs:0x58 signature of each
binary.

Usage:
    python scripts/compare_tls.py
    python scripts/compare_tls.py benchmarks/primes.py
    python scripts/compare_tls.py -n 5 benchmarks/primes.py benchmarks/vec.py
    python scripts/compare_tls.py --no-sanity benchmarks/loop_2.py
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# The three TLS strategies we compare. The first slot is toggleable via
# --baseline: by default we use build_gcc/ (whatever your current branch
# built with the default toolchain flags), which is what most users have;
# --baseline swaps it for the unmodified `main`-branch build_baseline/,
# useful for measuring the raw stock winlibs behaviour.
VARIANT_CLANG = ("clang", REPO / "build_clang" / "main.exe")
VARIANT_GCC_NOTLS = ("gcc-emutls", REPO / "build_gcc_real_noTLS" / "main.exe")
VARIANT_GCC_DEFAULT = ("gcc+emutls", REPO / "build_gcc" / "main.exe")
VARIANT_GCC_BASELINE = ("gcc+emutls*", REPO / "build_baseline" / "main.exe")

DEFAULT_SCRIPTS = [
    "benchmarks/primes.py",
    "benchmarks/fib.py",
    "benchmarks/loop_2.py",
    "benchmarks/sort.py",
    "benchmarks/dict_0.py",
    "benchmarks/vec.py",
]


def find_objdump() -> Path | None:
    vendored = REPO / "tmp" / "mingw64" / "bin" / "objdump.exe"
    if vendored.exists():
        return vendored
    on_path = shutil.which("objdump")
    return Path(on_path) if on_path else None


def sanity_line(exe: Path, objdump: Path) -> str:
    try:
        result = subprocess.run(
            [str(objdump), "-d", str(exe)],
            capture_output=True, check=True, text=False,
        )
    except (subprocess.CalledProcessError, FileNotFoundError) as exc:
        return f"objdump failed: {exc}"

    disasm = result.stdout
    emu = disasm.count(b"__emutls_get_address")
    gs = disasm.count(b"gs:0x58")
    return f"emutls_calls={emu:<5d} gs_refs={gs:<5d}"


def measure_best(exe: Path, script: Path, reps: int) -> int:
    """Return the shortest wall-clock time (ms) over `reps` runs."""
    best = float("inf")
    for _ in range(reps):
        t0 = time.perf_counter()
        completed = subprocess.run(
            [str(exe), str(script)],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        elapsed = (time.perf_counter() - t0) * 1000.0
        if completed.returncode != 0:
            print(f"  [warn] {exe.name} returned {completed.returncode} on {script}",
                  file=sys.stderr)
        if elapsed < best:
            best = elapsed
    return int(best)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("scripts", nargs="*",
                        help="Python scripts to benchmark (default: pocketpy benchmarks)")
    parser.add_argument("-n", "--reps", type=int, default=3,
                        help="Repetitions per binary (default 3)")
    parser.add_argument("--baseline", action="store_true",
                        help="Use build_baseline\\ (clean main) as the gcc+emutls variant "
                             "instead of build_gcc\\ (current branch)")
    parser.add_argument("--no-sanity", action="store_true",
                        help="Skip objdump-based emutls/gs signature check")
    args = parser.parse_args()

    gcc_variant = VARIANT_GCC_BASELINE if args.baseline else VARIANT_GCC_DEFAULT
    variants = [gcc_variant, VARIANT_GCC_NOTLS, VARIANT_CLANG]

    scripts = [Path(s) for s in (args.scripts or DEFAULT_SCRIPTS)]

    missing = [str(p) for _, p in variants if not p.exists()]
    if missing:
        print("[ERROR] missing build artefacts:", file=sys.stderr)
        for m in missing:
            print(f"  {m}", file=sys.stderr)
        print("\nRe-run the corresponding cmake+build before using this script.",
              file=sys.stderr)
        return 1

    for s in scripts:
        if not (REPO / s).exists():
            print(f"[ERROR] script not found: {s}", file=sys.stderr)
            return 1

    if not args.no_sanity:
        objdump = find_objdump()
        print("=== TLS sanity check ===")
        if objdump is None:
            print("  [skipped - objdump not found]")
        else:
            for label, exe in variants:
                print(f"  {label:<12} {sanity_line(exe, objdump)}")
        print()

    print(f"=== Benchmarks (best of {args.reps} runs, wall-clock in ms) ===")
    header = f"{'script':<28}" + "".join(f"{lbl:>12}" for lbl, _ in variants)
    header += "   g/cl    gN/cl"
    print(header)
    print("-" * len(header))

    for script in scripts:
        row_times: dict[str, int] = {}
        for label, exe in variants:
            row_times[label] = measure_best(exe, REPO / script, args.reps)

        g1 = row_times[variants[0][0]]
        g2 = row_times[variants[1][0]]
        cl = row_times[variants[2][0]]

        cells = f"{str(script):<28}"
        for label, _ in variants:
            cells += f"{row_times[label]:>9d} ms"
        cells += f"   {g1/cl:5.2f}x  {g2/cl:5.2f}x"
        print(cells, flush=True)

    return 0


if __name__ == "__main__":
    sys.exit(main())
