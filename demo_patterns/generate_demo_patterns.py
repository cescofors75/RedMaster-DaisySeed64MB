#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generador de banco de patrones a partir del DEMO de la Daisy.

Extrae el material musical del self-test de arranque de la Daisy
(RunStartup808SelfTest, DaisySeed/main.cpp:2367 PH_SYNTH_JAM) y lo convierte
en 19 patrones en el formato JSON que carga el master ESP32-S3
(WebInterface.cpp -> loadPatternBankFromFs).

Material de origen (idéntico al firmware Daisy):
  jamNotes        (TECHNO)  = 36 36 43 .  41 41 48 .  45 45 50 48  43 41 38 .
  jamNotesElectro (ELECTRO) = 36 43 36 .  48 46 43 .  41 43 45 .   50 48 46 43
  jamNotesAmbient (AMBIENT) = 36 .  43 .  48 .  50 .  53 .  48 .   45 .  41 .
  notes303 (escala)         = 36 38 41 43 45 48 50 53

Reglas de accent/slide del demo:
  no-ambient: accent = (st%4==0) o st in {6,14};  slide = (st%8==3) o st==11
  ambient:    accent = (st%8==0);                 slide = (st%8==7)

Reglas de percusión del demo (st = 0..15):
  TECHNO : kick st%4==0 | snare {4,12} | hatC impares | clap {7,15} | ride {10}
  ELECTRO: kick {0,6,8,14} | snare {4,12} | hatO st%4==2 | hatC impares | cowbell {11}
  AMBIENT: kick {0,8} | clap {4,12} | crash {6,14} | hatO st%4==2

NOTA IMPORTANTE (limitacion del firmware): el engine es GLOBAL por track
(gTrackSynthEngine[16] en S3 main.cpp:88), NO por patron. Por eso todo el banco
comparte un unico mapa track->engine, definido en TRACK_ENGINES abajo.
"""
import json

STEPS = 16

# ── Mapa GLOBAL track -> engine (compartido por TODOS los patrones) ──────────
# engine: -1 sampler | 0=808 | 1=909 | 2=505 | 3=303 | 4=WTOSC | 5=SH101 | 6=FM2Op
# El indice de track mapea a instrumento via padTo808/909/505 de la Daisy:
#   0 BD · 1 SD · 2 CH · 3 OH · 4 CY · 5 CP · 6 CB · 7 (303) · 8 LT · 9 MT · 10 HT
TRACK_ENGINES = [
    0,   # 0  BD  kick      (808)
    1,   # 1  SD  snare     (909)
    2,   # 2  CH  closed hat(505)
    1,   # 3  OH  open hat  (909)
    1,   # 4  CY  crash/ride(909)
    0,   # 5  CP  clap      (808)
    2,   # 6  CB  cowbell   (505)
    3,   # 7  303 ACID BASS (melodia)  <-- nota por paso
    0,   # 8  LT  low tom   (808)
    0,   # 9  MT  mid tom   (808)
    0,   # 10 HT  hi tom    (808)
    -1, -1, -1, -1, -1,   # 11-15 libres (sampler)
]
T_BD, T_SD, T_CH, T_OH, T_CY, T_CP, T_CB, T_303, T_LT, T_MT, T_HT = range(11)

TRACK_NAMES = ["BD","SD","CH","OH","CY","CP","CB","303","LT","MT","HT",
               "-","-","-","-","-"]

# ── Lineas de bajo 303 (idénticas al demo Daisy) ─────────────────────────────
JAM_TECHNO  = [36,36,43,0, 41,41,48,0, 45,45,50,48, 43,41,38,0]
JAM_ELECTRO = [36,43,36,0, 48,46,43,0, 41,43,45,0, 50,48,46,43]
JAM_AMBIENT = [36,0,43,0, 48,0,50,0, 53,0,48,0, 45,0,41,0]
SCALE_303   = [36,38,41,43,45,48,50,53]

# ── Presets por estilo (preset que recuerda cada patron, por engine) ─────────
# 808: 0 Classic 1 HipHop 2 Techno 3 Latin 4 Pure
# 909: 0 Classic 1 Techno 2 HousePound 3 Industrial 4 Pure
# 505: 0 Classic 1 NewWave 2 Electro 3 LoFiHipHop 4 Pure
# 303: 0 Acid 1 Squelch 2 SubBass 3 SoftLead
PRESETS_BY_STYLE = {
    "techno":  {0: 2, 1: 1, 2: 2, 3: 0},   # 808 Techno, 909 Techno, 505 Electro, 303 Acid
    "electro": {0: 4, 1: 2, 2: 2, 3: 1},   # 808 Pure, 909 HousePound, 505 Electro, 303 Squelch
    "ambient": {0: 0, 1: 3, 2: 1, 3: 2},   # 808 Classic, 909 Industrial, 505 NewWave, 303 SubBass
    "acid":    {0: 0, 1: 0, 2: 0, 3: 0},   # 303 Acid + drums Classic
    "fill":    {0: 0, 1: 0, 2: 0, 3: 0},
}

def preset_for(track, style):
    eng = TRACK_ENGINES[track]
    if eng < 0:
        return 0
    return PRESETS_BY_STYLE.get(style, PRESETS_BY_STYLE["acid"]).get(eng, 0)

def flags_for(st, ambient):
    """Devuelve byte de flags: bit0=accent, bit1=slide (regla del demo)."""
    if ambient:
        accent = (st % 8 == 0)
        slide  = (st % 8 == 7)
    else:
        accent = (st % 4 == 0) or st in (6, 14)
        slide  = (st % 8 == 3) or (st == 11)
    return (1 if accent else 0) | (2 if slide else 0)

def drum_track(track, hits, style, vel=110, accents=None, accent_vel=124):
    steps = [0]*STEPS
    vels  = [0]*STEPS
    for s in hits:
        steps[s] = 1
        vels[s]  = accent_vel if (accents and s in accents) else vel
    return {"track": track, "name": TRACK_NAMES[track],
            "engine": TRACK_ENGINES[track], "preset": preset_for(track, style),
            "steps": steps, "velocities": vels}

def acid_track(notes, ambient, style):
    """Track 303 con nota por paso, accent/slide y velocidades."""
    steps = [0]*STEPS; vels = [0]*STEPS; nts = [0]*STEPS; flgs = [0]*STEPS
    for st in range(STEPS):
        n = notes[st] if st < len(notes) else 0
        nts[st] = n
        if n:
            f = flags_for(st, ambient)
            flgs[st] = f
            steps[st] = 1
            vels[st]  = 122 if (f & 1) else 100
    return {"track": T_303, "name": "303", "engine": 3,
            "preset": preset_for(T_303, style),
            "steps": steps, "velocities": vels, "notes": nts, "flags": flgs}

# ── Sets de percusion del demo ───────────────────────────────────────────────
def techno_drums():
    s = "techno"
    return [
        drum_track(T_BD, [0,4,8,12], s, vel=120, accents=[0,8]),
        drum_track(T_SD, [4,12],     s, vel=112),
        drum_track(T_CH, [1,3,5,7,9,11,13,15], s, vel=78),
        drum_track(T_CP, [7,15],     s, vel=96),
        drum_track(T_CY, [10],       s, vel=88),   # ride
    ]
def electro_drums():
    s = "electro"
    return [
        drum_track(T_BD, [0,6,8,14], s, vel=120, accents=[0,8]),
        drum_track(T_SD, [4,12],     s, vel=110),
        drum_track(T_OH, [2,6,10,14],s, vel=82),
        drum_track(T_CH, [1,3,5,7,9,11,13,15], s, vel=70),
        drum_track(T_CB, [11],       s, vel=92),   # cowbell
    ]
def ambient_drums():
    s = "ambient"
    return [
        drum_track(T_BD, [0,8],      s, vel=96),
        drum_track(T_CP, [4,12],     s, vel=72),
        drum_track(T_CY, [6,14],     s, vel=64),   # crash
        drum_track(T_OH, [2,6,10,14],s, vel=58),
    ]

# ── Definicion de los 19 patrones ────────────────────────────────────────────
def build_patterns():
    P = []
    def add(slot, name, tracks):
        P.append({"slot": slot, "name": name, "tracks": tracks})

    # TECHNO (0-4)
    add(0, "TECHNO FULL",  techno_drums() + [acid_track(JAM_TECHNO, False, "techno")])
    add(1, "TECHNO BUILD", techno_drums() + [
        drum_track(T_OH, [2,6,10,14], "techno", vel=74),
        acid_track(JAM_TECHNO, False, "techno")])
    add(2, "TECHNO DRUMS", techno_drums())
    add(3, "TECHNO ACID",  [drum_track(T_BD, [0,4,8,12], "techno", vel=120)] +
                           [acid_track(JAM_TECHNO, False, "techno")])
    add(4, "TECHNO BREAK", [drum_track(T_CH, list(range(STEPS)), "techno", vel=70),
                            drum_track(T_CP, [7,15], "techno", vel=100),
                            acid_track(JAM_TECHNO, False, "techno")])

    # ELECTRO (5-9)
    add(5, "ELECTRO FULL",  electro_drums() + [acid_track(JAM_ELECTRO, False, "electro")])
    add(6, "ELECTRO BUILD", electro_drums() + [
        drum_track(T_CY, [0,8], "electro", vel=70),
        acid_track(JAM_ELECTRO, False, "electro")])
    add(7, "ELECTRO DRUMS", electro_drums())
    add(8, "ELECTRO ACID",  [drum_track(T_BD, [0,6,8,14], "electro", vel=118)] +
                            [acid_track(JAM_ELECTRO, False, "electro")])
    add(9, "ELECTRO BREAK", [drum_track(T_CH, list(range(STEPS)), "electro", vel=66),
                             drum_track(T_CB, [3,7,11,15], "electro", vel=92),
                             acid_track(JAM_ELECTRO, False, "electro")])

    # AMBIENT (10-13)
    add(10, "AMBIENT FULL",   ambient_drums() + [acid_track(JAM_AMBIENT, True, "ambient")])
    add(11, "AMBIENT SPARSE", [drum_track(T_BD, [0,8], "ambient", vel=90),
                               acid_track(JAM_AMBIENT, True, "ambient")])
    add(12, "AMBIENT DRUMS",  ambient_drums())
    add(13, "AMBIENT PAD",    [acid_track(JAM_AMBIENT, True, "ambient")])

    # ACID STUDIES (14-16) - escala notes303
    up   = [SCALE_303[i % 8] for i in range(STEPS)]
    down = [SCALE_303[(7 - (i % 8))] for i in range(STEPS)]
    octv = []
    for i in range(STEPS // 2):
        octv += [SCALE_303[i % 8], SCALE_303[(i + 4) % 8]]
    add(14, "ACID RUN UP",   [drum_track(T_BD, [0,4,8,12], "acid", vel=116),
                              acid_track(up, False, "acid")])
    add(15, "ACID RUN DOWN", [drum_track(T_BD, [0,4,8,12], "acid", vel=116),
                              acid_track(down, False, "acid")])
    add(16, "ACID OCTAVE",   [drum_track(T_BD, [0,4,8,12], "acid", vel=116),
                              acid_track(octv, False, "acid")])

    # FILLS / TRANSICIONES (17-18)
    add(17, "TOM FILL", [
        drum_track(T_LT, [0,1,2,3],   "fill", vel=110),
        drum_track(T_MT, [4,5,6,7],   "fill", vel=114),
        drum_track(T_HT, [8,9,10,11], "fill", vel=118),
        drum_track(T_SD, [12,13,14,15], "fill", vel=122)])
    roll = {"track": T_SD, "name": "SD", "engine": TRACK_ENGINES[T_SD],
            "preset": preset_for(T_SD, "fill"),
            "steps": [1]*STEPS,
            "velocities": [60 + int((127-60) * (i/(STEPS-1))) for i in range(STEPS)]}
    add(18, "SNARE ROLL", [roll])
    return P

def build_song_chain():
    # Un recorrido por el banco (pattern, repeats) 1..16
    return [
        {"pattern": 2,  "repeats": 1},  # TECHNO DRUMS (intro)
        {"pattern": 0,  "repeats": 4},  # TECHNO FULL
        {"pattern": 1,  "repeats": 2},  # TECHNO BUILD
        {"pattern": 17, "repeats": 1},  # TOM FILL
        {"pattern": 5,  "repeats": 4},  # ELECTRO FULL
        {"pattern": 6,  "repeats": 2},  # ELECTRO BUILD
        {"pattern": 18, "repeats": 1},  # SNARE ROLL
        {"pattern": 10, "repeats": 4},  # AMBIENT FULL
        {"pattern": 13, "repeats": 2},  # AMBIENT PAD (outro)
    ]

def main():
    bank = {
        "name": "19 Temas Demo Daisy (Techno/Electro/Ambient + Acid)",
        "tempo": 124,
        "stepCount": STEPS,
        "selectPattern": 0,
        "trackEngines": TRACK_ENGINES,
        "patterns": build_patterns(),
        "songChain": build_song_chain(),
    }
    out = "19_temas_demo_daisy.json"
    with open(out, "w", encoding="utf-8") as f:
        json.dump(bank, f, ensure_ascii=False, indent=1)
    print(f"OK -> {out}: {len(bank['patterns'])} patrones, "
          f"songChain {len(bank['songChain'])} entradas, stepCount {STEPS}")

if __name__ == "__main__":
    main()
