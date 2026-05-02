# Plano de Sprints — KeyDetectorVST

**Início:** 2026-04-21  
**Ritmo:** meio período (~10h/semana)  
**Total estimado:** 14–20 sprints de 1 semana

> Cada sprint tem tarefas, critérios de "done" verificáveis e dívidas técnicas conhecidas.

---

## Sprint 01 — MidiAnalyzer + testes de integração MIDI
**Fase:** 1 | **Duração est.:** 1 semana

### Objetivo
Fechar o loop MIDI → KeyEstimator no `processBlock()` e validar com testes automatizados.

### Tarefas
- [ ] Adicionar decay por bloco (não por mensagem) em `MidiAnalyzer::processMidiBuffer`
- [ ] Escrever `test_midi_analyzer.cpp`: note-on/off, decay, reset, notas ativas
- [ ] Testar integração MidiAnalyzer → KeyEstimator com fixture de C major (MIDI 60–72)
- [ ] Verificar `processBlock()` com buffer MIDI vazio não produz crash

### Critérios de done
- [ ] C major scale (MIDI 60–72) → `{ root=0, mode="major" }`
- [ ] `getActiveNotes()` reflete exatamente notas com note-on pendente
- [ ] Reset zera histograma e active notes

### Dívidas técnicas conhecidas
- Decay está sendo aplicado por mensagem MIDI (dentro do loop); deveria ser uma vez por bloco antes do loop para ser proporcional ao tempo real

---

## Sprint 02 — ScaleClassifier + modo na detecção MIDI
**Fase:** 1–3 | **Duração est.:** 1 semana

### Objetivo
Expor `ScaleResult` na saída do `PluginProcessor` e validar os 15 modos.

### Tarefas
- [ ] Adicionar `getLastScaleResult()` exposto via `PluginProcessor` (já existe stub)
- [ ] Escrever testes para Dorian, Mixolydian e Blues (profiles sintéticos)
- [ ] Verificar que `rankAll(..., topN=3)` retorna os 3 modos mais prováveis em ordem

### Critérios de done
- [ ] D Dorian (0,2,3,5,7,9,10 a partir de D=2) → `ScaleResult.name == "Dorian"`
- [ ] G Mixolydian → `"Mixolydian"` no top-1
- [ ] Minor Pentatonic vs Natural Minor: perfil só com {0,3,5,7,10} → Pentatonic no top-1

---

## Sprint 03 — AudioAnalyzer: extração de chroma via FFT
**Fase:** 2 | **Duração est.:** 2 semanas

### Objetivo
Implementar a extração de chroma features a partir de blocos de áudio usando a FFT do JUCE DSP.

### Tarefas
- [ ] Implementar buffer circular interno em `AudioAnalyzer` para acumular amostras até `fftSize`
- [ ] Aplicar janela Hann e executar FFT
- [ ] Implementar `updateChroma()`: mapear bins FFT → pitch class via `freqToPitchClass()`
- [ ] Acumular magnitudes por pitch class (normalizado)
- [ ] Escrever `test_audio_analyzer.cpp`: senoide sintética → pitch class esperado

### Critérios de done
- [ ] Senoide 440Hz (A4) → pitch class 9 com magnitude dominante no chroma vector
- [ ] Buffer de silêncio → chroma vector zerado (sem pitch class espúrio)
- [ ] Senoide 261.63Hz (C4) → pitch class 0 dominante

### Notas de implementação
- `processAudioBlock` recebe blocos menores que `fftSize` — buffer circular obrigatório
- Nenhuma alocação no `processBlock()`: o buffer circular deve ser pré-alocado no `prepareToPlay()`

---

## Sprint 04 — YIN pitch detection (monofônico)
**Fase:** 2 | **Duração est.:** 1–2 semanas

### Objetivo
Implementar o algoritmo YIN para detecção de pitch monofônico (guitarra solo, voz, sintetizador).

### Tarefas
- [ ] Implementar função difference em `AudioAnalyzer`
- [ ] Implementar cumulative mean normalized difference (CMND)
- [ ] Threshold absoluto (τ onde CMND < 0.1)
- [ ] Interpolação parabólica para precisão sub-sample
- [ ] Expor `getDetectedPitch()` em Hz

### Critérios de done
- [ ] A4 (440Hz) detectado com erro < 2 cents em sinal limpo
- [ ] Buffer de silêncio → `getDetectedPitch() == 0.0f`
- [ ] Sinal polifônico (soma de senoides) → 0.0f (YIN não tenta detectar)

### Notas de implementação
- YIN exige buffer de pelo menos 2x o período máximo esperado (~2048 samples a 44.1kHz para 20Hz)
- Para áudio polifônico, manter o chroma vector mas zerar `_detectedPitch`

---

## Sprint 05 — Fusão MIDI + Áudio no KeyEstimator
**Fase:** 2 | **Duração est.:** 1 semana

### Objetivo
Combinar o perfil MIDI e o chroma de áudio em um único PCP antes de passar para o `KeyEstimator`.

### Tarefas
- [ ] Adicionar parâmetro de blend `midiWeight / audioWeight` no `PluginProcessor`
- [ ] Implementar `blendProfiles(midiPCP, audioPCP, weight)` — média ponderada normalizada
- [ ] Expor o modo (MIDI-only / Audio-only / Blend) como `juce::AudioParameterChoice`

### Critérios de done
- [ ] Modo MIDI-only: comportamento idêntico ao Sprint 01
- [ ] Modo Audio-only: chroma vector alimenta KeyEstimator diretamente
- [ ] Modo Blend: resultado é média ponderada, normalizada

---

## Sprint 06 — ProgressionSuggester: parser JSON
**Fase:** 4 | **Duração est.:** 1 semana

### Objetivo
Carregar `markov_transitions.json` e popular a tabela `_table` do `ProgressionSuggester`.

### Tarefas
- [ ] Implementar parser via `juce::JSON` (sem dependência externa)
- [ ] Implementar `suggest()` com lookup na tabela e ordenação por probabilidade
- [ ] Implementar `romanToChordName()`: converter "V" + root=7 (G) → "D"
- [ ] Escrever `test_progression_suggester.cpp`

### Critérios de done
- [ ] `suggest("I", "major", 0, 3)` → top-3 são IV, V, vi com probabilidades corretas
- [ ] `suggest("V", "major", 7, 1)` → "I" com p≈0.52 (G major → D major)
- [ ] Arquivo JSON ausente → log de erro, tabela vazia, nenhum crash

---

## Sprint 07 — inferCurrentRoman + integração com active notes
**Fase:** 4 | **Duração est.:** 1 semana

### Objetivo
Implementar a identificação do grau romano atual baseado nas notas ativas.

### Tarefas
- [ ] Implementar `inferCurrentRoman()`: comparar `activeNotes` com tríades/tétrades da tonalidade
- [ ] Definir tabela interna de acordes por modo (I=tríade maior, ii=menor, etc.)
- [ ] Integrar no `processBlock()`: `_progressionSuggester->suggest(roman, ...)`
- [ ] Expor `getLastSuggestions()` no `PluginProcessor`

### Critérios de done
- [ ] Notas C+E+G ativas, root=0, mode=major → roman="I"
- [ ] Notas D+F+A ativas, root=0, mode=major → roman="ii"
- [ ] Notas ambíguas → roman="?" sem crash

---

## Sprint 08 — UI: layout base + notas ativas (12 LEDs)
**Fase:** 5 | **Duração est.:** 1–2 semanas

### Objetivo
Criar a estrutura visual do plugin com as 12 notas cromáticas como LEDs.

### Tarefas
- [ ] Definir layout geral: header / área central / painel de progressão
- [ ] Implementar componente `NoteLEDBar`: 12 retângulos com estado on/off
- [ ] Colorir LEDs: cor base = cinza escuro, ativo = cor da tonalidade, fora da escala = vermelho suave
- [ ] Conectar `PluginProcessor::getActiveNotes()` ao `timerCallback()`

### Critérios de done
- [ ] 12 LEDs visíveis e atualizando a 30fps sem flickering
- [ ] Nota C ativa → LED índice 0 acende
- [ ] Mudança de tonalidade reflete nova paleta de cores nos graus da escala

---

## Sprint 09 — UI: roda de quintas
**Fase:** 5 | **Duração est.:** 2 semanas

### Objetivo
Desenhar a roda de quintas com `juce::Path` e destacar a tonalidade detectada.

### Tarefas
- [ ] Implementar `CircleOfFifthsComponent` como `juce::Component`
- [ ] Desenhar 12 segmentos com nome de nota e relativo menor
- [ ] Highlight do segmento correspondente à tonalidade detectada
- [ ] Highlight mais fraco para os graus da escala ativa
- [ ] Animação suave de transição de tonalidade (fade entre estados)

### Critérios de done
- [ ] Roda visível e dimensionada corretamente em 400×400px
- [ ] Mudança de C major → G major → acende segmento G
- [ ] Segmentos Dó, Ré, Mi, Fá, Sol, Lá, Si ficam "dentro da escala" com cor diferente

---

## Sprint 10 — UI: display de tonalidade + escala + confiança
**Fase:** 5 | **Duração est.:** 1 semana

### Objetivo
Exibir textualmente a tonalidade, modo, escala e barra de confiança.

### Tarefas
- [ ] Label "C MAJOR / Ionian" atualizado pelo timer
- [ ] Barra de confiança (Pearson 0.0–1.0 → 0–100%)
- [ ] Exibir top-3 modos como texto secundário (`rankAll(..., 3)`)
- [ ] Nome de nota correto (C#/Db com enarmônico contextual)

### Critérios de done
- [ ] Confiança 0.82 → barra em 82%
- [ ] `root=6, mode="major"` → exibe "F# / Gb Major" (enarmônico configurável)

---

## Sprint 11 — UI: painel de progressão sugerida + Send MIDI
**Fase:** 5 | **Duração est.:** 1–2 semanas

### Objetivo
Exibir as 3 sugestões de próximo acorde e permitir envio via MIDI output.

### Tarefas
- [ ] Componente `ProgressionPanel`: 3 linhas com roman numeral, nome, barra de probabilidade
- [ ] Botões "Send MIDI ①②③": emitir note-on das notas do acorde sugerido
- [ ] Calcular notas do acorde a partir de `ChordSuggestion.roman` + `rootClass`
- [ ] Usar `juce::MidiOutput` (ou `MidiBuffer` de saída do `processBlock`) para emissão

### Critérios de done
- [ ] 3 sugestões visíveis e atualizando com mudança de acorde tocado
- [ ] Clicar "Send MIDI ①" → note-on das notas corretas na saída MIDI da DAW
- [ ] Probabilidades somam ≤ 1.0 e barras são proporcionais

---

## Sprint 12 — Testes de integração end-to-end
**Fase:** todas | **Duração est.:** 1 semana

### Objetivo
Validar o pipeline completo com testes automatizados de alto nível.

### Tarefas
- [ ] `test_integration.cpp`: simular buffer MIDI com progressão Am-Dm-E-Am e verificar detecção final
- [ ] Testar reset mid-session: tonalidade anterior não contamina nova sessão
- [ ] Testar processBlock com buffers de diferentes tamanhos (64, 256, 512, 2048 samples)
- [ ] Medir tempo médio de `processBlock()` — deve ser < 1ms a 44.1kHz/512 samples

### Critérios de done
- [ ] Pipeline Am-Dm-E-Am → detecta A minor após 4 ciclos de acorde
- [ ] Tempo de processBlock < 1ms (médio de 100 chamadas)
- [ ] Zero falhas em Valgrind/ASAN (se disponível no CI)

---

## Sprint 13 — Parâmetros VST3 + state save/load
**Fase:** transversal | **Duração est.:** 1 semana

### Objetivo
Expor parâmetros controlável pela DAW e persistir o estado do plugin.

### Tarefas
- [ ] `AudioParameterChoice`: modo de entrada (MIDI / Audio / Blend)
- [ ] `AudioParameterFloat`: sensitivity de detecção (decay factor)
- [ ] `AudioParameterBool`: mostrar modo relativo menor na UI
- [ ] Implementar `getStateInformation` / `setStateInformation` com `juce::ValueTree`

### Critérios de done
- [ ] Preset salvo no Reaper é restaurado ao reabrir o projeto
- [ ] Automação de blend MIDI/Audio funciona em tempo real

---

## Sprint 14 — Release Windows + macOS
**Fase:** final | **Duração est.:** 1 semana

### Objetivo
Build de release otimizado para distribuição.

### Tarefas
- [ ] Build Release no Windows: `cmake --build --config Release`
- [ ] Testar VST3 no Reaper, Ableton, FL Studio
- [ ] (macOS) Build AU + VST3, testar no Logic Pro e GarageBand
- [ ] Criar instalador (NSIS no Windows, .pkg no macOS) ou zip com instruções

### Critérios de done
- [ ] Plugin carrega e processa áudio sem crash em 3 DAWs diferentes
- [ ] Latência de UI < 33ms (1 frame a 30fps)
- [ ] Build CI passa no GitHub Actions (Windows + macOS matrix)

---

## Resumo do roadmap

| Sprint | Conteúdo | Fase | Duração est. |
|---|---|---|---|
| 01 | MidiAnalyzer + integração MIDI | 1 | 1 sem |
| 02 | ScaleClassifier + modo MIDI | 1–3 | 1 sem |
| 03 | AudioAnalyzer: chroma via FFT | 2 | 2 sem |
| 04 | YIN pitch detection | 2 | 1–2 sem |
| 05 | Fusão MIDI + Áudio | 2 | 1 sem |
| 06 | ProgressionSuggester: JSON | 4 | 1 sem |
| 07 | inferCurrentRoman + active notes | 4 | 1 sem |
| 08 | UI: layout + 12 LEDs | 5 | 1–2 sem |
| 09 | UI: roda de quintas | 5 | 2 sem |
| 10 | UI: tonalidade + confiança | 5 | 1 sem |
| 11 | UI: progressão + Send MIDI | 5 | 1–2 sem |
| 12 | Testes end-to-end | todas | 1 sem |
| 13 | Parâmetros VST3 + state | transversal | 1 sem |
| 14 | Release Windows + macOS | final | 1 sem |
| **Total** | | | **16–21 semanas** |

---

## Dependências entre sprints

```
S01 ──► S02 ──────────────────────────► S12
S03 ──► S04 ──► S05 ──► S06 ──► S07 ──► S12
                         S07 ──► S08 ──► S09 ──► S10 ──► S11
                                                    S13 ──► S14
```

Os sprints 03–05 (áudio) e 01–02 (MIDI) podem ser desenvolvidos em paralelo caso haja dois contextos de trabalho disponíveis.
