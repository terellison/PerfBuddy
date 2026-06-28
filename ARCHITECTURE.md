# PerfBuddy — Architecture

PerfBuddy is a suite of C++ tools that analyze a game's **executable** and
**codebase** to surface concrete, actionable improvements across five areas of
concern: **binary size**, **runtime performance**, **memory management**,
**code quality**, and **engine/asset** (Unreal-specific) analysis.

The design goal is *separation of concerns*: each area is a **discrete,
independently runnable program** that produces a normalized report. A thin CLI
orchestrator and a Qt desktop GUI compose those programs without owning any
analysis logic.

---

## 1. Language & layout

The core and every analysis module are **C++17**, built with **CMake**. The
core has **zero third-party dependencies** — only the standard library — so the
analysis stack builds anywhere with a C++17 compiler. The GUI is the one
optional piece that needs an external dependency (**Qt6**); when Qt isn't
present, CMake simply skips it and everything else still builds.

```
PerfBuddy/
├── CMakeLists.txt                top-level build
├── modules/
│   ├── pb_core/                  shared types, Analyzer interface, JSON, utils  (library)
│   ├── pb_binary/                executable size & layout            (lib + `pb-binary`)
│   ├── pb_runtime/               frame-time / profiler hot spots     (lib + `pb-runtime`)
│   ├── pb_memory/                allocation-log analysis             (lib + `pb-memory`)
│   ├── pb_code/                  C/C++ source heuristics             (lib + `pb-code`)
│   ├── pb_unreal/                Unreal Engine analysis              (lib + `pb-unreal`)
│   ├── pb_app/                   composition: registry + orchestrator (library)
│   └── pb_cli/                   the `perfbuddy` orchestrator binary (CLI)
├── gui/                          Qt6 desktop GUI            (`perfbuddy-gui`, optional)
└── samples/                      runnable demo inputs
```

---

## 2. Module boundaries (separation of concerns)

Every analysis area is its own CMake target that is **both a library and a
standalone binary**. The library exposes an `Analyzer`; the binary is a small
`main()` wrapping a shared CLI driver, so each module can be run, scripted, and
CI-gated entirely on its own.

| Target | Concern | Standalone binary | Primary inputs |
|--------|---------|-------------------|----------------|
| `pb_core` | Shared types, `Analyzer` interface, JSON, helpers | — (library) | — |
| `pb_binary` | Executable **size & layout** bloat | `pb-binary` | `--exe` |
| `pb_runtime` | **Runtime / frame-time** hot spots | `pb-runtime` | `--data` (profiles) |
| `pb_memory` | **Memory** leaks, peak, fragmentation | `pb-memory` | `--data` (alloc logs) |
| `pb_code` | **Code quality** & perf anti-patterns | `pb-code` | `--src` |
| `pb_unreal` | **Unreal** assets, config & C++ heuristics | `pb-unreal` | `--src`, `--data` |
| `pb_app` | Registry + orchestration (composition) | — (library) | all |
| `pb_cli` | Orchestrator over all modules | `perfbuddy` | all |
| `gui` | Desktop GUI over `pb_app` | `perfbuddy-gui` | all |

Rules that keep the boundaries clean:

1. **Analysis modules never depend on each other** — only on `pb_core`. This is
   what makes them independently usable and independently testable.
2. **One normalized output** — every module emits a `ModuleReport` of
   `Finding`s. Only `pb_app` knows the full set of modules; the CLI and GUI know
   only `pb_core` and `pb_app`.
3. **No analysis in the shell** — `pb_cli` and the GUI are pure composition:
   pick targets, fan out to analyzers, merge reports, render.

---

## 3. The shared contract (`pb_core`)

```cpp
enum class Severity { Info, Low, Medium, High, Critical };
enum class Category { BinarySize, Runtime, Memory, CodeQuality, Assets };

struct Finding {
  std::string id, title, description;
  Severity severity;
  Category category;
  std::optional<Location> location;     // file:line / symbol / section
  std::string impact;                   // why it matters
  std::string remediation;              // what to do about it
  std::map<std::string,double> metrics; // machine-readable evidence
};

struct ModuleReport {
  std::string module, version, target_label;
  std::vector<Finding> findings;
  std::map<std::string,double> summary; // headline numbers
  long long duration_ms;
  std::optional<std::string> error;
};

struct Report { std::string generated_at; std::vector<ModuleReport> modules; };

struct Target {
  std::optional<std::string> executable; // built game binary
  std::optional<std::string> source_dir; // codebase root
  std::optional<std::string> data_dir;   // profiles / alloc logs / cooked assets
  std::optional<std::string> rules_file; // rule severity overrides (see RuleSettings)
  Engine engine;                         // Native | Unreal | Unknown (auto-detected)
};

class Analyzer {
  virtual std::string name() const = 0;
  virtual std::string description() const = 0;
  virtual Category category() const = 0;
  virtual bool supports(const Target&) const = 0;
  virtual ModuleReport analyze(const Target&) const = 0;
};
```

Everything serializes to JSON via a small built-in `pb::json` value type, so any
module's output is consumable by the GUI, by CI, or by other tooling without
coupling and without external libraries.

`pb_core` also offers an opt-in `RuleSettings` helper
(`pb_core/ruleset.hpp`) that parses an `.editorconfig`-style file —
`[finding.id]` sections with a `severity = info|low|medium|high|critical|none`
key — so users can disable or re-rank individual findings without touching
code. It's loaded from `Target::rules_file` (set via `--rules <file>` on any
CLI); `pb_code` is the first module to honor it, applying overrides to every
`Finding` it emits before returning its `ModuleReport`.

---

## 4. What each module does

**`pb_binary`** parses the executable natively — ELF (Linux), PE (Windows), and
Mach-O (macOS) — and reports section/segment sizes, the heaviest symbols,
whether the binary is stripped, and debug-info weight: the levers for shrinking
executable size.

**`pb_runtime`** ingests profiler output (`.folded` collapsed stacks and a
frame-time series) and reports the hottest call stacks, frame-time percentiles
(p50/p95/p99), and frames that exceed the budget.

**`pb_memory`** replays an allocation log (alloc/free, CSV or JSON) and reports
leak candidates (never freed), peak usage, invalid/double frees, the largest
allocations, and small-allocation churn / fragmentation.

**`pb_code`** walks the C/C++ source tree and flags performance and quality
anti-patterns via fast, comment-aware heuristics: oversized translation units,
heavy headers, `std::endl` flushes, raw `new`/`delete`, deep nesting,
`using namespace std` in headers, and TODO/FIXME debt.

**`pb_unreal`** detects an Unreal project, sizes cooked content
(`.pak`/`.uasset`/`.umap`), flags oversized assets, scans `Config/*.ini` for
costly settings, and applies Unreal C++ heuristics (per-frame `Tick`, `GetWorld()`
/ `Cast<>` in `Tick`, `GetAllActorsOfClass`).

---

## 5. Composition layers

**`pb_app`** is the only component that depends on every module. It builds the
registry of analyzers and `run_analysis(target, only, parallel)` — asks each
analyzer whether it `supports` the target, runs the supported ones concurrently
(`std::async`), and merges their `ModuleReport`s into one `Report`.

**CLI (`perfbuddy`)** —
`perfbuddy run --exe game --src ./src --data ./prof --format json`. Builds a
`Target`, calls `pb_app`, renders text or JSON. `--only pb_binary,pb_code`
selects modules; `perfbuddy list` shows them.

**GUI (`perfbuddy-gui`)** — a Qt6 desktop shell that picks the
executable/source/data, runs the **exact same** `pb_app::run_analysis` on a
worker thread, and browses findings grouped by module and severity, with JSON
export. The GUI owns no analysis logic.

```
        ┌───────────────┐        ┌───────────────┐
        │ perfbuddy-gui │        │   perfbuddy   │   (both are thin)
        │   (Qt6)       │        │     CLI       │
        └──────┬────────┘        └──────┬────────┘
               │      pb_app::run_analysis(Target)
               ▼                        ▼
        ┌──────────────────────────────────────────┐
        │  pb_app  — registry + parallel orchestration│
        ├──────────┬──────────┬──────────┬──────────┬┤
        │ pb_binary│pb_runtime│ pb_memory│ pb_code  │pb_unreal │ ← independent libs,
        └──────────┴──────────┴──────────┴──────────┴──────────┘   each also a CLI
               │  every module emits ModuleReport (Finding[])
               ▼
        merged Report  →  JSON / text / GUI table
```

---

## 6. Extending PerfBuddy

Add a concern by creating a `modules/pb_<x>` target that depends only on
`pb_core`, implementing `Analyzer`, and registering it in
`pb_app::make_default_analyzers()`. The CLI and GUI pick it up automatically
because they go through `pb_app`. No existing module or shell changes — that is
the payoff of the boundaries above.

See `README.md` for build and run instructions.
