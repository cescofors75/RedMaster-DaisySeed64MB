# 19 Temas Demo Daisy → Patrones (S3)

Banco de **19 patrones** extraído del *demo* de arranque de la Daisy
(`DaisySeed/main.cpp` → `RunStartup808SelfTest`, fase `PH_SYNTH_JAM`) y
convertido al formato JSON que carga el master **ESP32-S3**.

- **Generador:** `generate_demo_patterns.py` (reproducible)
- **Salida:** `19_temas_demo_daisy.json`

```bash
cd demo_patterns
python3 generate_demo_patterns.py
```

## Qué contiene

Las 3 líneas de bajo **303** del demo son melodías paso a paso y se vuelcan
íntegras a los campos de nota. Los hits de batería siguen exactamente las
reglas del firmware (`st%4==0`, etc.).

| slot | nombre | contenido |
|---|---|---|
| 0 | TECHNO FULL | drums + 303 (jamNotes) |
| 1 | TECHNO BUILD | + open-hat |
| 2 | TECHNO DRUMS | solo percusión |
| 3 | TECHNO ACID | kick + 303 |
| 4 | TECHNO BREAK | hats + clap + 303 |
| 5–9 | ELECTRO * | jamNotesElectro |
| 10–13 | AMBIENT * | jamNotesAmbient |
| 14 | ACID RUN UP | escala notes303 ascendente |
| 15 | ACID RUN DOWN | descendente |
| 16 | ACID OCTAVE | saltos de octava |
| 17 | TOM FILL | cascada de toms |
| 18 | SNARE ROLL | redoble crescendo |

Más un `songChain` de 9 entradas que encadena un set Techno → Electro → Ambient.

## Esquema JSON (extendido)

Igual que `10_temas_referencia_808.json` **más** campos de melodía/engine:

```jsonc
{
  "name": "...", "tempo": 124, "stepCount": 16, "selectPattern": 0,
  "trackEngines": [0,1,2,1,1,0,2,3,0,0,0,-1,-1,-1,-1,-1],  // GLOBAL (ver abajo)
  "patterns": [
    { "slot": 0, "name": "TECHNO FULL", "tracks": [
        { "track": 0, "engine": 0, "steps": [...16], "velocities": [...16] },
        { "track": 7, "engine": 3, "steps": [...], "velocities": [...],
          "notes": [36,36,43,0,...],   // MIDI por paso, 0 = silencio
          "flags": [1,0,0,0,...] }     // bit0=accent, bit1=slide
    ]}
  ],
  "songChain": [ {"pattern":0,"repeats":4}, ... ]
}
```

Campos `notes`/`flags`/`engine`/`trackEngines` son **nuevos**; `steps` y
`velocities` ya existían.

## ⚠️ Importante: requiere ampliar el loader del S3

El loader actual **`loadPatternBankFromFs`** (`RedMaster_ESP32S3/src/WebInterface.cpp`)
solo lee `steps` y `velocities` — **ignora `notes`, `flags` y los engines**. Si
cargas este JSON tal cual, sonarán las baterías pero **no la melodía 303**.

### Mapa track → engine es GLOBAL, no por patrón

En el firmware S3, `gTrackSynthEngine[16]` (`main.cpp:88`) es global. Por eso el
banco define **un único** `trackEngines` y todos los patrones lo respetan:

```
0 BD(808) · 1 SD(909) · 2 CH(505) · 3 OH(909) · 4 CY(909) · 5 CP(808)
6 CB(505) · 7 303 acid · 8 LT(808) · 9 MT(808) · 10 HT(808) · 11-15 sampler
```

### Parche del loader (S3)

En `WebInterface.cpp`, función `loadPatternBankFromFs`:

**1) Tras leer `stepCount` (~línea 594), aplicar los engines globales:**

```cpp
// --- NUEVO: engines globales por track ---
JsonArrayConst trackEngines = doc["trackEngines"].as<JsonArrayConst>();
if (!trackEngines.isNull()) {
  for (int t = 0; t < MAX_TRACKS && t < (int)trackEngines.size(); t++) {
    setTrackSynthEngine(t, (int8_t)(trackEngines[t] | -1));
    spiMaster.dsqSetTrackEngine((uint8_t)t, getTrackSynthEngine(t));
  }
}
```

**2) Tras `sequencer.setPatternBulk(slot, stepsData, velsData);` (~línea 635),
añadir una segunda pasada para notas y flags** (después del bulk porque
`clearPattern` borra las notas):

```cpp
// --- NUEVO: melodia por paso (notes) + accent/slide (flags) ---
for (JsonObjectConst trObj : tracks) {
  int track = trObj["track"] | -1;
  if (track < 0 || track >= MAX_TRACKS) continue;
  JsonArrayConst notes = trObj["notes"].as<JsonArrayConst>();
  JsonArrayConst flags = trObj["flags"].as<JsonArrayConst>();
  for (int step = 0; step < stepCount && step < STEPS_PER_PATTERN; step++) {
    if (!notes.isNull() && step < (int)notes.size()) {
      uint8_t n = (uint8_t)constrain((int)notes[step], 0, 127);
      if (n) sequencer.setStepNoteVoice(slot, track, step, 0, n);
    }
    if (!flags.isNull() && step < (int)flags.size()) {
      sequencer.setStepFlags(slot, track, step,
                             (uint8_t)((int)flags[step] & 0x03));
    }
  }
}
```

> `setStepNoteVoice`, `setStepFlags` y `setTrackSynthEngine` ya existen en el
> firmware (`Sequencer.cpp` y `main.cpp`). El parche solo los conecta al JSON.

## Instalación

1. Aplica el parche del loader en el repo `RedMaster-ESP32S3`.
2. Copia `19_temas_demo_daisy.json` a `data/patterns/` del S3 y vuelca LittleFS.
3. Cárgalo desde la web/UDP igual que `10_temas_referencia_808.json`.

> Las melodías se reproducen vía `CMD_SYNTH_NOTE_ON_EX (0xC7)` en tiempo real
> (el S3 lee `stepNoteVoices` al disparar cada paso); la Daisy no necesita
> cambios.

## Compatibilidad sin tocar el S3

Si **no** aplicas el parche, el JSON sigue cargando: sonarán las baterías
(`steps`+`velocities`) y los campos de melodía se ignoran sin error.
