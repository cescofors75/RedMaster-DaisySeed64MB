# RED808 JOURNEY — Guión del set en vivo

> Demo standalone para **Daisy Seed** (STM32H750 + 64 MB SDRAM)
> **132 BPM** · 1 compás = 1.818 s · 16 pasos/compás · paso = 5454 muestras @ 48 kHz
> **19 secciones · 328 compases · ~9:56 min** · loop infinito
> 7 motores: TR909 · TR808 · TR505 · TB303 · SH101 · FM2Op · WavetableOsc

---

## Mapa rápido del viaje (curva de energía)

```
energía
1.00 |                    ##        ##          ##  ##  ##
0.98 |              ##  ##    ##  ##    ##    ##            
0.96 |     ##  ##         ##              ##                
0.94 |  ##     ##                                          ##
0.92 |##                                                     
     +----------------------------------------------------------
      1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19
      intro---->acid--->garage/house--->break->trance->TRIBAL->BUILD->DROP->reset
```

- **Acto I** (1–5): nacimiento → primer groove → ácido que hierve
- **Acto II** (6–9): humano, callejero, house emotivo y funky
- **Acto III** (10–13): respiro cósmico → minimal → trance épico → tribal
- **Acto IV** (14–19): subida → climax → subida final → DROP → apoteosis → reset

---

## Tracklist con especificaciones

| # | Tiempo | Sección | Bars | Drum | Bass | Lead | Kick | Bass pat / Mel | fc (Hz) | Q | Rev | Dly | Swing | Flags | Salida (TMIX) |
|---|--------|---------|-----:|------|------|------|------|----------------|--------:|--:|----:|----:|------:|-------|---------------|
| 1 | 0:00 | **DETROIT INTRO** | 16 | 808 | 303 | FM | — | — / bells(0) | 420 | 0.82 | 0.80 | 0.45 | — | CRASH | WASH (8) |
| 2 | 0:29 | **DETROIT GROOVE** | 16 | 808 | 303 | FM | 4×4 | ostinato(0) / bells(0) | 420 | 0.82 | 0.72 | 0.45 | — | CRASH | WASH (6) |
| 3 | 0:58 | **BREAKDOWN** | 4 | 808 | 303 | FM | — | — / bells(0) | 420 | 0.82 | 0.90 | 0.55 | — | — | FILTER (2) |
| 4 | 1:05 | **ACID HOUSE** | 24 | 909 | 303 | FM | 4×4 | wormy(1) / arp(1) | 900 | 0.90 | 0.45 | 0.40 | — | CRASH | FILTER (6) |
| 5 | 1:49 | **ACID PEAK** | 16 | 909 | 303 | FM | 4×4 | wormy(1) / arp(1) | 1500 | 0.94 | 0.40 | 0.50 | — | — | ECHO (6) |
| 6 | 2:18 | **UK GARAGE 2-STEP** | 16 | 505 | SH101 | FM | 2-step | 2step-sub(6) / hook(7) | 780 | 0.55 | 0.62 | 0.55 | 22 | FUNK+CRASH | STRIP (4) |
| 7 | 2:47 | **ORGANIC HOUSE** | 16 | 808 | SH101 | **WT** | 4×4 | octaves(2) / stabs(6) | 650 | 0.55 | 0.72 | 0.55 | — | — | WASH (6) |
| 8 | 3:16 | **DEEP HOUSE** | 24 | 808 | SH101 | FM | 4×4 | octaves(2) / chords(2) | 800 | 0.70 | 0.55 | 0.45 | 18 | CRASH | FILTER (6) |
| 9 | 4:00 | **FUNKY ELECTRO** | 24 | 505 | 303 | FM | funk | slap(7) / — | 900 | 0.80 | 0.48 | 0.45 | 14 | FUNK | ECHO (6) |
| 10 | 4:44 | **MICRO-BREAK** | 4 | 909 | 303 | FM | — | — / bells(0) | 200 | 0.60 | 0.95 | 0.45 | — | — | corte |
| 11 | 4:51 | **MINIMAL TECHNO** | 20 | 505 | SH101 | **WT** | 4×4 | sparse(3) / ostinato(3) | 650 | 0.60 | 0.78 | 0.62 | — | **STUTTER** | 🤯 WALL+STUTTER (8) |
| 12 | 5:27 | **TRANCE SUPERSAW** | 28 | 909 | 303 | **WT** | 4×4 | rolling-oct(5) / arp(4) | 420 | 0.82 | 0.88 | 0.55 | — | CRASH | WASH (8) |
| 13 | 6:18 | **TRIBAL PERC** | 16 | 808 | 303 | **WT** | 4×4 | ostinato(0) / chords(2) | 500 | 0.70 | 0.60 | 0.50 | 8 | TOMS+CRASH | STRIP (6) |
| 14 | 6:47 | **BUILDUP** | 16 | 909 | 303 | FM | 4×4 | — / arp(4) | 420 | 0.82 | 0.92 | 0.55 | — | **BUILDUP** | slam |
| 15 | 7:16 | **PEAK DROP** | 32 | 909 | 303 | **WT** | 4×4 | peak(5) / **anthem(8)** | 1100 | 0.88 | 0.65 | 0.45 | — | CRASH | STRIP (4) |
| 16 | 8:14 | **FINAL BUILDUP** | 16 | 909 | 303 | **WT** | gallop | — / minimal(3) | 420 | 0.82 | 0.60 | 0.22 | — | **BUILDUP** | slam |
| 17 | 8:44 | **FINAL DROP** | 16 | 909 | 303 | FM | 4×4 | **driving-16(8)** / peak(5) | 1100 | 0.86 | 0.40 | 0.20 | — | **FINALE** | slam |
| 18 | 9:13 | **APOTEOSIS** | 16 | 909 | 303 | FM | 4×4 | driving-16(8) / **anthem(8)** | 1300 | 0.88 | 0.45 | 0.22 | — | **FINALE** | WASH (8) |
| 19 | 9:42 | **RESET** | 8 | 808 | 303 | FM | — | — / bells(0) | 420 | 0.82 | 0.88 | 0.55 | — | — | corte → loop |

> *(N)* = índice del patrón en `BASS_BANK` / `MEL_BANK`. **WT** = lead Wavetable (resto FM2Op).

---

## Modos de transición (TMIX) — qué hace cada salida

| Modo | Carácter físico | Automatización |
|------|-----------------|----------------|
| **FILTER** | EQ-out de DJ: el LPF del bajo cierra a 80 Hz (el sub desaparece, tensión). Al entrar la nueva sección reabre. | `bassCutoff → 80`, reverb +0.3 |
| **ECHO** | Echo throw: el delay crece sobre el groove intacto, el lead se disuelve en ecos. | `dlyFb → 0.85`, reverb +0.5 |
| **WASH** | Reverb wash: cola gigante que envuelve; los drums se ENVÍAN a la reverb (se ensanchan, no bajan). | `revFb → 0.93`, `drumWashSend → 0.22` |
| **STRIP** | Tensión rítmica: hats/clap fuera, queda el KICK solo. El bajo se abre en brillo. | hats off, `bassCutoff ×1.5`, reverb +0.5 |
| **slam / corte** | Sin transición: golpe seco + crash. Tras BUILDUP o al entrar FINALE → todo instantáneo. | — |
| **🧱 WALL → SNAP** | *(Minimal→Trance, 8 bars · FLAG_PREVIEW)* Un LPF maestro estéreo cierra progresivamente y hunde TODO el mix «en la sala de al lado», mientras la reverb-wash y la progresión armónica (Am·Dm·Em) crecen creando expectación. Al entrar el Trance el filtro se **reabre de golpe (snap)** + crash → el espectro completo del supersaw explota. Adiós al cambio brusco de 3 motores. | `masterLpf 1.0→0.10` durante transOut, reset a 1.0 en EnterSection |
| **🤯 WALL+STUTTER** | *(Minimal→Trance, 8 bars)* El LPF maestro cierra y hunde TODO el groove «en la sala de al lado» (preview tras la pared). En las 2 últimas barras un gate retriggea acelerando 1/8→1/32 (efecto tape-stop/buffer-repeat). En el bar 1 del Trance: SNAP de apertura + crash. Rompe-cabezas garantizado. | `masterLpf 1.0→0.06`, `stutter period 1/8→1/32`, reset al entrar |

**Regla de oro:** *nunca hay silencio ni bajada de volumen*. El beat (`drumsGainEff = 0.9`) y el `outGain = 1.0` son constantes. La transición vive **solo en el timbre** (filtro del bajo) y el **color** (reverb/delay). 10 minutos a muerte sin parar.

---

## Progresión armónica (secciones largas ≥ 20 bars)

Las secciones largas transponen bajo + melodía siguiendo **Am – Dm – Em – C** (i – iv – v – III en La menor), **1 acorde cada 8 compases**:

| Sección | Bars | Acordes (8 bars c/u) |
|---------|-----:|----------------------|
| 4 Acid house | 24 | Am · Dm · Em |
| 8 Deep house | 24 | Am · Dm · Em |
| 9 Funky electro | 24 | Am · Dm · Em |
| 11 Minimal | 20 | Am · Dm · Em |
| 12 Trance | 28 | Am · Dm · Em · C |
| 15 **Peak drop** | 32 | Am · Dm · Em · C |

> Buildup y finale quedan FUERA: deben mantener la tónica (La) para máximo impacto en el drop.
> El silencio (nota 0) nunca se transpone; el rango se limita a MIDI 1–127.

---

## Estructura interna de cada compás / sección

- **Crash de estructura**: cada 8 compases en secciones ≥20 bars (marca el cambio de acorde).
- **Fill de snare**: en el ÚLTIMO compás de toda sección con groove (steps 8/10/12/14, velocity ascendente) → anticipa la transición.
- **BUILDUP** (14, 16): redoble de snare con densidad creciente `f(progress)` + riser de reverb.
- **FINALE** (17, 18): crash por compás; en el último compás de cada fase, redoble ascendente de toms low→mid + snare → catarsis.

---

## La historia (guión emocional, línea por sección)

1. **Detroit intro** — Nace en la oscuridad: campanas ring-mod del 808 flotan sin ritmo, un whoosh de reverb te arrastra dentro.
2. **Detroit groove** — Primer latido: el 4×4 entra con crash, el ride marca el 8avo y el 303 respira grave. La ciudad se mueve.
3. **Breakdown** — Contienes el aliento: cae el kick y solo queda la reverb (fb 0.90) disolviéndolo todo.
4. **Acid house** — La chispa ácida: el 303 serpentea con slides y el filtro ladder (Q 0.90) muerde. Empieza el viaje.
5. **Acid peak** — El ácido hierve: el 303 abre a 1500 Hz al borde de la auto-oscilación (Q 0.94). No puedes parar.
6. **UK garage 2-step** — Las caderas mandan: 2-step del 505, shuffle de 22 muestras, teclas cálidas del SH101. Groove de calle.
7. **Organic house** — Respira: 4×4 suave, órgano wavetable y octavas del SH101. Calor humano, las manos se buscan.
8. **Deep house** — Te sumerges: stabs aditivos, swing 18, claps del 808. Profundo e hipnótico, sin fondo.
9. **Funky electro** — Sonríes: slap bass con slides y rimshots a contratiempo. El cuerpo juega, el funk manda.
10. **Micro-break** — Pausa cósmica: campanas del 808 flotan en reverb 0.95, el sub a 200 Hz se disuelve. Todos se miran… ¿qué viene?
11. **Minimal techno** — Pulso hipnótico: marimba escasa y dub delay al 0.62 que rebota en el vacío. Minimal.
12. **Trance supersaw** — Manos al cielo: el supersaw FM (detune 9) sube en arpegio sobre reverb gigante. Lágrimas de felicidad.
13. **Tribal perc** — La tribu: toms y perc del 808, acordes stab, swing 8. Fuego y tierra bajo los pies.
14. **Buildup** — Sube… sube…: el redoble de snare se acelera y la reverb crece. La tensión lo invade todo.
15. **Peak drop** — EL CLÍMAX: el riff anthem (mel8) estalla sobre pluck FM corto. ¡Todos saltan a la vez!
16. **Final buildup** — Última subida: kick galopante y snare en redoble. El corazón a mil por hora.
17. **Final drop** — EL DROP golpea SIN AVISO: bajo motor de 16avos + riff de pico, crash en cada compás. ¡Catarsis!
18. **Apoteosis** — APOTEOSIS: la melodía anthem vuela sobre el bajo motor con redobles de toms. ¡¡¡No se lo creen!!!
19. **Reset** — La calma: vuelven las campanas y la reverb larga. Nada será igual… y vuelve a empezar.

---

## Notas técnicas para el directo

- **CPU**: en FINALE se usa `PRE_PLUCK` (decay 0.22 s) — NO `PRE_BELL` (2.6 s saturaba 8 voces FM → crackle).
- **Reverb**: bypass en FLAG_FINALE salvo cuando hay `drumWashSend>0` (salida WASH de la Apoteosis).
- **SDRAM**: motores de batería (909/808/505) y TB303 viven en SRAM normal (sus constructores corren antes de `hw.Init()`).
- **Monitor serial USB** (115200): banner por sección + línea viva 2 Hz con VU, paso, progreso, acorde actual, fc y voces FM.
- **Build**: `build_daisy.ps1 -DemoBells` · **Flash**: `flash_bells.ps1` (sin samples.bin).
</content>
</invoke>

File created with the same string in both Edit and Write tool calls.<system-reminder>Warning: the contents of the file YOU just read DO NOT match the contents on disk. The file has been freshly modified - trust the contents on disk, NOT what you read or wrote. The file was modified by a linter or another process.