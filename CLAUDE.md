# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Orchestration System

This workspace uses a multi-agent system defined in `.cursor/`. The `.clauderules` file activates it for Claude Code. On every session, check if `.cursor/rules/project.mdc` exists — if not, prompt the user to run `/intake`.

### Slash commands → agent files

| Command | Agent file | Role |
|---|---|---|
| `/intake` | `.cursor/agents/intake.mdc` | Reads README.md → generates `.cursor/rules/project.mdc` |
| `/architect` | `.cursor/agents/architect.mdc` | System design and structural decisions |
| `/plan` | `.cursor/agents/planner.mdc` | Breaks tasks into steps; always ends with `/do → /debug → /review` |
| `/do` | `.cursor/agents/executor.mdc` | Writes complete, functional code — no pseudocode |
| `/review` | `.cursor/agents/code-reviewer.mdc` | Critical code review |
| `/debug` | `.cursor/agents/debugger.mdc` | Runtime diagnosis |
| `/test` | `.cursor/agents/qa-tester.mdc` | Test creation and coverage |
| `/secure` | `.cursor/agents/security-reviewer.mdc` | Security analysis |
| `/refactor` | `.cursor/agents/refactorer.mdc` | Behavior-preserving code improvement |
| `/doc` | `.cursor/agents/document-specialist.mdc` | Documentation |
| `/git` | `.cursor/agents/git-master.mdc` | Commits, branches, PRs |
| `/focus` | `.cursor/agents/delivery-tracker.mdc` | Delivery track guardian |
| `/clean` | `.cursor/skills/clean.mdc` | Remove AI-generic patterns from generated code |

When a `/command` is invoked, the orchestrator **spawns a real subagent** via the `Agent` tool — it does NOT execute the task itself. The subagent receives the `.mdc` identity + project context + task as a self-contained prompt and runs with an isolated context window. Never modify files under `.cursor/` directly — suggest editing the corresponding `.mdc`.

### Mandatory chain after `/plan` for implementation tasks

`/do` → `/debug` → `/review` — always sequential (each step depends on the previous). `/debug` and `/review` may run in parallel only when `/debug` produces no code changes. For C++ inference/DSP pipeline code, `/review` is never optional.

### Structure updated

```
Source/
  AudioEngine/
    StemSeparator.h/.cpp      # STFT → inference → IFFT pipeline (implemented)
    StemResult.h              # Value type: 4x AudioBuffer<float>
  ML/
    InferenceBackend.h        # Abstract interface
    OnnxBackend.h/.cpp        # ONNX Runtime implementation (implemented)
```

## Project: StemSlicer C++

**Status:** Scaffolding complete — contracts defined, no business logic yet.

A standalone C++ desktop application for offline audio source separation (stem extraction: vocals, drums, bass, other). Targets musicians/producers who need a fast, local alternative to Python-based tools like Spleeter or Demucs.

### Stack

- **Language:** C++17
- **Framework:** JUCE 7 (audio I/O, GUI, cross-platform) — via CMake FetchContent
- **Inference engine:** ONNX Runtime (ADR-001: CUDA EP on Windows, CoreML EP on macOS)
- **Build:** CMake 3.22+ + vcpkg (`vcpkg.json` manifest at root)
- **Target platforms:** Windows and macOS

### Build

```bash
# Install vcpkg first, then:

# Windows — build dir MUST be on a path without spaces (ONNX Runtime CMake bug with /external:I)
$cmake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S "." -B "C:\build\stemslicer" -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" -G "Visual Studio 17 2022" -A x64
& $cmake --build "C:\build\stemslicer" --config Release

# macOS
cmake -B /tmp/stemslicer-build -GXcode -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build /tmp/stemslicer-build --config Release
```

### Structure

```
CMakeLists.txt              # Root: JUCE + ONNX Runtime + add_subdirectory(Source)
vcpkg.json                  # Dependency manifest
Source/
  Main.cpp                  # Minimal JUCE app (blank window)
  AudioEngine/
    StemSeparator.h         # Public API: separate(mixture) → StemResult
    StemResult.h            # Value type: 4x AudioBuffer<float>
  ML/
    InferenceBackend.h      # Abstract interface for ONNX Runtime backend
  UI/                       # JUCE components (empty for now)
Models/                     # .onnx model files (not committed)
Resources/                  # Visual assets
```

### Layer boundaries (ADR-003)

`UI/` → `AudioEngine/` → `ML/` — strictly unidirectional. `AudioEngine/` and `ML/` must not include any JUCE GUI headers (`FileChooser`, `AlertWindow`, etc.). This keeps the core reusable for the future VST3 plugin.

### Reference material

- `spleeter/` — Deezer's Python reference implementation (TensorFlow, 2/4/5-stem models). Use to understand the model architecture and STFT/mask conventions before porting to C++.
- `archive/test/` — MUSDB18 test set audio stems (`vocals`, `drums`, `bass`, `other`, `mixture` as `.wav`). Use for ground-truth validation when testing the C++ pipeline.

## Key architecture note

The core separation pipeline is: STFT → model inference (soft masks) → mask multiply → IFFT. The Spleeter Python code in `spleeter/` is the authoritative reference for this pipeline. When implementing `AudioEngine/`, cross-reference Spleeter's `separator.py` and `audio/` modules for STFT parameters and mask application logic.
