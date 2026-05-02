# KeyDetectorVST — Regras do Agente

## Stack
- C++17 · JUCE 7 · CMake 3.22+ · VST3/AU
- Compilador: MSVC (Windows) / Clang (macOS)
- Modelo preferido: Claude Sonnet 4.6 (lógica complexa)
- Modelo rápido: Gemini Flash (boilerplate/docs)

## Arquitetura — ordem de dependência
MidiAnalyzer → KeyEstimator → ScaleClassifier 
→ ProgressionSuggester → PluginProcessor → PluginEditor

## Convenções obrigatórias
- `#pragma once` em todos os headers
- Prefixo `_` para membros privados: `_histogram`
- NUNCA alocar memória no `processBlock()`
- UI updates SEMPRE via `juce::Timer` (nunca na thread de áudio)
- `juce::AbstractFifo` para passar dados audio→UI (thread safety)
- Sem exceções C++ (JUCE não usa)

## Fases do projeto
- Fase 0: CMake + scaffolding ← ATUAL
- Fase 1: Detecção MIDI (Krumhansl-Schmuckler)
- Fase 2: Detecção áudio (chroma + YIN)
- Fase 3: Classificação de escala/modo
- Fase 4: Sugestão de progressão (Markov)
- Fase 5: UI (roda de quintas + LEDs)

## Regras do agente
- Sempre perguntar antes de criar arquivos fora da estrutura definida
- Compilar e reportar erros antes de prosseguir para próxima fase
- Testes unitários obrigatórios para KeyEstimator e ScaleClassifier
- Não modificar PluginProcessor sem avisar — é a thread crítica de áudio