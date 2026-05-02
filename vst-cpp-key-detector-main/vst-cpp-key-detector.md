# VST Plugin — Key & Scale Detector (C++)

**Objetivo:** Plugin VST3/AU em C++ capaz de detectar tonalidade e escala em tempo real a partir de MIDI e/ou áudio, com display de notas ativas e sugestão de progressões contextuais.

**Stack:** C++17 · JUCE 7 · VST3 SDK (via JUCE) · FFTW3 (ou JUCE DSP)  
**Plataforma alvo:** Windows (VST3) + macOS (AU/VST3)  
**Estimativa total:** ~4–6 meses (desenvolvedor solo, meio período)

---

## Por que vale a pena incluir sugestão de progressão

Detecção de tonalidade isolada é uma feature de tuner avançado — útil, mas não retém usuário. Com a progressão integrada, o plugin fecha o loop criativo:

```
Você toca → plugin detecta tonalidade → sugere próximo acorde contextual
```

Isso transforma o plugin de ferramenta analítica em ferramenta composicional, que é o mercado mais valioso. A lógica de progressão pode ser alimentada pela mesma matriz de Markov do MIDI Harmonizer — reuso direto de trabalho já planejado.

---

## Arquitetura Geral

```
┌─────────────────────────────────────────────────────┐
│                    VST Plugin Host                   │
│  (DAW: Ableton / Logic / Reaper / etc.)             │
└──────────────┬──────────────────┬───────────────────┘
               │ MIDI Input        │ Audio Input
               ▼                  ▼
┌──────────────────┐  ┌──────────────────────────────┐
│  MidiAnalyzer    │  │       AudioAnalyzer           │
│                  │  │                               │
│ • PitchClassHist │  │ • ChromaExtractor (FFT)       │
│ • ChordDetector  │  │ • PitchDetector (YIN/pYIN)    │
│ • NoteTracker    │  │ • OnsetDetector               │
└────────┬─────────┘  └──────────────┬────────────────┘
         │                           │
         └─────────────┬─────────────┘
                       ▼
          ┌────────────────────────┐
          │    KeyEstimator        │
          │                        │
          │ Krumhansl-Schmuckler   │
          │ (24 correlações)       │
          │ → root + mode          │
          └────────────┬───────────┘
                       ▼
          ┌────────────────────────┐
          │    ScaleClassifier     │
          │                        │
          │ Compara pitch usage    │
          │ com 15 perfis de modo  │
          │ → Dorian, Mixolydian,  │
          │   Phrygian, etc.       │
          └────────────┬───────────┘
                       ▼
          ┌────────────────────────┐
          │  ProgressionSuggester  │
          │                        │
          │ Markov lookup table    │
          │ (JSON bundled)         │
          │ → 3 próximos acordes   │
          │   com probabilidade    │
          └────────────┬───────────┘
                       ▼
          ┌────────────────────────┐
          │       Plugin UI        │
          │                        │
          │ • Roda de quintas      │
          │ • Notas ativas (12 LEDs│
          │ • Tonalidade detectada │
          │ • Escala + modo        │
          │ • 3 sugestões de acorde│
          └────────────────────────┘
```

---

## Fase 0 — Setup e Scaffolding (1–2 semanas)

### Dependências

```bash
# JUCE — framework VST/AU/AAX
git clone https://github.com/juce-framework/JUCE.git

# Projucer (gerador de projeto JUCE) ou CMake moderno
# Recomendado: CMake + juce_add_plugin()
```

### CMakeLists.txt mínimo

```cmake
cmake_minimum_required(VERSION 3.22)
project(KeyDetectorVST VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(JUCE)

juce_add_plugin(KeyDetector
    PLUGIN_MANUFACTURER_CODE Mids
    PLUGIN_CODE Kdtc
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Key Detector"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(KeyDetector PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/MidiAnalyzer.cpp
    src/AudioAnalyzer.cpp
    src/KeyEstimator.cpp
    src/ScaleClassifier.cpp
    src/ProgressionSuggester.cpp
)

target_link_libraries(KeyDetector PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)
```

### Estrutura de pastas

```
KeyDetectorVST/
├── CMakeLists.txt
├── JUCE/                    ← git submodule
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
└── tests/
    ├── test_key_estimator.cpp
    └── test_scale_classifier.cpp
```

### Critérios de aceitação — Fase 0
- [ ] `cmake --build` compila sem erros no Windows e macOS
- [ ] Plugin carrega no Reaper como VST3 Standalone
- [ ] `PluginProcessor::processBlock()` é chamado sem crash com buffer vazio

---

## Fase 1 — Detecção de Tonalidade via MIDI (2–3 semanas)

### Algoritmo: Krumhansl-Schmuckler

Baseia-se nos perfis psicoacústicos de Krumhansl & Kessler (1982) — cada grau da escala tem um peso que reflete sua estabilidade perceptual na tonalidade.

```cpp
// src/KeyEstimator.h
#pragma once
#include <array>
#include <string>

struct KeyResult {
    int root;          // 0–11 (C=0, C#=1, ..., B=11)
    std::string mode;  // "major" | "minor"
    float confidence;  // correlação de Pearson (0.0–1.0)
};

class KeyEstimator {
public:
    KeyResult estimate(const std::array<float, 12>& pitchClassProfile) const;

private:
    // Perfis de Krumhansl-Kessler
    static constexpr std::array<float, 12> MAJOR_PROFILE = {
        6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
        2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
    };
    static constexpr std::array<float, 12> MINOR_PROFILE = {
        6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
        2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
    };

    float pearsonCorrelation(
        const std::array<float, 12>& x,
        const std::array<float, 12>& y,
        int shift
    ) const;
};
```

```cpp
// src/KeyEstimator.cpp
KeyResult KeyEstimator::estimate(const std::array<float, 12>& pcp) const {
    KeyResult best { 0, "major", -1.0f };

    for (int root = 0; root < 12; ++root) {
        float corrMajor = pearsonCorrelation(pcp, MAJOR_PROFILE, root);
        float corrMinor = pearsonCorrelation(pcp, MINOR_PROFILE, root);

        if (corrMajor > best.confidence)
            best = { root, "major", corrMajor };
        if (corrMinor > best.confidence)
            best = { root, "minor", corrMinor };
    }
    return best;
}
```

### MidiAnalyzer — acumulação de pitch classes

```cpp
// src/MidiAnalyzer.h
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

class MidiAnalyzer {
public:
    void processMidiBuffer(const juce::MidiBuffer& midi);
    void reset();

    std::array<float, 12> getNormalizedProfile() const;
    std::array<bool, 12>  getActiveNotes() const;   // notas soando agora

private:
    std::array<float, 12> _histogram {};   // acumulado ponderado por duração
    std::array<bool,  12> _heldNotes {};   // notas com note-on ativo
    int _totalNotes { 0 };

    // Janela de decaimento: notas antigas perdem peso
    static constexpr float DECAY_FACTOR = 0.995f;
};
```

**Decaimento temporal:** sem decaimento, o perfil fica "sujo" com notas de compassos anteriores. O fator `0.995f` por sample (ou por bloco) faz as notas antigas desaparecerem suavemente, mantendo a detecção responsiva.

### Critérios de aceitação — Fase 1
- [ ] C major scale (MIDI notas 60–72) → detecta `{ root=0, mode="major" }`
- [ ] A natural minor (69, 71, 72, 74, 76, 77, 79) → detecta `{ root=9, mode="minor" }`
- [ ] Progressão ii-V-I em G (Am7–D7–Gmaj7) → converge para G major após ~4 compassos
- [ ] Reset limpa o histograma e a detecção volta para estado neutro
- [ ] `getActiveNotes()` reflete exatamente as notas com note-on pendente

---

## Fase 2 — Detecção de Tonalidade via Áudio (3–4 semanas)

Esta fase é mais complexa. O pipeline é:

```
Audio buffer → FFT → Chroma features → KeyEstimator (mesmo da Fase 1)
```

### ChromaExtractor

```cpp
// src/AudioAnalyzer.h
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>

class AudioAnalyzer {
public:
    explicit AudioAnalyzer(double sampleRate, int fftOrder = 12); // 4096 samples
    
    void processAudioBlock(const float* samples, int numSamples);
    
    std::array<float, 12> getChromaVector() const;
    float getDetectedPitch() const;   // Hz, 0 se não detectado (polifônico)

private:
    juce::dsp::FFT _fft;
    juce::dsp::WindowingFunction<float> _window;

    std::array<float, 12> _chroma {};
    float _detectedPitch { 0.0f };
    double _sampleRate;

    // Mapeamento frequência → pitch class
    // f → MIDI note → pitch class (mod 12)
    int freqToPitchClass(float freqHz) const;

    // Acumulação ponderada por magnitude
    void updateChroma(const std::vector<float>& magnitudeSpectrum);
};
```

### Mapeamento frequência → chroma

```cpp
int AudioAnalyzer::freqToPitchClass(float freqHz) const {
    if (freqHz < 20.0f) return -1;  // abaixo do audível
    
    // MIDI note number: A4=69=440Hz
    float midiNote = 69.0f + 12.0f * std::log2(freqHz / 440.0f);
    int pitchClass = static_cast<int>(std::round(midiNote)) % 12;
    
    return (pitchClass + 12) % 12;  // garantir positivo
}
```

### Detecção de pitch monofônico: algoritmo YIN

Para linhas melódicas monofônicas (guitarra, voz, sintetizador solo):

```cpp
// Implementação simplificada do YIN (de Cheveigné & Kawahara, 2002)
float AudioAnalyzer::yinPitchDetect(const float* buffer, int bufferSize) {
    // 1. Difference function
    // 2. Cumulative mean normalized difference
    // 3. Absolute threshold (tau onde CMND < 0.1)
    // 4. Parabolic interpolation para precisão sub-sample
    // Retorna frequência em Hz, 0 se aperiódico
}
```

**Nota:** Para áudio polifônico (piano, guitarra com acordes), YIN não funciona — usa-se apenas o chroma vector para alimentar o KeyEstimator, sem tentar detectar pitch único.

### Critérios de aceitação — Fase 2
- [ ] Senoide pura em 440Hz (A4) → pitch class 9 com magnitude dominante no chroma
- [ ] C major scale tocada em piano (áudio real) → detecta C major com confiança > 0.7
- [ ] YIN detecta A4 (440Hz) com erro < 2 cents em sinal limpo
- [ ] Buffer de silêncio não produz pitch class espúrio

---

## Fase 3 — Classificação de Escala / Modo (1–2 semanas)

Após saber a tonalidade, identifica o **modo** com base nos graus usados.

```cpp
// src/ScaleClassifier.h
#pragma once
#include <array>
#include <string>
#include <vector>

struct ScaleResult {
    std::string name;     // "Dorian", "Mixolydian", "Blues", etc.
    float confidence;
    std::vector<int> formula;  // intervalos em semitons a partir da tônica
};

class ScaleClassifier {
public:
    ScaleResult classify(
        const std::array<float, 12>& pitchProfile,
        int rootClass
    ) const;

    std::vector<ScaleResult> rankAll(
        const std::array<float, 12>& pitchProfile,
        int rootClass,
        int topN = 3
    ) const;

private:
    struct ScaleProfile {
        std::string name;
        std::array<float, 12> weights;  // 1.0 = grau da escala, 0.0 = fora
    };

    // 15 perfis pré-definidos
    static const std::vector<ScaleProfile> PROFILES;
};
```

### Perfis incluídos

| Escala | Graus (semitons) |
|---|---|
| Major (Ionian) | 0,2,4,5,7,9,11 |
| Natural Minor (Aeolian) | 0,2,3,5,7,8,10 |
| Dorian | 0,2,3,5,7,9,10 |
| Phrygian | 0,1,3,5,7,8,10 |
| Lydian | 0,2,4,6,7,9,11 |
| Mixolydian | 0,2,4,5,7,9,10 |
| Locrian | 0,1,3,5,6,8,10 |
| Harmonic Minor | 0,2,3,5,7,8,11 |
| Melodic Minor | 0,2,3,5,7,9,11 |
| Major Pentatonic | 0,2,4,7,9 |
| Minor Pentatonic | 0,3,5,7,10 |
| Blues | 0,3,5,6,7,10 |
| Whole Tone | 0,2,4,6,8,10 |
| Diminished (HW) | 0,1,3,4,6,7,9,10 |
| Lydian Dominant | 0,2,4,6,7,9,10 |

---

## Fase 4 — Sugestão de Progressão (2–3 semanas)

### Fonte de dados

O arquivo `markov_transitions.json` é gerado pelo pipeline do MIDI Harmonizer (Sprint 04) e bundled no plugin. Estrutura mínima:

```json
{
  "major": {
    "I":   { "IV": 0.28, "V": 0.24, "vi": 0.18, "ii": 0.14, "iii": 0.08, "I": 0.05, "vii°": 0.03 },
    "ii":  { "V": 0.45, "IV": 0.20, "vii°": 0.15, "I": 0.10, "vi": 0.07, "ii": 0.03 },
    "V":   { "I": 0.52, "vi": 0.18, "IV": 0.12, "ii": 0.08, "V": 0.06, "iii": 0.04 },
    "IV":  { "I": 0.32, "V": 0.25, "ii": 0.18, "vi": 0.12, "IV": 0.08, "iii": 0.05 },
    "vi":  { "ii": 0.28, "IV": 0.25, "V": 0.20, "I": 0.15, "iii": 0.07, "vi": 0.05 },
    "iii": { "vi": 0.35, "IV": 0.25, "ii": 0.18, "I": 0.12, "V": 0.10 },
    "vii°":{ "I": 0.60, "V": 0.20, "iii": 0.12, "vi": 0.08 }
  },
  "minor": { ... },
  "dorian": { ... }
}
```

### ProgressionSuggester

```cpp
// src/ProgressionSuggester.h
#pragma once
#include <string>
#include <vector>

struct ChordSuggestion {
    std::string roman;      // "V7", "IV", "bVII"
    std::string name;       // "G7", "F", "Bb" (calculado da tonalidade)
    float probability;
};

class ProgressionSuggester {
public:
    explicit ProgressionSuggester(const std::string& jsonPath);

    std::vector<ChordSuggestion> suggest(
        const std::string& currentRoman,
        const std::string& mode,
        int rootClass,
        int topN = 3
    ) const;

    // Inferir grau romano a partir das notas ativas
    std::string inferCurrentRoman(
        const std::array<bool, 12>& activeNotes,
        int rootClass,
        const std::string& mode
    ) const;

private:
    // { mode → { roman → { next_roman → probability } } }
    std::map<std::string, std::map<std::string, std::map<std::string, float>>> _table;
    
    std::string romanToChordName(const std::string& roman, int rootClass, const std::string& mode) const;
};
```

---

## Fase 5 — Interface Gráfica (2–3 semanas)

### Layout proposto

```
┌──────────────────────────────────────────────────┐
│  KEY DETECTOR                        [MIDI] [AUDIO]│
├──────────────────────────────────────────────────┤
│                                                  │
│           [  Roda de Quintas  ]                  │
│           C destacado = tônica                   │
│           LEDs nos graus da escala               │
│                                                  │
├──────────────────────────────────────────────────┤
│  Tonalidade:  C  MAJOR    Modo: Ionian           │
│  Confiança:   ████████░░  82%                    │
├──────────────────────────────────────────────────┤
│  Notas ativas:  C  ·  E  ·  G  ·  ·  ·  ·  ·   │
│                 (12 LEDs cromáticos)             │
├──────────────────────────────────────────────────┤
│  PROGRESSÃO SUGERIDA           [acorde atual: I] │
│                                                  │
│  ① IV   F major      ███████░░░  28%            │
│  ② V7   G dominant   ██████░░░░  24%            │
│  ③ vi   A minor      █████░░░░░  18%            │
│                                                  │
│  [Send MIDI ①]  [Send MIDI ②]  [Send MIDI ③]   │
└──────────────────────────────────────────────────┘
```

**"Send MIDI":** ao clicar, o plugin emite um note-on com as notas do acorde sugerido para a saída MIDI da DAW. Isso permite acionar um instrumento virtual diretamente.

### Componentes JUCE relevantes

| Componente | Uso |
|---|---|
| `juce::Component` | Base de todos os elementos |
| `juce::Timer` | Refresh da UI a 30fps |
| `juce::Path` | Desenhar a roda de quintas |
| `juce::MidiOutput` | Emitir acordes sugeridos |
| `juce::Colour` | Highlight de notas ativas |

---

## Roadmap Resumido

| Fase | Conteúdo | Duração Est. | Entregável |
|---|---|---|---|
| 0 | Setup JUCE, CMake, scaffolding | 1–2 sem | Plugin vazio carregando na DAW |
| 1 | Detecção MIDI (Krumhansl-Schmuckler) | 2–3 sem | Tonalidade via MIDI funcionando |
| 2 | Detecção áudio (chroma + YIN) | 3–4 sem | Tonalidade via áudio funcionando |
| 3 | Classificação de modo/escala | 1–2 sem | "Dorian", "Mixolydian" identificados |
| 4 | Sugestão de progressão | 2–3 sem | 3 acordes sugeridos com probabilidade |
| 5 | UI polida | 2–3 sem | Roda de quintas + LEDs + botões MIDI out |

**Total:** 11–17 semanas (2.5–4 meses em ritmo dedicado, 5–8 meses meio período)

---

## Integração com o MIDI Harmonizer

Este plugin e o MIDI Harmonizer compartilham:

| Componente | MIDI Harmonizer (Python) | VST Plugin (C++) |
|---|---|---|
| Normalização de acordes | `chord_normalizer.py` | Reimplementar em `ChordNormalizer.cpp` |
| Matrizes de Markov | `markov_engine.py` → Supabase/SQLite | `markov_transitions.json` bundled |
| Tonalidade | `roman_annotator.py` | `KeyEstimator.cpp` (Krumhansl-Schmuckler) |

O JSON de transições pode ser exportado diretamente pelo script `export_sqlite.py` do Sprint 05, criando um arquivo leve para bundar no VST sem depender de Supabase.

---

## Referências Técnicas

| Recurso | Uso |
|---|---|
| Krumhansl & Kessler (1982) — *Tracing the dynamic changes in perceived tonal organization* | Perfis de tonalidade usados no KeyEstimator |
| Cheveigné & Kawahara (2002) — *YIN, a fundamental frequency estimator* | Algoritmo de pitch detection |
| Müller (2015) — *Fundamentals of Music Processing* (Cap. 3) | Extração de features chroma do áudio |
| JUCE Documentation — `juce.com/learn` | Framework VST/AU |
| The Audio Programmer (YouTube) | Tutoriais JUCE para iniciantes em C++ de áudio |
