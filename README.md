# PerfBuddy

A suite of C++ tools that analyze a game's **executable** and **codebase** and
suggest concrete improvements to **binary size**, **runtime performance**,
**memory management**, **code quality**, and **Unreal Engine** assets/config.

Each area of concern is a **standalone program**; a single `perfbuddy` CLI and a
**Qt6 desktop GUI** compose them. See [`ARCHITECTURE.md`](ARCHITECTURE.md) for
the full design.

---

## Modules

| Binary | Concern | Inputs |
|--------|---------|--------|
| `pb-binary`  | Executable size & layout (ELF/PE/Mach-O) | `--exe` |
| `pb-runtime` | Frame-time percentiles & hottest stacks  | `--data` (`*.folded`, `*frametime*.csv`) |
| `pb-memory`  | Leaks, peak, churn from an alloc log      | `--data` (`*alloc*.csv` / `.json`) |
| `pb-code`    | C/C++ perf & quality anti-patterns        | `--src` |
| `pb-unreal`  | Unreal assets, config & C++ heuristics    | `--src`, `--data` |
| `perfbuddy`  | Runs all applicable modules at once       | any of the above |
| `perfbuddy-gui` | Desktop UI over all modules (optional) | any of the above |

---

## Build

Requires **CMake ≥ 3.16** and a **C++17** compiler. Qt6 is optional (GUI only).

```bash
cmake -S . -B build
cmake --build build -j
```

The analysis stack has **no third-party dependencies**. If Qt6 is found, the GUI
(`perfbuddy-gui`) is built too; otherwise CMake skips it with a message and
everything else still builds. To build without even looking for Qt:

```bash
cmake -S . -B build -DPERFBUDDY_BUILD_GUI=OFF
```

Binaries land under `build/modules/*/` (and `build/gui/` for the GUI).

---

## Tests

Unit tests build by default (`PERFBUDDY_BUILD_TESTS`, `ON`) and run via CTest —
no third-party test framework, just a small header-only harness under `tests/`.

```bash
cmake -S . -B build
cmake --build build -j
cd build && ctest --output-on-failure
```

To skip building tests entirely: `cmake -S . -B build -DPERFBUDDY_BUILD_TESTS=OFF`.

Each module has its own test binary (`pb_core_test`, `pb_code_test`,
`pb_unreal_test`, `pb_app_test`) under `build/tests/`, runnable directly too:

```bash
./build/tests/pb_unreal_test
```

---

## Quick start (with the bundled sample)

```bash
# Build the demo executable so pb-binary has a real target
g++ -std=c++17 -g samples/native-demo/src/game.cpp -o samples/native-demo/game

# Run the whole suite
./build/modules/pb_cli/perfbuddy run \
    --exe  samples/native-demo/game \
    --src  samples/native-demo/src \
    --data samples/native-demo/profiling
```

List the modules, or run just one:

```bash
./build/modules/pb_cli/perfbuddy list
./build/modules/pb_cli/perfbuddy run --src samples/native-demo/src --only pb_code
```

Run a single module's standalone binary:

```bash
./build/modules/pb_binary/pb-binary  --exe  samples/native-demo/game
./build/modules/pb_memory/pb-memory  --data samples/native-demo/profiling --format json
```

---

## Output

Text by default; `--format json` emits a machine-readable `Report` (handy for CI
gates). Every finding carries a severity, the location, why it matters
(*impact*), and what to do (*remediation*), plus numeric *metrics*.

```
== pb_runtime (10 findings) ==
  [HIGH] Frame time p50=14.11 p95=16.74 p99=30.92 max=38.91 ms (budget 16.67)
        fix: Profile the hottest stacks below; move work off the critical path…
  [MED ] Hot stack (24% of samples): main;GameLoop;Render;DrawCalls;SubmitGPU
```

---

## Input formats

- **Profiler stacks** — `*.folded` (collapsed/folded stacks): `frameA;frameB;leaf <count>` per line (Brendan Gregg / `perf script | stackcollapse` format).
- **Frame times** — any `*frametime*.csv` (or `*frames*`): one millisecond value per line; a header row is auto-skipped. Budget defaults to 16.67 ms (60 FPS).
- **Allocation log** — `*alloc*.csv`: `op,address,size[,timestamp][,tag]` where `op` is `A`/`alloc` or `F`/`free`; or a JSON array of `{op,addr,size,time,tag}`.

---

## GUI

`perfbuddy-gui` (built when Qt6 is available) lets you pick the executable,
source, and data directories, choose which modules to run, and browse findings
grouped by module and severity, with one-click JSON export. It calls the exact
same orchestration as the CLI, so results are identical.

### Installing Qt6

The GUI requires Qt6 (`Widgets` + `Concurrent`). Install it, then re-run CMake —
`find_package(Qt6)` picks it up and the `perfbuddy-gui` target builds
automatically.

**macOS (Homebrew):**

```bash
brew install qt    # installs Qt6
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
./build/gui/perfbuddy-gui
```

Homebrew doesn't put Qt on CMake's default search path, so pass
`-DCMAKE_PREFIX_PATH`. The `$(brew --prefix qt)` expansion resolves correctly on
both Apple Silicon (`/opt/homebrew/...`) and Intel (`/usr/local/...`).

**Ubuntu / Debian:**

```bash
sudo apt install qt6-base-dev
cmake -S . -B build      # found automatically; no prefix needed
cmake --build build -j
```

**Windows / specific Qt version:** use the official
[Qt Online Installer](https://www.qt.io/download), then point CMake at it:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/<compiler>"
```

---

## Status

This is a working v0.1 foundation: all five analyzers, the orchestrator, and the
GUI are implemented with real parsing/heuristics and verified against the bundled
sample. Heuristics are intentionally lightweight and dependency-free; they are
designed to be extended (see *Extending PerfBuddy* in `ARCHITECTURE.md`).
