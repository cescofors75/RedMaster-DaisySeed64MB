/* ═══════════════════════════════════════════════════════════════════
 *  DEMO BELLS — "RED808 Journey" · Daisy Seed standalone
 * ─────────────────────────────────────────────────────────────────
 *  Un viaje de ~10 minutos que recorre estilos reconocibles de la
 *  música electrónica, mostrando el potencial del RED808 sin ESP32.
 *  Suena solo al arrancar y evoluciona por SECCIONES; dentro de cada
 *  sección la energía se dirige con el "truco Mills": muteando y
 *  desmuteando bucles. NO hay fills aleatorios — todo es groove.
 *
 *  Recorrido (loop infinito, ~11-12 min por vuelta):
 *    1.  Detroit intro          (bells hipnóticas, kick que entra)
 *    2.  Detroit groove         (bells + ride + bass)
 *    3.  Breakdown              (sólo bells + reverb gigante)
 *    4.  Acid house             (303 ácido, lead FM pluck)
 *    5.  Acid peak              (303 abierto al máximo)
 *    6.  UK garage / 2-step     (Fred again.. : shuffle, EP cálido)
 *    7.  Organic house emotivo  (acordes Rhodes, sub redondo)
 *    8.  Deep house             (swing, claps, stabs)
 *    9.  Funky electro          (kick sincopado, slap bass, rimshots)
 *   10.  Minimal techno         (hipnótico, sparse, dub delay)
 *   11.  Electro / breakbeat    (kick sincopado, breaks)
 *   12.  Trance melódico        (arpegios FM, reverb enorme)
 *   13.  Tribal percussion      (toms + perc, groove)
 *   14.  Buildup                (snare roll + riser de reverb)
 *   15.  Peak-time drop         (todo sonando, máxima energía)
 *   16.  FINAL buildup          (riser supersaw + snare roll)
 *   17.  FINAL DROP explosivo   (crash por compás, sub 16th, anthem)
 *   18.  Reset                  (silencio breve → vuelve al intro)
 *
 *  Motores (todos ya en el repo):
 *    · TR-909 (tr909.h)  → kick, snare, claps, hats, ride, toms, perc
 *    · FM 2-op (fm2op.h) → bells / pluck / lead / stabs (presets)
 *    · TB-303 (tb303.h)  → bassline + lead ácido
 *  FX: reverb (DaisySP-LGPL) + delay con feedback, automatizados.
 *
 *  Build:   build_daisy.ps1 -DemoBells   (o make DEMO_BELLS=1)
 *  Flash:   flash_bells.ps1   (NO necesita samples — síntesis pura)
 * ═══════════════════════════════════════════════════════════════════ */

#include "daisy_seed.h"
#define USE_DAISYSP_LGPL
#include "daisysp.h"
#include <math.h>

/* ── Fast math para los motores de síntesis (igual que el firmware
 *    principal): sinf parabólico + expf bit-trick. ── */
static inline float __fast_sinf(float x) {
    float phase = x * 0.15915494f;
    phase -= (float)(int)(phase);
    if(phase < 0.0f) phase += 1.0f;
    float p = 2.0f * phase - 1.0f;
    float y = 4.0f * p * (1.0f - fabsf(p));
    return -(0.225f * (y * fabsf(y) - y) + y);
}
static inline float __fast_expf(float x) {
    union { float f; int32_t i; } v;
    v.i = (int32_t)(12102203.0f * x) + 1065353216;
    return (v.i > 0) ? v.f : 0.0f;
}
#define sinf(x) __fast_sinf(x)
#define expf(x) __fast_expf(x)

#include "synth/tr909.h"
#include "synth/tb303.h"
#include "synth/fm2op.h"

#undef sinf
#undef expf

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

static constexpr float  SAMPLE_RATE = 48000.0f;
static constexpr size_t AUDIO_BLOCK = 48;

/* ═══════════════════════════════════════════════════════════════════
 *  MOTORES
 *    · TR909::Kit y TB303::Synth → SRAM normal (constructores corren
 *      antes de hw.Init(); en SDRAM darían HardFault).
 *    · ReverbSc y buffers grandes → SDRAM (constructor trivial).
 * ═══════════════════════════════════════════════════════════════════ */
static TR909::Kit   drums;
static TB303::Synth bass;
DSY_SDRAM_BSS static ReverbSc reverb;

/* Delay estéreo con feedback (eco dub) — buffers en SDRAM */
static constexpr size_t DLY_SIZE = 28800;   /* 0.6 s @ 48k */
DSY_SDRAM_BSS static float dlyBufL[DLY_SIZE];
DSY_SDRAM_BSS static float dlyBufR[DLY_SIZE];
static size_t dlyWp = 0;
static float  dlyFb       = 0.45f;   /* feedback actual (lerp)  */
static float  dlyFbTgt    = 0.45f;   /* objetivo de la sección  */
static size_t dlyTimeL    = 18000;   /* ~3/8 de nota a 132 bpm  */
static size_t dlyTimeR    = 24000;   /* tap derecho más largo   */

/* ═══════════════════════════════════════════════════════════════════
 *  VOCES FM — polifonía con presets conmutables
 * ═══════════════════════════════════════════════════════════════════ */
static constexpr int NUM_FM = 8;
static FM2Op::Synth fmv[NUM_FM];
static uint8_t      fmNext = 0;

enum FmPreset : uint8_t {
    PRE_BELL = 0,   /* campana metálica ring-mod (Detroit)   */
    PRE_PLUCK,      /* pluck corto (acid lead, deep stabs)   */
    PRE_LEAD,       /* lead sostenido (trance)               */
    PRE_MARIMBA,    /* mallet suave (minimal)                */
    PRE_STAB,       /* stab aditivo (house chord)            */
    PRE_KEYS,       /* electric-piano cálido (organic/Fred)  */
    PRE_SUPER       /* supersaw-ish brillante (final épico)  */
};

static void ApplyPreset(FM2Op::Synth& v, uint8_t preset)
{
    switch(preset){
        case PRE_BELL:
            v.params.algo=2; v.params.ratio=1.41f; v.params.index=1.0f;
            v.params.feedback=0.0f; v.params.detune=4.0f; v.params.velSens=0.4f;
            v.params.cAtk=0.001f; v.params.cDec=2.6f; v.params.cSus=0.0f; v.params.cRel=1.4f;
            v.params.mAtk=0.001f; v.params.mDec=2.2f; v.params.mSus=0.2f; v.params.mRel=1.0f;
            v.params.volume=0.55f;
            break;
        case PRE_PLUCK:
            v.params.algo=0; v.params.ratio=2.0f; v.params.index=4.5f;
            v.params.feedback=0.1f; v.params.detune=2.0f; v.params.velSens=0.6f;
            v.params.cAtk=0.001f; v.params.cDec=0.22f; v.params.cSus=0.0f; v.params.cRel=0.12f;
            v.params.mAtk=0.001f; v.params.mDec=0.12f; v.params.mSus=0.0f; v.params.mRel=0.08f;
            v.params.volume=0.5f;
            break;
        case PRE_LEAD:
            v.params.algo=0; v.params.ratio=1.0f; v.params.index=2.6f;
            v.params.feedback=0.15f; v.params.detune=6.0f; v.params.velSens=0.5f;
            v.params.cAtk=0.012f; v.params.cDec=0.4f; v.params.cSus=0.7f; v.params.cRel=0.4f;
            v.params.mAtk=0.02f;  v.params.mDec=0.5f; v.params.mSus=0.5f; v.params.mRel=0.3f;
            v.params.volume=0.42f;
            break;
        case PRE_MARIMBA:
            v.params.algo=0; v.params.ratio=1.0f; v.params.index=2.0f;
            v.params.feedback=0.0f; v.params.detune=0.0f; v.params.velSens=0.5f;
            v.params.cAtk=0.001f; v.params.cDec=0.5f; v.params.cSus=0.0f; v.params.cRel=0.25f;
            v.params.mAtk=0.001f; v.params.mDec=0.18f; v.params.mSus=0.0f; v.params.mRel=0.1f;
            v.params.volume=0.5f;
            break;
        case PRE_STAB:
            v.params.algo=1; v.params.ratio=1.0f; v.params.index=3.0f;
            v.params.feedback=0.0f; v.params.detune=8.0f; v.params.velSens=0.4f;
            v.params.cAtk=0.004f; v.params.cDec=0.3f; v.params.cSus=0.0f; v.params.cRel=0.2f;
            v.params.mAtk=0.004f; v.params.mDec=0.25f; v.params.mSus=0.0f; v.params.mRel=0.15f;
            v.params.volume=0.45f;
            break;
        case PRE_KEYS:   /* Rhodes/EP cálido: ratio 1, index suave, decay medio */
            v.params.algo=0; v.params.ratio=1.0f; v.params.index=1.6f;
            v.params.feedback=0.05f; v.params.detune=3.0f; v.params.velSens=0.7f;
            v.params.cAtk=0.003f; v.params.cDec=0.9f; v.params.cSus=0.25f; v.params.cRel=0.5f;
            v.params.mAtk=0.003f; v.params.mDec=0.5f; v.params.mSus=0.1f; v.params.mRel=0.3f;
            v.params.volume=0.5f;
            break;
        case PRE_SUPER:  /* brillante y ancho para el clímax */
            v.params.algo=1; v.params.ratio=2.0f; v.params.index=4.0f;
            v.params.feedback=0.2f; v.params.detune=12.0f; v.params.velSens=0.5f;
            v.params.cAtk=0.006f; v.params.cDec=0.6f; v.params.cSus=0.6f; v.params.cRel=0.45f;
            v.params.mAtk=0.006f; v.params.mDec=0.4f; v.params.mSus=0.4f; v.params.mRel=0.3f;
            v.params.volume=0.5f;
            break;
    }
}

static void FmNoteOn(uint8_t midiNote, float vel, uint8_t preset)
{
    FM2Op::Synth& v = fmv[fmNext];
    ApplyPreset(v, preset);
    v.NoteOn(midiNote, vel);
    fmNext = (uint8_t)((fmNext + 1) % NUM_FM);
}

/* ═══════════════════════════════════════════════════════════════════
 *  BANCO DE PATRONES DE BATERÍA (bitmask de 16 pasos; bit i = paso i)
 * ═══════════════════════════════════════════════════════════════════ */
/* kick */
static const uint16_t KICK_FOUR    = 0x1111; /* 0,4,8,12         */
static const uint16_t KICK_HOUSE   = 0x1111;
static const uint16_t KICK_ELECTRO = (1<<0)|(1<<3)|(1<<6)|(1<<10)|(1<<13);
static const uint16_t KICK_BREAK   = (1<<0)|(1<<6)|(1<<10);
static const uint16_t KICK_ROLL    = (1<<0)|(1<<4)|(1<<8)|(1<<10)|(1<<12)|(1<<14);
static const uint16_t KICK_2STEP   = (1<<0)|(1<<6)|(1<<10);             /* UK garage / Fred again */
static const uint16_t KICK_FUNK    = (1<<0)|(1<<4)|(1<<7)|(1<<10)|(1<<12);/* síncopa funky */
static const uint16_t KICK_GALLOP  = (1<<0)|(1<<3)|(1<<4)|(1<<8)|(1<<11)|(1<<12); /* peak rodante */
static const uint16_t KICK_NONE    = 0x0000;
/* snare / clap (backbeat 4 y 12) */
static const uint16_t SNR_BACK     = (1<<4)|(1<<12);
static const uint16_t SNR_GHOST    = (1<<4)|(1<<12)|(1<<7)|(1<<15);
static const uint16_t SNR_2STEP    = (1<<4)|(1<<12)|(1<<14);            /* clap+rim shuffle 2-step */
static const uint16_t SNR_NONE     = 0x0000;
/* hihat cerrado */
static const uint16_t HHC_8TH      = 0x5555;            /* pares     */
static const uint16_t HHC_16TH     = 0xFFFF;            /* todos     */
static const uint16_t HHC_OFF      = (1<<2)|(1<<6)|(1<<10)|(1<<14);
static const uint16_t HHC_SHUF     = (1<<0)|(1<<3)|(1<<4)|(1<<7)|(1<<8)|(1<<11)|(1<<12)|(1<<15);
static const uint16_t HHC_GARAGE   = (1<<2)|(1<<3)|(1<<6)|(1<<7)|(1<<10)|(1<<11)|(1<<14)|(1<<15); /* dobles tipo skippy */
static const uint16_t HHC_NONE     = 0x0000;
/* hihat abierto (offbeats) */
static const uint16_t HHO_OFF      = (1<<2)|(1<<6)|(1<<10)|(1<<14);
static const uint16_t HHO_NONE     = 0x0000;
/* ride (corcheas) */
static const uint16_t RIDE_8TH     = 0x5555;
static const uint16_t RIDE_16TH    = 0xFFFF;
static const uint16_t RIDE_NONE    = 0x0000;

static inline bool Hit(uint16_t pat, int step) { return (pat >> step) & 1; }

/* ═══════════════════════════════════════════════════════════════════
 *  BANCO DE BAJOS (16 pasos: nota MIDI, 0=silencio; acento; slide)
 * ═══════════════════════════════════════════════════════════════════ */
struct BassPat { uint8_t note[16]; uint8_t acc[16]; uint8_t slide[16]; };

static const BassPat BASS_BANK[] = {
    /* 0: steady A1 — ostinato de semicorcheas (Detroit) */
    { {33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33},
      { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 1: acid wormy — saltos + slides (acid house) */
    { {33, 0,33,36, 0,33, 0,40,33, 0,33,45, 0,33,36, 0},
      { 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0},
      { 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0} },
    /* 2: deep house bouncy — con octavas */
    { {33, 0, 0,33, 0, 0,33, 0,45, 0, 0,45, 0, 0,40, 0},
      { 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 3: minimal — muy escaso */
    { {33, 0, 0, 0, 0, 0,33, 0, 0, 0, 0, 0,33, 0,36, 0},
      { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 4: offbeat — bajo en las corcheas "and" (house clásico) */
    { { 0,33, 0,33, 0,33, 0,33, 0,33, 0,33, 0,33, 0,33},
      { 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 5: rolling octaves — peak time energético */
    { {33,33,45,33,33,33,45,33,33,33,45,33,40,40,45,40},
      { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1} },
    /* 6: 2-step garage sub — síncopa Fred again, redondo y cálido */
    { { 0, 0,33, 0, 0,36, 0, 0,33, 0, 0, 0,40, 0,36, 0},
      { 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
      { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0} },
    /* 7: funky slap — pops sincopados con slides (raíces electro-funk) */
    { {33, 0,40,33, 0,45, 0,40,33, 0,33,40, 0,45,43, 0},
      { 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0},
      { 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0} },
    /* 8: driving 16th — final explosivo, sub martillado */
    { {33,33,33,40,33,33,33,40,33,33,33,40,45,45,40,45},
      { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
};

/* ═══════════════════════════════════════════════════════════════════
 *  BANCO DE MELODÍAS FM (16 pasos: dos voces hi/lo, 0=silencio)
 * ═══════════════════════════════════════════════════════════════════ */
struct Melody { uint8_t hi[16]; uint8_t lo[16]; };

static const Melody MEL_BANK[] = {
    /* 0: Detroit bells — el motif hipnótico (La menor) */
    { {81, 0, 0,76, 0,79, 0, 0,81, 0, 0,84, 0,83, 0, 0},
      {69, 0, 0, 0, 0, 0,64, 0, 0, 0, 0,72, 0, 0, 0, 0} },
    /* 1: acid lead arp (La menor ascendente/descendente) */
    { {69, 0,72, 0,76, 0,72, 0,69, 0,76, 0,81, 0,76, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 2: deep house stabs (acordes Am en off) */
    { {69, 0, 0, 0,72, 0, 0, 0,76, 0, 0, 0,72, 0, 0, 0},
      {57, 0, 0, 0,60, 0, 0, 0,64, 0, 0, 0,60, 0, 0, 0} },
    /* 3: minimal pluck ostinato */
    { {81, 0,81, 0,79, 0,76, 0,81, 0,84, 0,83, 0,79, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 4: trance arp uplifting (semicorcheas continuas) */
    { {69,72,76,81,84,81,76,72,69,72,76,81,84,88,84,81},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 5: peak melodic riff */
    { {88, 0,84,86, 0,81, 0,84,88, 0,91, 0,88,84, 0,81},
      {69, 0, 0, 0,64, 0, 0, 0,69, 0, 0, 0,72, 0, 0, 0} },
    /* 6: organic-house chord stabs (Fred again) — acordes emotivos en off */
    { { 0, 0,72, 0, 0, 0,76, 0, 0,74, 0, 0, 0,72, 0, 0},
      { 0, 0,64, 0, 0, 0,67, 0, 0,65, 0, 0, 0,64, 0, 0} },
    /* 7: chopped vocal-ish hook — notas cortas sincopadas (2-step) */
    { {84, 0, 0,86,84, 0,79, 0, 0,81, 0,84, 0, 0,79, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 8: euphoric anthem lead — final explosivo, melodía que sube */
    { {81,84,88,84,86,88,91,88,84,88,91,93,88,91,96,93},
      {69, 0,72, 0,76, 0,72, 0,69, 0,76, 0,81, 0,84, 0} },
};

/* ═══════════════════════════════════════════════════════════════════
 *  SECCIONES — la estructura del set (loop infinito)
 * ═══════════════════════════════════════════════════════════════════ */
enum SecFlag : uint8_t {
    FLAG_NONE     = 0,
    FLAG_BUILDUP  = 1 << 0,   /* snare roll + riser de reverb     */
    FLAG_CRASH    = 1 << 1,   /* crash al entrar la sección       */
    FLAG_TOMS     = 1 << 2,   /* añade toms/perc tribales         */
    FLAG_FINALE   = 1 << 3,   /* clímax: crash por compás + extra perc */
    FLAG_FUNK     = 1 << 4,   /* acentos funky: clap+rim ghost notes   */
};

struct Section {
    uint16_t bars;
    uint16_t kick, snare, clap, hhc, hho, ride;  /* patrones (bitmask) */
    int8_t   bassPat;     /* índice BASS_BANK, -1 = sin bajo     */
    int8_t   melPat;      /* índice MEL_BANK,  -1 = sin melodía  */
    uint8_t  fmPreset;    /* preset FM para la melodía           */
    float    bassCutoff;  /* fc del 303                          */
    float    bassReso;    /* resonancia del 303                  */
    float    revFb;       /* feedback reverb objetivo            */
    float    dlyFb;       /* feedback delay objetivo             */
    uint8_t  swing;       /* muestras de swing (0 = recto)       */
    uint8_t  flags;
};

static const Section SECTIONS[] = {
/*  bars kick         snare      clap       hhc        hho       ride       bass mel pre        cut    res   rev   dly  sw flags */
  { 16, KICK_NONE,   SNR_NONE,  SNR_NONE,  HHC_NONE,  HHO_NONE, RIDE_NONE,  -1,  0, PRE_BELL,   420,0.82f,0.80f,0.45f, 0, FLAG_CRASH },                 /* 1  Detroit intro */
  { 24, KICK_FOUR,   SNR_NONE,  SNR_NONE,  HHC_NONE,  HHO_OFF,  RIDE_8TH,   0,   0, PRE_BELL,   420,0.82f,0.72f,0.45f, 0, FLAG_NONE  },                 /* 2  Detroit groove */
  {  8, KICK_NONE,   SNR_NONE,  SNR_NONE,  HHC_NONE,  HHO_NONE, RIDE_NONE,  -1,  0, PRE_BELL,   420,0.82f,0.90f,0.55f, 0, FLAG_NONE  },                 /* 3  breakdown */
  { 32, KICK_FOUR,   SNR_NONE,  SNR_BACK,  HHC_OFF,   HHO_NONE, RIDE_NONE,  1,   1, PRE_PLUCK,  900,0.90f,0.45f,0.40f, 0, FLAG_CRASH },                 /* 4  acid house */
  { 16, KICK_FOUR,   SNR_NONE,  SNR_BACK,  HHC_16TH,  HHO_OFF,  RIDE_NONE,  1,   1, PRE_PLUCK, 1500,0.94f,0.40f,0.50f, 0, FLAG_NONE  },                 /* 5  acid peak */
  { 24, KICK_2STEP,  SNR_2STEP, SNR_NONE,  HHC_GARAGE,HHO_OFF,  RIDE_NONE,  6,   7, PRE_KEYS,   780,0.55f,0.62f,0.55f,22, FLAG_FUNK  },                 /* 6  UK garage 2-step (Fred again) */
  { 24, KICK_2STEP,  SNR_BACK,  SNR_NONE,  HHC_OFF,   HHO_OFF,  RIDE_NONE,  6,   6, PRE_KEYS,   650,0.55f,0.72f,0.55f,22, FLAG_NONE  },                 /* 7  organic house emotivo */
  { 24, KICK_HOUSE,  SNR_NONE,  SNR_BACK,  HHC_OFF,   HHO_OFF,  RIDE_NONE,  2,   2, PRE_STAB,   800,0.70f,0.55f,0.45f,18, FLAG_CRASH },                 /* 8  deep house (swing) */
  { 24, KICK_FUNK,   SNR_GHOST, SNR_NONE,  HHC_GARAGE,HHO_NONE, RIDE_NONE,  7,  -1, PRE_PLUCK,  900,0.80f,0.48f,0.45f,14, FLAG_FUNK  },                 /* 9  funky electro */
  { 32, KICK_FOUR,   SNR_NONE,  SNR_NONE,  HHC_NONE,  HHO_OFF,  RIDE_NONE,  3,   3, PRE_MARIMBA,650,0.60f,0.78f,0.62f, 0, FLAG_NONE  },                 /* 10 minimal (dub delay) */
  { 24, KICK_ELECTRO,SNR_GHOST, SNR_NONE,  HHC_16TH,  HHO_NONE, RIDE_NONE,  4,  -1, PRE_PLUCK,  700,0.85f,0.50f,0.40f, 0, FLAG_CRASH },                 /* 11 electro / break */
  { 40, KICK_FOUR,   SNR_NONE,  SNR_NONE,  HHC_OFF,   HHO_OFF,  RIDE_8TH,  -1,  4, PRE_LEAD,    420,0.82f,0.88f,0.55f, 0, FLAG_NONE  },                 /* 12 trance melódico */
  { 24, KICK_FOUR,   SNR_NONE,  SNR_NONE,  HHC_OFF,   HHO_NONE, RIDE_NONE,  0,  3, PRE_MARIMBA,500,0.70f,0.60f,0.50f,12, FLAG_TOMS  },                  /* 13 tribal perc (swing) */
  {  8, KICK_FOUR,   SNR_BACK,  SNR_NONE,  HHC_16TH,  HHO_NONE, RIDE_NONE, -1,  4, PRE_LEAD,    420,0.82f,0.92f,0.55f, 0, FLAG_BUILDUP },               /* 14 buildup */
  { 32, KICK_FOUR,   SNR_NONE,  SNR_BACK,  HHC_16TH,  HHO_OFF,  RIDE_8TH,   5,  5, PRE_BELL,   1100,0.88f,0.65f,0.45f, 0, FLAG_CRASH },                 /* 15 peak drop */
  {  8, KICK_GALLOP, SNR_BACK,  SNR_BACK,  HHC_16TH,  HHO_NONE, RIDE_16TH, -1,  8, PRE_SUPER,  1400,0.85f,0.95f,0.55f, 0, FLAG_BUILDUP },               /* 16 FINAL buildup (riser) */
  { 48, KICK_GALLOP, SNR_NONE,  SNR_BACK,  HHC_16TH,  HHO_OFF,  RIDE_16TH,  8,  8, PRE_SUPER,  1600,0.88f,0.70f,0.50f, 0, FLAG_FINALE|FLAG_FUNK },     /* 17 FINAL DROP explosivo */
  {  8, KICK_NONE,   SNR_NONE,  SNR_NONE,  HHC_NONE,  HHO_NONE, RIDE_NONE, -1,  0, PRE_BELL,    420,0.82f,0.88f,0.55f, 0, FLAG_NONE  },                 /* 18 reset → vuelve al intro */
};
static constexpr int NUM_SECTIONS = sizeof(SECTIONS)/sizeof(SECTIONS[0]);

/* ═══════════════════════════════════════════════════════════════════
 *  SECUENCIADOR
 * ═══════════════════════════════════════════════════════════════════ */
static constexpr float BPM = 132.0f;
static const uint32_t kStepSamples =
    (uint32_t)(SAMPLE_RATE * 60.0f / BPM / 4.0f);   /* semicorchea */

static uint32_t stepCounter = 0;
static uint32_t curStepLen  = kStepSamples;
static int      step16      = 0;
static uint32_t barCounter  = 0;

static int      secIdx  = 0;
static uint16_t secBar  = 0;     /* compás dentro de la sección actual */
static Section  cur     = SECTIONS[0];

static float ledLevel = 0.0f;
static float rideGain = 0.0f;    /* fade-in del ride                   */
static float masterGain = 1.0f;  /* para fade out de la última sección */

/* Objetivos de FX (lerp suave en el callback) */
static float revFbTgt = 0.80f;
static float revFb    = 0.80f;

/* Longitud de step con swing (retrasa la 2ª semicorchea de cada par) */
static inline uint32_t StepLength(int s)
{
    int32_t sw = (int32_t)cur.swing;
    if(sw == 0) return kStepSamples;
    if((s & 1) == 0) return kStepSamples + (uint32_t)sw;       /* on-beat: largo  */
    return (kStepSamples > (uint32_t)sw) ? kStepSamples - sw : 1;/* off-beat: corto */
}

static void EnterSection()
{
    cur      = SECTIONS[secIdx];
    revFbTgt = cur.revFb;
    dlyFbTgt = cur.dlyFb;
    rideGain = 0.0f;   /* el ride vuelve a hacer fade-in */

    /* Aplicar timbre del 303 de la sección */
    bass.SetCutoff(cur.bassCutoff);
    bass.SetResonance(cur.bassReso);

    /* Crash de impacto al entrar */
    if(cur.flags & FLAG_CRASH)
        drums.Trigger(TR909::INST_CRASH, 0.8f);
}

static void SequencerTick()
{
    /* ── Inicio de compás: ¿cambio de sección? ── */
    if(step16 == 0){
        if(secBar >= cur.bars){
            secIdx = (secIdx + 1) % NUM_SECTIONS;
            secBar = 0;
            EnterSection();
        }
    }

    const bool buildup = (cur.flags & FLAG_BUILDUP);
    /* progreso 0..1 dentro de la sección (para rampas) */
    const float secProg =
        (cur.bars > 0) ? ((float)secBar + (float)step16/16.0f) / (float)cur.bars
                       : 0.0f;

    /* ── KICK ── (en buildup el kick se mantiene 4x4) */
    if(Hit(cur.kick, step16))
        drums.Trigger(TR909::INST_KICK, 1.0f);

    /* ── SNARE ── normal, o roll ascendente en buildup ── */
    if(buildup){
        /* roll: cada vez más denso y fuerte conforme avanza la sección */
        bool roll = (secProg < 0.5f) ? (step16 % 4 == 0)
                  : (secProg < 0.8f) ? (step16 % 2 == 0)
                                     : true;
        if(roll)
            drums.Trigger(TR909::INST_SNARE, 0.35f + 0.65f * secProg);
    } else if(Hit(cur.snare, step16)){
        float v = (step16==4 || step16==12) ? 0.9f : 0.45f; /* ghost suaves */
        drums.Trigger(TR909::INST_SNARE, v);
    }

    /* ── CLAP (backbeat, va al delay → enterrado) ── */
    if(Hit(cur.clap, step16))
        drums.Trigger(TR909::INST_CLAP, 0.85f);

    /* ── HI-HAT cerrado ── */
    if(Hit(cur.hhc, step16))
        drums.Trigger(TR909::INST_HIHAT_C, (step16%2==0)?0.6f:0.4f);

    /* ── HI-HAT abierto (offbeats, decay abierto → se derrama) ── */
    if(Hit(cur.hho, step16))
        drums.Trigger(TR909::INST_HIHAT_O, 0.7f);

    /* ── RIDE en corcheas con fade-in ── */
    if(Hit(cur.ride, step16)){
        rideGain += (1.0f - rideGain) * 0.02f;
        drums.SetVolume(TR909::INST_RIDE, 0.55f * rideGain);
        drums.Trigger(TR909::INST_RIDE, 0.6f);
    }

    /* ── TOMS / PERC tribales ── */
    if(cur.flags & FLAG_TOMS){
        if(step16==2 || step16==11) drums.Trigger(TR909::INST_LOW_TOM, 0.7f);
        if(step16==6 || step16==14) drums.Trigger(TR909::INST_MID_TOM, 0.6f);
        if(step16==3 || step16==9 || step16==13) drums.Trigger(TR909::INST_HI_PERC, 0.5f);
        if(step16==1 || step16==7)  drums.Trigger(TR909::INST_SHAKER, 0.5f);
    }

    /* ── FUNK: rimshot + clave en las síncopas (toque electro-funk sutil) ── */
    if(cur.flags & FLAG_FUNK){
        if(step16==3 || step16==7 || step16==11) drums.Trigger(TR909::INST_RIMSHOT, 0.45f);
        if(step16==6 || step16==14)              drums.Trigger(TR909::INST_HI_PERC, 0.4f);
    }

    /* ── FINALE: clímax — crash al inicio de cada compás + perc rodante ── */
    if(cur.flags & FLAG_FINALE){
        if(step16==0)               drums.Trigger(TR909::INST_CRASH, 0.55f);
        if((step16 & 1) == 0)       drums.Trigger(TR909::INST_SHAKER, 0.4f);
        if(step16==2 || step16==10) drums.Trigger(TR909::INST_LOW_TOM, 0.6f);
    }

    /* ── MELODÍA FM (hi/lo) ── */
    if(cur.melPat >= 0){
        const Melody& m = MEL_BANK[cur.melPat];
        uint8_t hi = m.hi[step16];
        if(hi){
            float vel = ((step16 % 4) == 0) ? 0.9f : 0.6f;
            FmNoteOn(hi, vel, cur.fmPreset);
        }
        uint8_t lo = m.lo[step16];
        if(lo)
            FmNoteOn(lo, 0.65f, cur.fmPreset);
    }

    /* ── BASS 303 ── */
    if(cur.bassPat >= 0){
        const BassPat& bp = BASS_BANK[cur.bassPat];
        uint8_t note = bp.note[step16];
        if(note){
            bass.NoteOn(note, bp.acc[step16] != 0, bp.slide[step16] != 0);
        }
    }

    ledLevel = (step16 % 4 == 0) ? 1.0f : 0.4f;
}

/* ═══════════════════════════════════════════════════════════════════
 *  AUDIO CALLBACK
 * ═══════════════════════════════════════════════════════════════════ */
void AudioCallback(AudioHandle::InputBuffer  /*in*/,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    /* Lerp suave de FX hacia los objetivos de la sección (por bloque) */
    revFb += (revFbTgt - revFb) * 0.02f;
    dlyFb += (dlyFbTgt - dlyFb) * 0.02f;
    reverb.SetFeedback(revFb);

    /* Empuje de ganancia en el clímax (el resto a 1.0; el tanh limita) */
    float gainTgt = (cur.flags & FLAG_FINALE) ? 1.15f : 1.0f;
    masterGain += (gainTgt - masterGain) * 0.001f;

    for(size_t i = 0; i < size; i++)
    {
        if(stepCounter == 0){
            SequencerTick();
            curStepLen = StepLength(step16);
        }
        stepCounter++;
        if(stepCounter >= curStepLen){
            stepCounter = 0;
            step16++;
            if(step16 >= 16){
                step16 = 0;
                barCounter++;
                secBar++;
            }
        }

        /* ── 909 drums ── */
        float drumMix = drums.Process();

        /* ── Delay estéreo con feedback (dub) ── */
        size_t rpL = (dlyWp + DLY_SIZE - dlyTimeL) % DLY_SIZE;
        size_t rpR = (dlyWp + DLY_SIZE - dlyTimeR) % DLY_SIZE;
        float dL = dlyBufL[rpL];
        float dR = dlyBufR[rpR];

        /* ── FM voces ── */
        float fmMix = 0.0f;
        for(int v = 0; v < NUM_FM; v++)
            fmMix += fmv[v].Process();
        fmMix *= 0.45f;

        /* ── Bass 303 ── */
        float bassMix = bass.Process() * 0.9f;

        /* Alimentar el delay con percusión + algo de FM (cross-feed L/R) */
        float dlyInL = drumMix * 0.16f + fmMix * 0.20f;
        float dlyInR = drumMix * 0.16f + fmMix * 0.20f;
        dlyBufL[dlyWp] = dlyInL + dR * dlyFb;   /* ping-pong */
        dlyBufR[dlyWp] = dlyInR + dL * dlyFb;
        dlyWp = (dlyWp + 1) % DLY_SIZE;

        /* ── Mezcla seca ── */
        float dryL = drumMix * 0.9f + fmMix + bassMix + dL * 0.5f;
        float dryR = drumMix * 0.9f + fmMix + bassMix + dR * 0.5f;

        /* ── Reverb (sobre todo las FM + colas del delay) ── */
        float wetL, wetR;
        reverb.Process(fmMix + dL * 0.4f, fmMix + dR * 0.4f, &wetL, &wetR);

        float outL = (dryL + wetL * 0.45f) * masterGain;
        float outR = (dryR + wetR * 0.45f) * masterGain;

        /* Soft clip de seguridad */
        out[0][i] = tanhf(outL * 0.7f);
        out[1][i] = tanhf(outR * 0.7f);
    }

    ledLevel *= 0.85f;
    hw.SetLed(ledLevel > 0.2f);
}

/* ═══════════════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════════════ */
int main()
{
    /* FPU Flush-to-Zero + Default-NaN (evita denormales en la reverb) */
    __asm volatile("VMRS r0, FPSCR\n"
                   "ORR  r0, r0, #(1<<24)|(1<<25)\n"
                   "VMSR FPSCR, r0" ::: "r0");
    *(volatile uint32_t*)0xE000EF3Cu |= (1u << 24) | (1u << 25);

    hw.Init();
    hw.SetAudioBlockSize(AUDIO_BLOCK);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    /* ── 909: kit techno, kick distorsionado al máximo ── */
    drums.Init(SAMPLE_RATE);
    drums.kick.SetDrive(1.0f);
    drums.kick.SetDecay(0.55f);
    drums.kick.SetCompression(1.0f);
    drums.hihatO.SetDecay(1.6f);          /* decay abierto → se derrama */
    drums.SetVolume(TR909::INST_KICK,    1.3f);
    drums.SetVolume(TR909::INST_SNARE,   0.7f);
    drums.SetVolume(TR909::INST_HIHAT_C, 0.5f);
    drums.SetVolume(TR909::INST_HIHAT_O, 0.6f);
    drums.SetVolume(TR909::INST_CLAP,    0.5f);
    drums.SetVolume(TR909::INST_RIDE,    0.5f);
    drums.SetVolume(TR909::INST_LOW_TOM, 0.7f);
    drums.SetVolume(TR909::INST_MID_TOM, 0.7f);
    drums.SetVolume(TR909::INST_HI_PERC, 0.5f);
    drums.SetVolume(TR909::INST_SHAKER,  0.5f);
    drums.SetMasterVolume(0.9f);

    /* ── Voces FM ── */
    for(int v = 0; v < NUM_FM; v++){
        fmv[v].Init(SAMPLE_RATE);
        ApplyPreset(fmv[v], PRE_BELL);
    }

    /* ── 303 bass ── */
    bass.Init(SAMPLE_RATE);
    bass.SetWaveform(TB303::WAVE_SAW);
    bass.SetCutoff(420.0f);
    bass.SetResonance(0.82f);
    bass.SetEnvMod(0.8f);
    bass.SetDecay(0.12f);
    bass.SetAccent(0.85f);
    bass.SetOverdrive(0.5f);
    bass.SetVolume(0.8f);

    /* ── Reverb ── */
    reverb.Init(SAMPLE_RATE);
    reverb.SetFeedback(0.80f);
    reverb.SetLpFreq(8500.0f);

    /* Limpiar buffers de delay */
    for(size_t i = 0; i < DLY_SIZE; i++){ dlyBufL[i] = 0.0f; dlyBufR[i] = 0.0f; }

    /* Arrancar en la primera sección */
    secIdx = 0; secBar = 0;
    EnterSection();

    hw.StartAudio(AudioCallback);

    while(1) System::Delay(100);
}

/* ── Fault handler: SOS para diferenciar de cuelgues silenciosos ── */
static void FaultDelay(uint32_t ms)
{
    volatile uint32_t cycles = ms * 240000u;
    while(cycles--) __asm volatile("nop");
}
static void FaultSosLoop(void)
{
    __disable_irq();
    while(1)
    {
        for(int i = 0; i < 3; i++){ hw.SetLed(true); FaultDelay(120); hw.SetLed(false); FaultDelay(120); }
        FaultDelay(250);
        for(int i = 0; i < 3; i++){ hw.SetLed(true); FaultDelay(450); hw.SetLed(false); FaultDelay(150); }
        FaultDelay(250);
        for(int i = 0; i < 3; i++){ hw.SetLed(true); FaultDelay(120); hw.SetLed(false); FaultDelay(120); }
        FaultDelay(1500);
    }
}
extern "C" void HardFault_Handler(void) { FaultSosLoop(); }
extern "C" void MemManage_Handler(void) { FaultSosLoop(); }
extern "C" void BusFault_Handler(void)  { FaultSosLoop(); }
extern "C" void UsageFault_Handler(void){ FaultSosLoop(); }
