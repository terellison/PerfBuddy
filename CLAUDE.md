# CLAUDE.md

Guidance for Claude (and contributors) working in the PerfBuddy repository.

## What this is

PerfBuddy is a suite of **C++17** tools that analyze a game's **executable** and
**codebase** and report concrete improvements across five concerns: binary size,
runtime performance, memory, code quality, and Unreal Engine assets/config.
See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the full design and
[`README.md`](README.md) for build/run instructions.

## Build & run

Build out-of-tree. CMake ≥ 3.16 and a C++17 compiler; Qt6 is optional (GUI only).

```bash
cmake -S . -B build            # add -DPERFBUDDY_BUILD_GUI=OFF to skip Qt entirely
cmake --build build -j
./build/modules/pb_cli/perfbuddy list
./build/modules/pb_cli/perfbuddy run --exe <bin> --src <dir> --data <dir>
```

Standalone module binaries: `build/modules/pb_<x>/pb-<x>` (e.g. `pb-binary`,
`pb-code`). The GUI, when Qt6 is present: `build/gui/perfbuddy-gui`.

Try it against the bundled sample (see README "Quick start"). For the Unreal
sample's large-asset findings, run `samples/unreal-demo/make_content.sh` first.

## Architecture rules (do not break these)

- **Analysis modules depend ONLY on `pb_core`** — never on each other. This is
  what keeps them independently buildable, runnable, and testable.
- **`pb_app` is the only component that knows every module.** It builds the
  analyzer registry and orchestrates runs.
- **`pb_cli` and `gui/` are pure composition** — they collect a `Target`, call
  `pb::app::run_analysis(...)`, and render. They contain no analysis logic.
- **Everything flows through `pb_core` types** (`Finding`, `ModuleReport`,
  `Report`, `Target`, `Analyzer`) and serializes via the built-in `pb::json`.
- **No third-party dependencies in the core/analysis stack.** Standard library
  only. The single exception is Qt6 in `gui/`, which is optional.

## Adding a new analysis concern

1. Create `modules/pb_<name>/` mirroring an existing module (`include/`, `src/`,
   `CMakeLists.txt`) with a class implementing `pb::Analyzer`.
2. Make it a library + a standalone CLI whose `main()` calls
   `pb::run_module_cli(analyzer, argc, argv)`.
3. Link it only against `pb_core`.
4. Register it in `pb::app::make_default_analyzers()` (in `modules/pb_app/src/app.cpp`)
   and add the subdirectory in the top-level `CMakeLists.txt`.
5. The CLI and GUI pick it up automatically — do not special-case it anywhere.

## Conventions

- C++17, 2-space indent, `#pragma once` headers, namespaces `pb`, `pb::<module>`,
  `pb::util`, `pb::json`, `pb::app`.
- Public headers live under `modules/pb_<x>/include/pb_<x>/`.
- Analyzers should **not throw for ordinary data problems** — return a
  `ModuleReport` with `error` set instead. `pb_app` guards against exceptions too.
- Each `Finding` carries: `severity`, `category`, optional `location`, `impact`
  (why it matters), `remediation` (what to do), and numeric `metrics`.
- Warnings are errors-in-spirit: build is `-Wall -Wextra -Wpedantic` and must stay clean.

## Environment quirks (important)

- **Build out-of-tree.** The working directory here is a mounted folder that
  cannot host the CMake build dir or delete build artifacts; configure with
  `-B /tmp/<dir>` (or any path outside the mount) when building in this sandbox.
- **Qt6 cannot be installed in this sandbox**, so the GUI is compile-unverified
  from here; it is written to standard Qt6 Widgets + Concurrent APIs. Verify GUI
  changes on a machine with Qt6 installed.

## Verifying changes

Rebuild clean and run both samples through the CLI (text and `--format json`),
plus the relevant standalone module(s). Confirm: no compiler warnings, JSON
parses, and modules auto-select correctly per target (e.g. `pb_unreal` only fires
on Unreal projects; `pb_binary` only with `--exe`).
