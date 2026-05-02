# StemSlicer — Delivery Track

## Sprint 1 — Pipeline End-to-End

**Objetivo:** dado `archive/test/mixture.wav`, gerar 4 arquivos `.wav` (vocals, drums, bass, other) sem crash, com qualidade auditivamente verificável, em thread separada, < 2GB RAM.

---

## Fases

| ID | Entrega | Status | Prioridade | Observação |
|---|---|---|---|---|
| F1 | Corrigir bugs críticos DSP no StemSeparator | ✅ DONE | P0 | 9 fixes; SNR round-trip 141 dB; build limpo |
| F2 | Pipeline stereo + chunking T=512, shape `[N,512,1024,2]` + center=True | ✅ DONE | P0 | SNR 141 dB; trim bug corrigido; build limpo |
| EXT | Converter modelo Spleeter TF→ONNX (Python, externo) | ✅ DONE | P0 | `scripts/tf_to_onnx.py` + `scripts/tests/` (40 testes); requer checkpoint + Python 3.10/3.11 |
| F3 | `AudioFileReader` + Resample (LagrangeInterpolator) | ✅ DONE | P1 | `loadAudioFile()` → stereo 44100Hz; build limpo |
| F4 | `AudioFileWriter` (stems → `.wav` 32-bit float) | ✅ DONE | P1 | `writeAudioFile()` WAV 32-bit float; build limpo |
| F5 | Integração `separateFile()` no AudioEngine | ✅ DONE | P1 | `separateFile()` com progressCallback; build limpo |
| F6 | Threading — `SeparationThread : juce::Thread` | ✅ DONE | P2 | `onProgress`/`onFinished` callbacks; `stopThread(5000)`; build limpo |
| F7 | UI mínima — FileChooser, barra de progresso, output dir | ✅ DONE | P2 | Dark theme `#1a1a2e`/cyan `#00cfff`; drag-drop; máquina de estados; build limpo |
| F8 | Validação end-to-end contra MUSDB18 | 🔄 WIP | P3 | Build ✅ (5.08 MB, 02/05/2026); aguardando `Models/4stems.onnx` para validação runtime |

---

## Critério de "Pronto" do Sprint 1

- [ ] `archive/test/mixture.wav` → 4 `.wav` sem crash
- [ ] Soma dos 4 stems ≈ mixture por escuta (conservação de energia)
- [ ] `vocals.wav` claramente menos percussivo que o mixture
- [ ] Arquivo de 3 min processado com RAM pico < 2GB
- [ ] UI não trava durante separação (thread separada)

---

## Decisões Arquiteturais Fixadas

| ADR | Decisão |
|---|---|
| ADR-001 | ONNX Runtime (não TFLite) — suporte a CUDA EP |
| ADR-002 | CMake + vcpkg (não Projucer) |
| ADR-003 | Camadas UI→AudioEngine→ML unidirecionais; AudioEngine/ML sem JUCE GUI headers |
| DA-001 | Shape do modelo Spleeter 4stems: `[batch, T=512, F=1024, n_channels=2]` |
| DA-002 | `UI/` cria `OnnxBackend`, chama `loadModel()`, injeta no `StemSeparator` |
| DA-003 | `AudioFileReader`/`Writer` em `AudioEngine/` usando `juce_audio_formats` (não-GUI) |
| DA-004 | Resample interno ao `separate()` via `juce::LagrangeInterpolator` |
| DA-005 | Pipeline stereo — sem mixdown mono; tensor `[N, 512, 1024, 2]` |

---

## Descobertas Técnicas Confirmadas

- **Janela Hann periódica** (`/ size`) satisfaz COLA exatamente com hop=N/4: `winSum = 1.5` constante
- **Round-trip STFT→IFFT:** SNR 141.1 dB no sinal útil (validado em Python)
- **Shape do modelo Spleeter:** `[N, 512, 1024, 2]` — F=1024 (não 2049), T=512, n_channels=2, com `mask_extension="zeros"` para bins 1025..2048
- **center=True:** Spleeter faz zero-padding de `N_FFT/2 = 2048` amostras em cada extremidade antes do STFT
- **Build dir:** `C:\build\stemslicer` — sem espaços (bug ONNX Runtime com `/external:I` no MSVC)

---

## Log de Sessões

### Sessão 2 (concluída)
- ✅ F1–F7 completas (todas as fases C++ implementadas e build limpo)
- ✅ UI com estética dark pro-audio baseada em `vst-cpp-key-detector-main`

### Sessão 3 (concluída)
- ✅ EXT: `scripts/tf_to_onnx.py` — conversão TF1→ONNX via `/do`→`/review`→`/debug`→`/test`
- 4 bugs críticos corrigidos (custom Graph, Dropout ativo, `from_session` inexistente, `Saver` fora de escopo)
- 40 testes pytest em `scripts/tests/test_tf_to_onnx.py`

### Sessão 4 (concluída — C++ pronto para validação)
- ✅ F8: `findModelPath()` em Main.cpp — busca `Models/4stems.onnx` e `4stems.onnx` ao lado do exe; Unicode-safe via `toWideCharPointer()` no Windows
- ✅ F8: progresso granular por chunk em `separate()` + fases distintas em `separateFile()` (load→0.05, chunks→[0.1,0.88], sep done→0.90, escrita→[0.925..1.0])
- ✅ Bug: data race em `onProgress`/`onFinished` — todas as escritas de estado movidas para `callAsync` (message thread)
- ✅ Bug: `chunkProgress` agora disparado após validação dos outputs do chunk, não antes
- ✅ Bug: guarda `state_ == State::Processing` em `chooseInputFile()` e `filesDropped()` (race condition de UI)

---

## Próximas Ações

| Ordem | ID | Ação | Bloqueio |
|---|---|---|---|
| 1 | F8 | Build C++: `cmake --build C:\build\stemslicer --config Release` | — |
| 2 | F8 | Gerar ONNX: `python scripts/tf_to_onnx.py --model-dir %USERPROFILE%\.cache\spleeter\pretrained_models\4stems --output Models/4stems.onnx` | Requer Python 3.10/3.11 + spleeter instalado |
| 3 | F8 | Copiar modelo: `New-Item -Force -ItemType Directory C:\build\stemslicer\Source\StemSlicer_artefacts\Release\Models; Copy-Item Models\4stems.onnx C:\build\stemslicer\Source\StemSlicer_artefacts\Release\Models\` | Após passo 2 |
| 4 | F8 | Executar: `C:\build\stemslicer\Source\StemSlicer_artefacts\Release\StemSlicer.exe` e validar os 5 critérios do Sprint 1 | Após passos 1-3 |
