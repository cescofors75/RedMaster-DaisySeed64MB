# 19 Temas Demo Daisy → Patrones con ritmo + melodía + presets

Banco de **19 patrones** extraído del *demo* de arranque de la Daisy
(`DaisySeed/main.cpp` → `RunStartup808SelfTest`, fase `PH_SYNTH_JAM`) y puesto
a funcionar end-to-end en los tres equipos: **Daisy** (audio), **ESP32-S3**
(master) y **ESP32-P4** (panel táctil).

```
P4 7" (UI) ──UDP──► ESP32-S3 (master, secuenciador) ──SPI──► Daisy (audio)
                          ▲ WebSocket (web propia)
```

## Contenido de la carpeta

| Fichero | Qué es | Dónde se aplica |
|---|---|---|
| `19_temas_demo_daisy.json` | Banco de 19 patrones (ritmo + melodía 303 + preset por track) | `data/patterns/` del **S3** |
| `generate_demo_patterns.py` | Generador reproducible del JSON | — |
| `s3_presets_melody.patch` | Lee melodía/engine/preset y los aplica al cambiar de patrón | repo **RedMaster-ESP32S3** |
| `p4_19_patterns.patch` | Sube el tope de patrones 16→19 | repo **BlueSlaveP4** |
| (Daisy) `DSQ_PATTERNS 16→19` | Ya commiteado en este repo | **este repo** |

## Qué hace cada capa (estado tras los parches)

| | Ritmos | Melodías | Presets | Cambio |
|---|---|---|---|---|
| **Daisy** | ✅ | ✅ `0xC7` | ✅ `0xC6` | `DSQ_PATTERNS` 16→19 (este repo) |
| **S3** | ✅ | ✅ loader lee `notes`/`flags` | ✅ preset **por patrón** | `s3_presets_melody.patch` |
| **P4** | ✅ | ✅ | ✅ | `p4_19_patterns.patch` |

## Los 19 patrones

| slot | nombre | contenido |
|---|---|---|
| 0–4 | TECHNO FULL/BUILD/DRUMS/ACID/BREAK | drums + 303 (jamNotes) |
| 5–9 | ELECTRO * | jamNotesElectro |
| 10–13 | AMBIENT * | jamNotesAmbient |
| 14–16 | ACID UP / DOWN / OCTAVE | escala notes303 |
| 17–18 | TOM FILL / SNARE ROLL | fills |

\+ `songChain` de 9 entradas (Techno → Electro → Ambient).

## Esquema JSON (extendido)

```jsonc
{
  "name": "...", "tempo": 124, "stepCount": 16, "selectPattern": 0,
  "trackEngines": [0,1,2,1,1,0,2,3,0,0,0,-1,-1,-1,-1,-1],  // mapa de referencia
  "patterns": [
    { "slot": 0, "name": "TECHNO FULL", "tracks": [
        { "track": 0, "engine": 0, "preset": 2, "steps": [...16], "velocities": [...16] },
        { "track": 7, "engine": 3, "preset": 0, "steps": [...], "velocities": [...],
          "notes": [36,36,43,0,...],   // MIDI por paso, 0 = silencio
          "flags": [1,0,0,0,...] }     // bit0=accent, bit1=slide
    ]}
  ],
  "songChain": [ {"pattern":0,"repeats":4}, ... ]
}
```

Campos nuevos respecto a `10_temas_referencia_808.json`:
`engine`, `preset`, `notes`, `flags` por track.

## Presets por patrón

Cada track recuerda su **engine** + **preset de fábrica**; el S3 los reaplica al
activar el patrón (`CMD_SYNTH_PRESET 0xC6` + `dsqSetTrackEngine`). Presets por
estilo (ver generador): Techno→808 *Techno*/909 *Techno*/303 *Acid*; Electro→909
*HousePound*/303 *Squelch*; Ambient→909 *Industrial*/303 *Sub Bass*.

> ⚠️ **El engine global se sobreescribe al cambiar de patrón.** `gTrackSynthEngine[]`
> del S3 es global; el recall por patrón lo actualiza en cada switch. Es el
> comportamiento deseado para este banco.

## Instalación (3 pasos)

```bash
# 1) S3: aplicar el parche de melodía + presets
cd RedMaster-ESP32S3 && git apply /ruta/s3_presets_melody.patch

# 2) P4: aplicar el bump de patrones
cd BlueSlaveP4 && git apply /ruta/p4_19_patterns.patch

# 3) Daisy: ya está (DSQ_PATTERNS=19 en este repo). Recompilar y flashear los 3.
```

Luego copia `19_temas_demo_daisy.json` a `data/patterns/` del S3, vuelca LittleFS
y cárgalo desde la web/UDP igual que `10_temas_referencia_808.json`.

## Detalle de los parches del S3

`s3_presets_melody.patch` toca 4 ficheros (85 líneas, solo añadidos):

- **`Sequencer.h`**: `PatternData` gana `trackEngine[][]` y `trackPreset[][]`; 4 setters/getters.
- **`Sequencer.cpp`**: init a `-1`/`0` en constructor y `clearPattern`; implementación de los métodos.
- **`WebInterface.cpp`**: `loadPatternBankFromFs` hace una 2ª pasada que lee
  `engine`/`preset`/`notes`/`flags` por track (tras `setPatternBulk`, que limpia notas).
- **`main.cpp`**: al final de `dsqUploadPattern` reaplica engine + preset de cada track.

## ⚠️ Notas de integración (verificar en hardware)

1. **Sin compilar contra el build real.** Los parches se han escrito sobre el
   HEAD clonado pero no se han compilado/flasheado. Revisa que compilan en tu
   toolchain antes de subir.
2. **Doble disparo melódico.** La melodía 303 la dispara el `stepCallback` del S3
   (`synthNoteOnEx`). Si la Daisy también dispara ese track desde su secuenciador
   interno, podría sonar doble. Es el **mismo flujo** que el "melodyAssign" del P4
   ya en producción, así que debería comportarse igual — pero conviene oírlo.
3. **P4 UI.** El bump deja seleccionar P01–P19; mostrar el preset activo por
   patrón en pantalla es una mejora opcional aún no incluida.

## Compatibilidad sin parchear el S3

El JSON sigue cargando sin el parche: sonarían solo las baterías
(`steps`+`velocities`); melodía y presets se ignorarían sin error.
