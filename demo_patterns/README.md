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

## ⚠️ Por qué solo se veían 6 patrones (resuelto)

Dos cosas tapaban el banco, ya arregladas en `s3_presets_melody.patch`:

1. **Autoload desactivado.** El S3 arrancaba con 6 patrones *inline* hardcodeados
   (HIP HOP, TECHNO, DnB, BREAK, HOUSE, TRAP); el autoload de bancos estaba
   apagado a propósito. El parche lo **activa** → al boot carga
   `19_temas_demo_daisy.json` (con fallback a los inline si el JSON no está).
2. **Nombres del selector hardcodeados.** La web fijaba el nº de patrones en
   `PATTERN_NAMES` (6) → `totalPatterns = max(len, 6)` = 6. El parche pone los
   **19 nombres del demo** en `data/web/app.js`.

## Instalación (3 pasos)

```bash
# 1) S3: parche (autoload + nombres web + melodia + presets) y JSON
cd RedMaster-ESP32S3 && git apply /ruta/s3_presets_melody.patch
copy /ruta/19_temas_demo_daisy.json data/patterns/
pio run -t upload          # firmware
pio run -t uploadfs        # JSON + web nuevo  ← IMPRESCINDIBLE

# 2) P4: bump de patrones 16->19
cd BlueSlaveP4 && git apply /ruta/p4_19_patterns.patch
pio run -t upload

# 3) Daisy: ya en este repo (DSQ_PATTERNS=19). build + flash.
```

Al reiniciar el master, el log mostrará `Banco demo '19_temas_demo_daisy.json'
cargado (19 patrones)` y la web/P4 enseñarán los 19 temas del demo.

### Carga manual (sin recompilar)

Con el JSON ya en LittleFS puedes cargarlo por URL, aunque la web seguirá
mostrando los 6 nombres viejos si no aplicas el cambio de `app.js`:
```
http://<IP-master>/api/patternBanks
http://<IP-master>/api/patternBank/load?file=19_temas_demo_daisy.json
```

## Detalle de los parches del S3

`s3_presets_melody.patch` toca 5 ficheros (≈98 líneas):

- **`Sequencer.h`**: `PatternData` gana `trackEngine[][]` y `trackPreset[][]`; 4 setters/getters.
- **`Sequencer.cpp`**: init a `-1`/`0` en constructor y `clearPattern`; implementación de los métodos.
- **`WebInterface.cpp`**: `loadPatternBankFromFs` hace una 2ª pasada que lee
  `engine`/`preset`/`notes`/`flags` por track (tras `setPatternBulk`, que limpia notas).
- **`main.cpp`**: **autoload del banco al boot** + al activar patrón reaplica engine + preset.
- **`data/web/app.js`**: `PATTERN_NAMES` con los 19 nombres del demo.

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
