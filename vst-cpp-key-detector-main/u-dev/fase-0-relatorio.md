# Relatório — Fase 0: Setup e Scaffolding

**Projeto:** KeyDetectorVST  
**Data de conclusão:** 2026-04-21  
**Status:** ✅ Concluída

---

## Objetivo da fase

Estabelecer a infraestrutura de compilação e a estrutura de arquivos do projeto antes de qualquer lógica funcional. O critério de aceitação era: `cmake --build` compilar sem erros e o plugin carregar como artefato VST3.

---

## O que foi entregue

### Estrutura de pastas

```
vst-cpp-key-detector/
├── CMakeLists.txt
├── JUCE/                          ← git clone shallow tag 7.0.12
├── src/
│   ├── PluginProcessor.h / .cpp
│   ├── PluginEditor.h / .cpp
│   ├── MidiAnalyzer.h / .cpp
│   ├── AudioAnalyzer.h / .cpp
│   ├── KeyEstimator.h / .cpp
│   ├── ScaleClassifier.h / .cpp
│   ├── ProgressionSuggester.h / .cpp
│   └── data/
│       └── markov_transitions.json
├── tests/
│   ├── CMakeLists.txt
│   ├── test_key_estimator.cpp
│   └── test_scale_classifier.cpp
└── u-dev/
    └── fase-0-relatorio.md        ← este arquivo
```

### CMakeLists.txt

- JUCE 7 via submodule local ou `FetchContent` (`-DFETCH_JUCE=ON`)
- Formatos: **VST3 + Standalone** (Windows); **VST3 + AU + Standalone** (macOS)
- `COPY_PLUGIN_AFTER_BUILD FALSE` — evita depender de DAW instalada no CI
- Testes com Catch2 v3 via FetchContent, isolados de JUCE (sem linkagem)
- Opção `BUILD_TESTS=OFF` para builds de produção

### Stubs implementados

| Arquivo | Estado | Observação |
|---|---|---|
| `PluginProcessor` | Funcional | processBlock sem alocação; SpinLock thread-safe para UI |
| `PluginEditor` | Stub | Timer 30fps; placeholder de texto |
| `MidiAnalyzer` | **Funcional** | Histograma com decay 0.995f; notas ativas |
| `AudioAnalyzer` | Stub | Estrutura FFT declarada; implementação na Fase 2 |
| `KeyEstimator` | **Funcional** | Krumhansl-Schmuckler completo com correlação de Pearson |
| `ScaleClassifier` | **Funcional** | 15 modos, ranking por dot product normalizado |
| `ProgressionSuggester` | Stub | Estrutura e assinaturas definidas; implementação na Fase 4 |

### Convenções aplicadas (AGENT.md)

- `#pragma once` em todos os headers
- Prefixo `_` em todos os membros privados
- Zero alocações em `processBlock()`
- UI timer na thread de mensagens (nunca na thread de áudio)
- `juce::SpinLock` com `ScopedTryLockType` no lado da thread de áudio

---

## Resultados de build

| Alvo | Resultado |
|---|---|
| Gerador | Visual Studio 18 2026 / MSVC 19.50 |
| `KeyDetector_SharedCode.lib` | ✅ 0 erros, 0 warnings |
| `Key Detector.vst3` | ✅ gerado em `build/…/VST3/` |
| `Key Detector.exe` (Standalone) | ✅ gerado em `build/…/Standalone/` |
| `test_key_estimator` | ✅ 4 testes, 10 assertions — **all passed** |
| `test_scale_classifier` | ✅ 5 testes, 23 assertions — **all passed** |

---

## Bugs encontrados e corrigidos durante a fase

### 1. `ScaleProfile` inacessível fora da classe

**Problema:** `ScaleProfile` declarado como `private` mas usado em função livre no `.cpp`.  
**Fix:** Promovido para escopo `public` (antes da seção `private:`).

### 2. Shift direction errado na correlação de Pearson

**Problema:** O loop de rotação usava `y[(i + shift) % 12]`, o que mapeava a tônica para a nota errada. Para `root=9` (Lá), a tônica era atribuída a Eb em vez de A.  
**Fix:** Alterado para `y[(i - shift + 12) % 12]`, garantindo que `pitch_i - root` determine o grau da escala.  
**Impacto:** Detecção para tonalidades diferentes de C estava errada. Bug crítico corrigido antes de avançar para Fase 1.

### 3. Warning de shift 32→64 bit (MSVC C4334)

**Problema:** `1 << fftOrder` produzia warning no MSVC ao ser atribuído a `size_t`.  
**Fix:** `static_cast<size_t>(1) << fftOrder`.

### 4. Teste de A minor com pesos iguais

**Problema:** O teste usava `profile[pc] = 1.0f` para todos os graus, mas A menor e Dó maior compartilham exatamente os mesmos pitch classes. Com pesos iguais, o Krumhansl-Schmuckler escolhe Dó maior (C tem peso de tônica 6.35 vs A com 3.66 no template major).  
**Fix:** Teste atualizado com A enfatizado (`3.0f`), refletindo o comportamento real em que a tônica aparece mais vezes na música. Esse comportamento é matematicamente correto e documentado para Fase 1.

---

## Critérios de aceitação — status final

| Critério | Status |
|---|---|
| `cmake --build` compila sem erros no Windows | ✅ |
| VST3 gerado como artefato | ✅ |
| Standalone gerado como artefato | ✅ |
| `processBlock()` não aloca memória | ✅ |
| Testes unitários compilam sem JUCE | ✅ |
| Todos os testes passam | ✅ |

---

## Próximo passo

**Fase 1 — Detecção de tonalidade via MIDI**

`KeyEstimator` e `MidiAnalyzer` já estão funcionais desde esta fase. O trabalho da Fase 1 será conectá-los no `processBlock()` com os testes de aceitação definidos no roadmap:

- C major scale → `{ root=0, mode="major" }`
- A natural minor → `{ root=9, mode="minor" }` com A enfatizado
- Progressão ii-V-I em G → converge para G major após ~4 compassos
- `getActiveNotes()` reflete exatamente as notas com note-on pendente
