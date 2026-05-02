# StemSlicer

Standalone desktop application for offline audio source separation. Extracts vocals, drums, bass, and other from any audio file — no Python, no cloud, no internet required.

Built with C++17 + JUCE 7 + ONNX Runtime, using a port of Deezer's [Spleeter](https://github.com/deezer/spleeter) 4-stems model.

---

## Features

- Drag-and-drop or file picker input (WAV, MP3, FLAC, AIFF)
- 4-stem separation: **vocals**, **drums**, **bass**, **other**
- Output as 32-bit float WAV, stereo, 44100 Hz
- Background processing thread — UI never freezes
- Granular progress feedback per chunk
- Cross-platform: Windows and macOS

## Stack

| Layer | Technology |
|---|---|
| GUI / Audio I/O | JUCE 7 |
| Inference | ONNX Runtime (CUDA EP on Windows, CoreML EP on macOS) |
| Language | C++17 |
| Build | CMake 3.22+ + vcpkg |

## Architecture

Strictly unidirectional layers (ADR-003):

```
UI/  →  AudioEngine/  →  ML/
```

`AudioEngine/` and `ML/` contain no JUCE GUI headers — reusable for a future VST3/AU plugin.

### DSP Pipeline

```
Input audio
  → resample to 44100 Hz (LagrangeInterpolator)
  → STFT  (N_FFT=4096, hop=1024, Hann window, center=True)
  → ONNX inference  [batch=1, T=512, F=1024, channels=2]
  → soft-mask multiply per stem
  → ISTFT  (overlap-add, COLA=1.5)
  → 4x WAV output
```

Round-trip STFT→ISTFT SNR: **141 dB**.

## Build

### Prerequisites

- CMake 3.22+
- [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set
- Visual Studio 2022 Build Tools (Windows) or Xcode (macOS)

### Windows

```powershell
$cmake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

# Build dir MUST be on a path without spaces (ONNX Runtime CMake bug)
& $cmake -S "." -B "C:\build\stemslicer" `
  -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" `
  -G "Visual Studio 17 2022" -A x64

& $cmake --build "C:\build\stemslicer" --config Release
```

### macOS

```bash
cmake -B /tmp/stemslicer-build -GXcode \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build /tmp/stemslicer-build --config Release
```

## Model Setup

The `Models/4stems.onnx` file (~150 MB) is tracked via Git LFS and included in the repository.

To regenerate it from the original Spleeter TensorFlow checkpoints:

```bash
# Requires Python 3.10 or 3.11 + spleeter installed
pip install -r scripts/requirements.txt

python scripts/tf_to_onnx.py \
  --model-dir ~/.cache/spleeter/pretrained_models/4stems \
  --output Models/4stems.onnx
```

After building, copy the model next to the executable:

```powershell
# Windows example
Copy-Item Models\4stems.onnx C:\build\stemslicer\Source\StemSlicer_artefacts\Release\Models\
```

## Project Structure

```
CMakeLists.txt              # Root: JUCE + ONNX Runtime
vcpkg.json                  # vcpkg manifest (onnxruntime)
Source/
  Main.cpp                  # App entry point, model loading, MainWindow
  AudioEngine/
    StemSeparator.h/.cpp    # STFT → inference → ISTFT pipeline
    StemResult.h            # Value type: 4x AudioBuffer<float>
    AudioFileReader.h/.cpp  # Load + resample audio files
    AudioFileWriter.h/.cpp  # Write WAV 32-bit float
    SeparationThread.h/.cpp # juce::Thread wrapper with progress callbacks
  ML/
    InferenceBackend.h      # Abstract inference interface
    OnnxBackend.h/.cpp      # ONNX Runtime implementation
  UI/
    MainComponent.h/.cpp    # Drop-zone, progress bar, file choosers
Models/                     # 4stems.onnx (Git LFS)
scripts/
  tf_to_onnx.py             # Convert Spleeter TF checkpoints → ONNX
  tests/                    # 40 pytest tests for the conversion script
  requirements.txt
spleeter/                   # Deezer's Python reference implementation
```

## Reference

- `spleeter/` — original Python implementation (STFT parameters, mask conventions)
- `archive/` — MUSDB18 test set for validation (stored on Google Drive, not in repo)
  - **Google Drive:** https://drive.google.com/drive/folders/1XirPRsEIHEtHDM6VlE23gRoaPnG21oeN?usp=drive_link
  - Download locally to `archive/` before running end-to-end quality validation
