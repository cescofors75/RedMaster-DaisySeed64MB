#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generador del banco de 19 patrones — UNA SECCION DEL DEMO POR PATRON.

Sigue el orden de fases del self-test de la Daisy (RunStartup808SelfTest):
  Samplers -> 808 -> 909 -> 505 -> 303 -> XTRA -> Sampler FX -> Techno/Electro/Ambient,
y rellena con variantes hasta 19.

Melodia 303: SOLO donde el demo la tenia (seccion 303 y los jams). Las secciones
de bateria se escanean/tocan sin bajo (fiel al self-test).

Engine POR PATRON y POR TRACK: gracias al recall de engine/preset en el S3
(setTrackEngine + dsqUploadPattern), cada patron define el motor de cada track,
asi un mismo track puede ser 808 en un patron y 909 en otro.

  engine: -1 sampler | 0=808 | 1=909 | 2=505 | 3=303
  presets 808: 0 Classic 2 Techno · 909: 0 Classic 1 Techno · 505: 0 Classic 2 Electro
  presets 303: 0 Acid 1 Squelch 2 SubBass 3 SoftLead
"""
import json

STEPS = 16

# Track -> instrumento (igual en 808/909/505, segun padToXXX de la Daisy):
# 0 BD 1 SD 2 CH 3 OH 4 CY 5 CP 6 RS 7 CB 8 LT 9 MT 10 HT 11 MA 12 CL 13 HC 14 MC 15 LC
INST = ["BD","SD","CH","OH","CY","CP","RS","CB","LT","MT","HT","MA","CL","HC","MC","LC"]

# Lineas de bajo 303 del demo (idénticas al firmware)
JAM_TECHNO  = [36,36,43,0, 41,41,48,0, 45,45,50,48, 43,41,38,0]
JAM_ELECTRO = [36,43,36,0, 48,46,43,0, 41,43,45,0, 50,48,46,43]
JAM_AMBIENT = [36,0,43,0, 48,0,50,0, 53,0,48,0, 45,0,41,0]
SCALE_303   = [36,38,41,43,45,48,50,53]

ENG_SAMPLER, ENG_808, ENG_909, ENG_505, ENG_303 = -1, 0, 1, 2, 3


def flags_for(st, ambient):
    if ambient:
        accent = (st % 8 == 0); slide = (st % 8 == 7)
    else:
        accent = (st % 4 == 0) or st in (6, 14); slide = (st % 8 == 3) or (st == 11)
    return (1 if accent else 0) | (2 if slide else 0)


def drum(track, engine, hits, preset=0, vel=110, accents=None, accent_vel=124):
    steps = [0]*STEPS; vels = [0]*STEPS
    for s in hits:
        steps[s] = 1
        vels[s]  = accent_vel if (accents and s in accents) else vel
    return {"track": track, "name": INST[track], "engine": engine,
            "preset": preset, "steps": steps, "velocities": vels}


def acid(track, notes, ambient, preset=0):
    steps = [0]*STEPS; vels = [0]*STEPS; nts = [0]*STEPS; flgs = [0]*STEPS
    for st in range(STEPS):
        n = notes[st] if st < len(notes) else 0
        nts[st] = n
        if n:
            f = flags_for(st, ambient); flgs[st] = f
            steps[st] = 1; vels[st] = 122 if (f & 1) else 100
    return {"track": track, "name": "303", "engine": ENG_303, "preset": preset,
            "steps": steps, "velocities": vels, "notes": nts, "flags": flgs}


def scan(engine, n_instruments, preset=0, vel=112):
    """Escaneo: instrumento t en el paso t (showcase del kit)."""
    return [drum(t, engine, [t], preset=preset, vel=vel) for t in range(n_instruments)]


# ── Jams del demo (drums con engine por track + 303 en track 7) ──────────────
def techno_drums():
    return [
        drum(0, ENG_808, [0,4,8,12], preset=2, vel=120, accents=[0,8]),
        drum(1, ENG_909, [4,12],     preset=1, vel=112),
        drum(2, ENG_505, [1,3,5,7,9,11,13,15], preset=2, vel=78),
        drum(5, ENG_505, [7,15],     preset=2, vel=96),
        drum(4, ENG_909, [10],       preset=1, vel=88),
    ]
def electro_drums():
    return [
        drum(0, ENG_909, [0,6,8,14], preset=2, vel=120, accents=[0,8]),
        drum(1, ENG_505, [4,12],     preset=2, vel=110),
        drum(3, ENG_909, [2,6,10,14],preset=2, vel=82),
        drum(2, ENG_505, [1,3,5,7,9,11,13,15], preset=2, vel=70),
        drum(7, ENG_505, [11],       preset=2, vel=92),   # cowbell
    ]
def ambient_drums():
    return [
        drum(0, ENG_808, [0,8],      preset=0, vel=96),
        drum(5, ENG_808, [4,12],     preset=0, vel=72),   # clap
        drum(4, ENG_909, [6,14],     preset=3, vel=64),   # crash
        drum(3, ENG_505, [2,6,10,14],preset=0, vel=58),
    ]


def build_patterns():
    P = []
    def add(slot, name, tracks):
        P.append({"slot": slot, "name": name, "tracks": tracks})

    # ── 1-10: una por seccion del demo (orden del self-test) ──
    add(0, "SAMPLERS",   [drum(t, ENG_SAMPLER, [t], vel=115) for t in range(16)])
    add(1, "808 SCAN",   scan(ENG_808, 16, preset=0))
    add(2, "909 SCAN",   scan(ENG_909, 11, preset=0))
    add(3, "505 SCAN",   scan(ENG_505, 11, preset=0))
    # 303: escala notes303 a lo largo de 16 pasos (MELODIA)
    escala = [SCALE_303[i % 8] for i in range(STEPS)]
    add(4, "303 ESCALA", [acid(0, escala, False, preset=0)])
    # XTRA: los pads xtra (16-23) NO entran en la rejilla de 16 tracks ->
    # se aproxima con un groove de samplers (ver README).
    add(5, "XTRA",       [drum(t, ENG_SAMPLER, [t*4 % STEPS, t*4 % STEPS + 2], vel=110)
                          for t in range(4)])
    # SAMPLER FX: groove de samplers (la automatizacion de FX no va en el banco)
    add(6, "SAMPLER FX", [drum(0, ENG_SAMPLER, [0,4,8,12], vel=118),
                          drum(1, ENG_SAMPLER, [2,6,10,14], vel=96),
                          drum(2, ENG_SAMPLER, [1,3,5,7,9,11,13,15], vel=70)])
    add(7, "TECHNO",  techno_drums()  + [acid(7, JAM_TECHNO,  False, preset=0)])
    add(8, "ELECTRO", electro_drums() + [acid(6, JAM_ELECTRO, False, preset=1)])
    add(9, "AMBIENT", ambient_drums() + [acid(7, JAM_AMBIENT, True,  preset=2)])

    # ── 11-19: variantes (grooves de cada caja + lineas 303 + jams sin/con bajo) ──
    add(10, "808 BEAT", [drum(0, ENG_808, [0,4,8,12], preset=2, vel=120, accents=[0]),
                         drum(1, ENG_808, [4,12], preset=2, vel=110),
                         drum(2, ENG_808, [1,3,5,7,9,11,13,15], preset=2, vel=76)])
    add(11, "909 BEAT", [drum(0, ENG_909, [0,4,8,12], preset=1, vel=120, accents=[0]),
                         drum(1, ENG_909, [4,12], preset=1, vel=112),
                         drum(3, ENG_909, [2,6,10,14], preset=1, vel=80)])
    add(12, "505 BEAT", [drum(0, ENG_505, [0,3,6,10], preset=2, vel=116),
                         drum(1, ENG_505, [4,12], preset=2, vel=108),
                         drum(2, ENG_505, [1,3,5,7,9,11,13,15], preset=2, vel=72)])
    add(13, "ACID LINE A", [acid(0, JAM_TECHNO,  False, preset=0)])   # MELODIA
    add(14, "ACID LINE B", [acid(0, JAM_ELECTRO, False, preset=1)])   # MELODIA
    add(15, "TECHNO DRUMS",  techno_drums())
    add(16, "ELECTRO DRUMS", electro_drums())
    add(17, "AMBIENT DRUMS", ambient_drums())
    add(18, "FULL JAM", techno_drums() + [acid(7, JAM_TECHNO, False, preset=0)])  # MELODIA
    return P


def build_song_chain():
    return [
        {"pattern": 0, "repeats": 1}, {"pattern": 1, "repeats": 1},
        {"pattern": 2, "repeats": 1}, {"pattern": 3, "repeats": 1},
        {"pattern": 4, "repeats": 2}, {"pattern": 7, "repeats": 4},
        {"pattern": 8, "repeats": 4}, {"pattern": 9, "repeats": 4},
    ]


def main():
    bank = {
        "name": "19 Secciones Demo Daisy (samplers/808/909/505/303 + jams)",
        "tempo": 124, "stepCount": STEPS, "selectPattern": 0,
        "patterns": build_patterns(),
        "songChain": build_song_chain(),
    }
    out = "19_temas_demo_daisy.json"
    with open(out, "w", encoding="utf-8") as f:
        json.dump(bank, f, ensure_ascii=False, indent=1)
    mel = [p["slot"] for p in bank["patterns"]
           if any(t.get("engine") == 3 for t in p["tracks"])]
    print(f"OK -> {out}: {len(bank['patterns'])} patrones; "
          f"con melodia 303 en slots {mel}")

if __name__ == "__main__":
    main()
