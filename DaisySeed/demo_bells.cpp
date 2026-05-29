/* ═══════════════════════════════════════════════════════════════════
 *  DEMO BELLS — "RED808 Journey" · Daisy Seed standalone
 * ─────────────────────────────────────────────────────────────────
 *  Un viaje de ~12.6 min que recorre estilos electrónicos modernos y
 *  clásicos con TRANSICIONES DE DJ PROFESIONALES entre cada sección.
 *
 *  Técnicas de mezcla implementadas (por sección):
 *    TMIX_FILTER  LPF sweep: el bajo cierra progresivamente, reabre
 *                 en la nueva sección. Como girar el filtro en el mixer.
 *    TMIX_ECHO    Echo out: el delay se dispara, todo se disuelve
 *                 en ecos. Toque de reverb wash simultáneo.
 *    TMIX_WASH    Reverb wash: la reverb sube a cola gigante y
 *                 "borra" la sección saliente.
 *    TMIX_STRIP   Strip: hats/clap desaparecen, queda sólo el kick
 *                 (tensión) antes del drop.
 *
 *  Cada entrada también tiene su propio "filter-in sweep": el bajo
 *  arranca desde el grave y abre hacia el timbre objetivo de la sección,
 *  igual que el DJ al subir el EQ después de mezclar.
 *
 *  Recorrido (loop infinito, ~12.6 min/vuelta):
 *     1  Detroit intro          bells hipnóticas, sin batería
 *     2  Detroit groove         kick + ride + bells + bass
 *     3  Breakdown              solo bells + reverb gigante
 *     4  Acid house             303 ácido wormy + pluck FM
 *     5  Acid peak              303 abierto al máximo
 *     6  UK garage / 2-step     Fred again..: shuffle, EP cálido
 *     7  Organic house emotivo  acordes Rhodes, sub sincopado
 *     8  Deep house             swing, claps, stabs
 *     9  Funky electro          slap bass, rimshots, síncopa
 *    10  Minimal techno         sparse, dub delay profundo
 *    11  Electro / break        kick sincopado, hats 16th
 *    12  Trance melódico        arpegios FM, reverb enorme
 *    13  Tribal percussion      toms + perc + shaker
 *    14  Buildup                snare roll + riser de reverb
 *    15  Peak-time drop         todo sonando, máxima energía
 *    16  Final buildup          riser supersaw + snare roll
 *    17  FINAL DROP explosivo   crash/compás, sub 16th, anthem
 *    18  Reset                  silencio breve → vuelve al inicio
 *
 *  Build:  build_daisy.ps1 -DemoBells   (o make DEMO_BELLS=1)
 *  Flash:  flash_bells.ps1              (sin samples.bin)
 * ═══════════════════════════════════════════════════════════════════ */

#include "daisy_seed.h"
#define USE_DAISYSP_LGPL
#include "daisysp.h"
#include <math.h>

/* ── Fast math: sinf parabólico + expf bit-trick ── */
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
static constexpr size_t AUDIO_BLOCK = 96;  /* 2ms/callback: más margen de CPU sin efecto audible */

static inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float Clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ═══════════════════════════════════════════════════════════════════
 *  COLORES ANSI (terminal keeper / PuTTY / Tera Term / VS Code serial)
 *  Si tu terminal no soporta ANSI, pon kAnsi=false y verás texto plano.
 * ═══════════════════════════════════════════════════════════════════ */
static constexpr bool kAnsi = true;
#define A_RST  "\x1b[0m"
#define A_BOLD "\x1b[1m"
#define A_DIM  "\x1b[2m"
#define A_BLK  "\x1b[30m"
#define A_RED  "\x1b[31m"
#define A_GRN  "\x1b[32m"
#define A_YEL  "\x1b[33m"
#define A_BLU  "\x1b[34m"
#define A_MAG  "\x1b[35m"
#define A_CYN  "\x1b[36m"
#define A_WHT  "\x1b[37m"
#define A_BRED "\x1b[91m"
#define A_BGRN "\x1b[92m"
#define A_BYEL "\x1b[93m"
#define A_BBLU "\x1b[94m"
#define A_BMAG "\x1b[95m"
#define A_BCYN "\x1b[96m"
#define A_BWHT "\x1b[97m"
#define A_CLR  "\x1b[2J\x1b[H"   /* clear screen + home */
/* Devuelve "" si kAnsi=false, el código si true (resuelto en runtime) */
static inline const char* C(const char* code){ return kAnsi ? code : ""; }

/* ═══════════════════════════════════════════════════════════════════
 *  MOTORES
 *    · TR909::Kit y TB303::Synth → SRAM normal (constructores corren
 *      antes de hw.Init(); en SDRAM causarían HardFault).
 *    · ReverbSc y buffers grandes → SDRAM (constructor trivial).
 * ═══════════════════════════════════════════════════════════════════ */
static TR909::Kit   drums;
static TB303::Synth bass;
DSY_SDRAM_BSS static ReverbSc reverb;

/* Delay estéreo ping-pong — buffers en SDRAM */
static constexpr size_t DLY_SIZE = 28800;   /* 0.6 s @ 48k */
DSY_SDRAM_BSS static float dlyBufL[DLY_SIZE];
DSY_SDRAM_BSS static float dlyBufR[DLY_SIZE];
static size_t dlyWp    = 0;
static float  dlyFb    = 0.45f;
static float  dlyFbTgt = 0.45f;
static size_t dlyTimeL = 18000;  /* tap L: ~3/8 a 132 bpm */
static size_t dlyTimeR = 24000;  /* tap R: más largo       */

/* ═══════════════════════════════════════════════════════════════════
 *  VOCES FM — 8-voice polyphony con presets
 * ═══════════════════════════════════════════════════════════════════ */
static constexpr int NUM_FM = 8;
static FM2Op::Synth fmv[NUM_FM];
static uint8_t      fmNext = 0;

enum FmPreset : uint8_t {
    PRE_BELL    = 0,  /* campana ring-mod (Detroit)          */
    PRE_PLUCK,        /* pluck corto (acid/stabs)            */
    PRE_LEAD,         /* lead sostenido (trance)             */
    PRE_MARIMBA,      /* mallet (minimal)                    */
    PRE_STAB,         /* stab aditivo (house)                */
    PRE_KEYS,         /* Rhodes/EP cálido (organic/garage)   */
    PRE_SUPER         /* supersaw-ish brillante (finale)     */
};

static void ApplyPreset(FM2Op::Synth& v, uint8_t preset)
{
    switch(preset){
        case PRE_BELL:
            v.params.algo=2; v.params.ratio=1.41f; v.params.index=1.0f;
            v.params.feedback=0.0f; v.params.detune=4.0f; v.params.velSens=0.4f;
            v.params.cAtk=0.001f; v.params.cDec=2.6f; v.params.cSus=0.0f; v.params.cRel=1.4f;
            v.params.mAtk=0.001f; v.params.mDec=2.2f; v.params.mSus=0.2f; v.params.mRel=1.0f;
            v.params.volume=0.55f; break;
        case PRE_PLUCK:
            v.params.algo=0; v.params.ratio=2.0f; v.params.index=4.5f;
            v.params.feedback=0.1f; v.params.detune=2.0f; v.params.velSens=0.6f;
            v.params.cAtk=0.001f; v.params.cDec=0.22f; v.params.cSus=0.0f; v.params.cRel=0.12f;
            v.params.mAtk=0.001f; v.params.mDec=0.12f; v.params.mSus=0.0f; v.params.mRel=0.08f;
            v.params.volume=0.5f; break;
        case PRE_LEAD:
            v.params.algo=0; v.params.ratio=1.0f; v.params.index=2.6f;
            v.params.feedback=0.15f; v.params.detune=6.0f; v.params.velSens=0.5f;
            v.params.cAtk=0.012f; v.params.cDec=0.4f; v.params.cSus=0.7f; v.params.cRel=0.4f;
            v.params.mAtk=0.02f;  v.params.mDec=0.5f; v.params.mSus=0.5f; v.params.mRel=0.3f;
            v.params.volume=0.42f; break;
        case PRE_MARIMBA:
            v.params.algo=0; v.params.ratio=1.0f; v.params.index=2.0f;
            v.params.feedback=0.0f; v.params.detune=0.0f; v.params.velSens=0.5f;
            v.params.cAtk=0.001f; v.params.cDec=0.5f; v.params.cSus=0.0f; v.params.cRel=0.25f;
            v.params.mAtk=0.001f; v.params.mDec=0.18f; v.params.mSus=0.0f; v.params.mRel=0.1f;
            v.params.volume=0.5f; break;
        case PRE_STAB:
            v.params.algo=1; v.params.ratio=1.0f; v.params.index=3.0f;
            v.params.feedback=0.0f; v.params.detune=8.0f; v.params.velSens=0.4f;
            v.params.cAtk=0.004f; v.params.cDec=0.3f; v.params.cSus=0.0f; v.params.cRel=0.2f;
            v.params.mAtk=0.004f; v.params.mDec=0.25f; v.params.mSus=0.0f; v.params.mRel=0.15f;
            v.params.volume=0.45f; break;
        case PRE_KEYS:
            v.params.algo=0; v.params.ratio=1.0f; v.params.index=1.6f;
            v.params.feedback=0.05f; v.params.detune=3.0f; v.params.velSens=0.7f;
            v.params.cAtk=0.003f; v.params.cDec=0.9f; v.params.cSus=0.25f; v.params.cRel=0.5f;
            v.params.mAtk=0.003f; v.params.mDec=0.5f; v.params.mSus=0.1f;  v.params.mRel=0.3f;
            v.params.volume=0.5f; break;
        case PRE_SUPER:
            v.params.algo=1; v.params.ratio=1.0f; v.params.index=2.2f;
            v.params.feedback=0.1f; v.params.detune=9.0f; v.params.velSens=0.5f;
            v.params.cAtk=0.008f; v.params.cDec=0.6f; v.params.cSus=0.55f; v.params.cRel=0.45f;
            v.params.mAtk=0.008f; v.params.mDec=0.4f; v.params.mSus=0.35f; v.params.mRel=0.3f;
            v.params.volume=0.30f; break;
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
 *  PATRONES DE BATERÍA (bitmask 16 pasos, bit i = paso i)
 * ═══════════════════════════════════════════════════════════════════ */
static const uint16_t KICK_FOUR    = 0x1111;
static const uint16_t KICK_ELECTRO = (1<<0)|(1<<3)|(1<<6)|(1<<10)|(1<<13);
static const uint16_t KICK_2STEP   = (1<<0)|(1<<6)|(1<<10);
static const uint16_t KICK_FUNK    = (1<<0)|(1<<4)|(1<<7)|(1<<10)|(1<<12);
static const uint16_t KICK_GALLOP  = (1<<0)|(1<<3)|(1<<4)|(1<<8)|(1<<11)|(1<<12);
static const uint16_t KICK_NONE    = 0x0000;

static const uint16_t SNR_BACK     = (1<<4)|(1<<12);
static const uint16_t SNR_GHOST    = (1<<4)|(1<<12)|(1<<7)|(1<<15);
static const uint16_t SNR_2STEP    = (1<<4)|(1<<12)|(1<<14);
static const uint16_t SNR_NONE     = 0x0000;

static const uint16_t HHC_16TH     = 0xFFFF;
static const uint16_t HHC_OFF      = (1<<2)|(1<<6)|(1<<10)|(1<<14);
static const uint16_t HHC_GARAGE   = (1<<2)|(1<<3)|(1<<6)|(1<<7)|(1<<10)|(1<<11)|(1<<14)|(1<<15);
static const uint16_t HHC_NONE     = 0x0000;

static const uint16_t HHO_OFF      = (1<<2)|(1<<6)|(1<<10)|(1<<14);
static const uint16_t HHO_NONE     = 0x0000;

static const uint16_t RIDE_8TH     = 0x5555;
static const uint16_t RIDE_16TH    = 0xFFFF;
static const uint16_t RIDE_NONE    = 0x0000;

static inline bool Hit(uint16_t pat, int step) { return (pat >> step) & 1; }

/* ═══════════════════════════════════════════════════════════════════
 *  BANCO DE BAJOS (16 pasos: nota MIDI, 0=silencio; accent; slide)
 * ═══════════════════════════════════════════════════════════════════ */
struct BassPat { uint8_t note[16]; uint8_t acc[16]; uint8_t slide[16]; };

static const BassPat BASS_BANK[] = {
    /* 0: ostinato semicorcheas A1 (Detroit) */
    { {33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33},
      { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 1: acid wormy (slides) */
    { {33, 0,33,36, 0,33, 0,40,33, 0,33,45, 0,33,36, 0},
      { 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0},
      { 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0} },
    /* 2: deep house bouncy (octavas) */
    { {33, 0, 0,33, 0, 0,33, 0,45, 0, 0,45, 0, 0,40, 0},
      { 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 3: minimal escaso */
    { {33, 0, 0, 0, 0, 0,33, 0, 0, 0, 0, 0,33, 0,36, 0},
      { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 4: offbeat "and" (electro) */
    { { 0,33, 0,33, 0,33, 0,33, 0,33, 0,33, 0,33, 0,33},
      { 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 5: rolling octaves peak */
    { {33,33,45,33,33,33,45,33,33,33,45,33,40,40,45,40},
      { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1} },
    /* 6: 2-step garage sub (Fred again) */
    { { 0, 0,33, 0, 0,36, 0, 0,33, 0, 0, 0,40, 0,36, 0},
      { 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
      { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0} },
    /* 7: funky slap slides */
    { {33, 0,40,33, 0,45, 0,40,33, 0,33,40, 0,45,43, 0},
      { 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0},
      { 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0} },
    /* 8: driving 16th finale */
    { {33,33,33,40,33,33,33,40,33,33,33,40,45,45,40,45},
      { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
};

/* ═══════════════════════════════════════════════════════════════════
 *  BANCO DE MELODÍAS FM (hi/lo, 0=silencio)
 * ═══════════════════════════════════════════════════════════════════ */
struct Melody { uint8_t hi[16]; uint8_t lo[16]; };

static const Melody MEL_BANK[] = {
    /* 0: Detroit bells (La menor) */
    { {81, 0, 0,76, 0,79, 0, 0,81, 0, 0,84, 0,83, 0, 0},
      {69, 0, 0, 0, 0, 0,64, 0, 0, 0, 0,72, 0, 0, 0, 0} },
    /* 1: acid arp La menor */
    { {69, 0,72, 0,76, 0,72, 0,69, 0,76, 0,81, 0,76, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 2: deep house stabs acordes Am */
    { {69, 0, 0, 0,72, 0, 0, 0,76, 0, 0, 0,72, 0, 0, 0},
      {57, 0, 0, 0,60, 0, 0, 0,64, 0, 0, 0,60, 0, 0, 0} },
    /* 3: minimal pluck ostinato */
    { {81, 0,81, 0,79, 0,76, 0,81, 0,84, 0,83, 0,79, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 4: trance arp uplifting */
    { {69,72,76,81,84,81,76,72,69,72,76,81,84,88,84,81},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 5: peak melodic riff */
    { {88, 0,84,86, 0,81, 0,84,88, 0,91, 0,88,84, 0,81},
      {69, 0, 0, 0,64, 0, 0, 0,69, 0, 0, 0,72, 0, 0, 0} },
    /* 6: organic-house chord stabs (Fred again) */
    { { 0, 0,72, 0, 0, 0,76, 0, 0,74, 0, 0, 0,72, 0, 0},
      { 0, 0,64, 0, 0, 0,67, 0, 0,65, 0, 0, 0,64, 0, 0} },
    /* 7: chopped vocal hook 2-step */
    { {84, 0, 0,86,84, 0,79, 0, 0,81, 0,84, 0, 0,79, 0},
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
    /* 8: euphoric anthem finale */
    { {81,84,88,84,86,88,91,88,84,88,91,93,88,91,96,93},
      {69, 0,72, 0,76, 0,72, 0,69, 0,76, 0,81, 0,84, 0} },
};

/* ═══════════════════════════════════════════════════════════════════
 *  MODOS DE TRANSICIÓN DE MEZCLA (estilo DJ profesional)
 * ═══════════════════════════════════════════════════════════════════ */
enum TransMode : uint8_t {
    TMIX_NONE   = 0, /* corte seco + crash (para drops duros)         */
    TMIX_FILTER = 1, /* LPF sweep: bass cierra → reabre en nueva sec. */
    TMIX_ECHO   = 2, /* Echo out: delay fb sube, todo muere en ecos   */
    TMIX_WASH   = 3, /* Reverb wash: reverb a 0.95, borra sección     */
    TMIX_STRIP  = 4, /* Strip: hats/clap fuera, sólo kick → tensión   */
};

/* ═══════════════════════════════════════════════════════════════════
 *  FLAGS DE SECCIÓN
 * ═══════════════════════════════════════════════════════════════════ */
enum SecFlag : uint8_t {
    FLAG_NONE    = 0,
    FLAG_BUILDUP = 1 << 0,  /* snare roll + riser                  */
    FLAG_CRASH   = 1 << 1,  /* crash al entrar                     */
    FLAG_TOMS    = 1 << 2,  /* toms + perc tribales                */
    FLAG_FINALE  = 1 << 3,  /* crash/compás + perc rodante         */
    FLAG_FUNK    = 1 << 4,  /* rimshot + perc en síncopas          */
};

/* ═══════════════════════════════════════════════════════════════════
 *  ESTRUCTURA DE SECCIÓN
 * ═══════════════════════════════════════════════════════════════════ */
struct Section {
    uint16_t bars;
    uint16_t kick, snare, clap, hhc, hho, ride;
    int8_t   bassPat;      /* índice BASS_BANK, -1=sin bajo     */
    int8_t   melPat;       /* índice MEL_BANK,  -1=sin melodía  */
    uint8_t  fmPreset;
    float    bassCutoff;
    float    bassReso;
    float    revFb;
    float    dlyFb;
    uint8_t  swing;        /* muestras de swing (0=recto)        */
    uint8_t  flags;
    /* ── transición saliente ── */
    uint8_t  transOutBars; /* compases anticipados de transición */
    uint8_t  transMode;    /* TMIX_*                             */
};

/* ═══════════════════════════════════════════════════════════════════
 *  TABLA DE SECCIONES
 *  Cada fila incluye ahora: transOutBars | transMode
 *  El "filter-in sweep" al entrar es automático para todas las secciones
 *  con bajo (transInProg en EnterSection).
 * ═══════════════════════════════════════════════════════════════════ */
static const Section SECTIONS[] = {
/* bars kick          snare     clap      hhc        hho      ride       bp  mp  pre        cut    res    rev    dly  sw flags            tOB tMode        */
{  16, KICK_NONE,  SNR_NONE, SNR_NONE, HHC_NONE, HHO_NONE, RIDE_NONE,  -1,  0, PRE_BELL,  420, 0.82f, 0.80f, 0.45f,  0, FLAG_CRASH,      4, TMIX_WASH   }, /* 1  Detroit intro    */
{  24, KICK_FOUR,  SNR_NONE, SNR_NONE, HHC_NONE, HHO_OFF,  RIDE_8TH,    0,  0, PRE_BELL,  420, 0.82f, 0.72f, 0.45f,  0, FLAG_NONE,       4, TMIX_WASH   }, /* 2  Detroit groove   */
{   8, KICK_NONE,  SNR_NONE, SNR_NONE, HHC_NONE, HHO_NONE, RIDE_NONE,  -1,  0, PRE_BELL,  420, 0.82f, 0.90f, 0.55f,  0, FLAG_NONE,       2, TMIX_FILTER }, /* 3  Breakdown        */
{  32, KICK_FOUR,  SNR_NONE, SNR_BACK, HHC_OFF,  HHO_NONE, RIDE_NONE,   1,  1, PRE_PLUCK, 900, 0.90f, 0.45f, 0.40f,  0, FLAG_CRASH,      4, TMIX_FILTER }, /* 4  Acid house       */
{  16, KICK_FOUR,  SNR_NONE, SNR_BACK, HHC_16TH, HHO_OFF,  RIDE_NONE,   1,  1, PRE_PLUCK,1500, 0.94f, 0.40f, 0.50f,  0, FLAG_NONE,       4, TMIX_ECHO   }, /* 5  Acid peak        */
{  24, KICK_2STEP, SNR_2STEP,SNR_NONE, HHC_GARAGE,HHO_OFF, RIDE_NONE,   6,  7, PRE_KEYS,  780, 0.55f, 0.62f, 0.55f, 22, FLAG_FUNK,       4, TMIX_STRIP  }, /* 6  UK garage 2-step */
{  24, KICK_2STEP, SNR_BACK, SNR_NONE, HHC_OFF,  HHO_OFF,  RIDE_NONE,   6,  6, PRE_KEYS,  650, 0.55f, 0.72f, 0.55f, 22, FLAG_NONE,       4, TMIX_WASH   }, /* 7  Organic house    */
{  24, KICK_FOUR,  SNR_NONE, SNR_BACK, HHC_OFF,  HHO_OFF,  RIDE_NONE,   2,  2, PRE_STAB,  800, 0.70f, 0.55f, 0.45f, 18, FLAG_CRASH,      4, TMIX_FILTER }, /* 8  Deep house       */
{  24, KICK_FUNK,  SNR_GHOST,SNR_NONE, HHC_GARAGE,HHO_NONE,RIDE_NONE,   7, -1, PRE_PLUCK, 900, 0.80f, 0.48f, 0.45f, 14, FLAG_FUNK,       4, TMIX_ECHO   }, /* 9  Funky electro    */
{  32, KICK_FOUR,  SNR_NONE, SNR_NONE, HHC_NONE, HHO_OFF,  RIDE_NONE,   3,  3, PRE_MARIMBA,650,0.60f, 0.78f, 0.62f,  0, FLAG_NONE,       4, TMIX_WASH   }, /* 10 Minimal          */
{  24, KICK_ELECTRO,SNR_GHOST,SNR_NONE,HHC_16TH, HHO_NONE, RIDE_NONE,   4, -1, PRE_PLUCK, 700, 0.85f, 0.50f, 0.40f,  0, FLAG_CRASH,      4, TMIX_FILTER }, /* 11 Electro break    */
{  40, KICK_FOUR,  SNR_NONE, SNR_NONE, HHC_OFF,  HHO_OFF,  RIDE_8TH,   -1,  4, PRE_LEAD,  420, 0.82f, 0.88f, 0.55f,  0, FLAG_NONE,       6, TMIX_WASH   }, /* 12 Trance melódico  */
{  24, KICK_FOUR,  SNR_NONE, SNR_NONE, HHC_OFF,  HHO_NONE, RIDE_NONE,   0,  3, PRE_MARIMBA,500,0.70f, 0.60f, 0.50f, 12, FLAG_TOMS,       4, TMIX_STRIP  }, /* 13 Tribal perc      */
{   8, KICK_FOUR,  SNR_BACK, SNR_NONE, HHC_16TH, HHO_NONE, RIDE_NONE,  -1,  4, PRE_LEAD,  420, 0.82f, 0.92f, 0.55f,  0, FLAG_BUILDUP,    0, TMIX_NONE   }, /* 14 Buildup          */
{  32, KICK_FOUR,  SNR_NONE, SNR_BACK, HHC_16TH, HHO_OFF,  RIDE_8TH,    5,  5, PRE_STAB, 1100, 0.88f, 0.65f, 0.45f,  0, FLAG_CRASH,      4, TMIX_STRIP  }, /* 15 Peak drop        */
{   8, KICK_GALLOP,SNR_BACK, SNR_NONE, HHC_16TH, HHO_NONE, RIDE_NONE,   -1,  3, PRE_PLUCK, 420, 0.82f, 0.60f, 0.22f,  0, FLAG_BUILDUP,    0, TMIX_NONE   }, /* 16 Final buildup    */
{  48, KICK_FOUR,  SNR_NONE, SNR_BACK, HHC_NONE, HHO_NONE, RIDE_8TH,    5,  3, PRE_PLUCK,  1100,0.86f, 0.40f, 0.20f,  0, FLAG_FINALE,    8, TMIX_WASH }, /* 17 FINAL DROP */
{   8, KICK_NONE,  SNR_NONE, SNR_NONE, HHC_NONE, HHO_NONE, RIDE_NONE,  -1,  0, PRE_BELL,  420, 0.82f, 0.88f, 0.55f,  0, FLAG_NONE,       0, TMIX_NONE   }, /* 18 Reset            */
};
static constexpr int NUM_SECTIONS = (int)(sizeof(SECTIONS)/sizeof(SECTIONS[0]));

/* ── Nombres y "fórmula" de cada sección (para el monitor serial) ── */
static const char* const SEC_NAME[NUM_SECTIONS] = {
    "DETROIT INTRO", "DETROIT GROOVE", "BREAKDOWN", "ACID HOUSE",
    "ACID PEAK", "UK GARAGE 2-STEP", "ORGANIC HOUSE", "DEEP HOUSE",
    "FUNKY ELECTRO", "MINIMAL TECHNO", "ELECTRO BREAK", "TRANCE MELODICO",
    "TRIBAL PERC", "BUILDUP", "PEAK DROP", "FINAL BUILDUP",
    "FINAL DROP", "RESET"
};
static const char* const SEC_FX[NUM_SECTIONS] = {
    "ring-mod bell: y=sin(wc.t)*sin(wm.t), r=1.41",
    "ring-mod bell + 4x4 kick + ride 8th",
    "wash: reverb fb->0.95, all dissolves",
    "acid: ladder LPF, Q=0.90, env->fc",
    "acid peak: fc=1500Hz, Q=0.94 (self-osc)",
    "garage: T_odd=T-22smp shuffle, EP keys",
    "organic: Rhodes algo0 r1.0 idx1.6",
    "house stab: additive algo1, swing 18smp",
    "funk: slap bass + rimshot syncopa",
    "minimal: dub delay fb->0.62, sparse",
    "electro: kick syncopado, hats 16th",
    "trance: arp 16th, reverb tail 0.88",
    "tribal: toms+perc, swing 12smp",
    "riser: snare roll, density f(progress)",
    "peak: stab FM (fast), sub octaves rolling",
    "riser: pluck FM (short), gallop kick, snare roll",
    "FINAL: crash/bar, rolling bass, minimal pluck, ride 8th, NO-reverb",
    "reset -> loop back to intro"
};
static const char* const MIX_NAME[5] = {
    "CUT", "FILTER-SWEEP", "ECHO-OUT", "REVERB-WASH", "STRIP-KICK"
};

/* ═══════════════════════════════════════════════════════════════════
 *  ESTADO DEL SECUENCIADOR
 * ═══════════════════════════════════════════════════════════════════ */
static constexpr float BPM = 132.0f;
static const uint32_t kStepSamples =
    (uint32_t)(SAMPLE_RATE * 60.0f / BPM / 4.0f);

static uint32_t stepCounter = 0;
static uint32_t curStepLen  = kStepSamples;
static int      step16      = 0;
static uint32_t barCounter  = 0;
static int      secIdx      = 0;
static uint16_t secBar      = 0;
static Section  cur         = SECTIONS[0];

static float ledLevel   = 0.0f;
static float rideGain   = 0.0f;
static float masterGain = 1.0f;

/* Objetivos de reverb/delay (lerp en el callback) */
static float revFb    = 0.80f;
static float revFbTgt = 0.80f;

/* ═══════════════════════════════════════════════════════════════════
 *  ESTADO COMPARTIDO PARA EL MONITOR SERIAL
 *  Escrito desde el audio callback (sólo stores simples), leído desde
 *  el main loop. PrintLine NO se llama nunca desde el callback.
 * ═══════════════════════════════════════════════════════════════════ */
static constexpr bool kMonitor = true;   /* monitor serial USB on/off */
static volatile bool     monSecChanged = true;
static volatile int      monSec        = 0;
static volatile uint32_t monBar        = 0;
static volatile float    monVU         = 0.0f;
static volatile float    monTransOut   = 0.0f;
static volatile uint8_t  monMode       = 0;
static volatile float    monCutoff     = 420.0f;
static volatile float    monRev        = 0.80f;
static volatile float    monDly        = 0.45f;
static volatile int      monStep16     = 0;      /* paso actual 0-15       */
static volatile uint8_t  monFmVoices   = 0;      /* voces FM activas       */
static float monVuPeak = 0.0f;

/* ── Estado de transición ─────────────────────────────────────────
 *  transOut:    0→1 durante los últimos transOutBars de la sección.
 *               Controla el sweep de filtro/echo/wash/strip salientes.
 *  transInProg: 1→0 durante los primeros kTransInBars tras entrar.
 *               Barre el filtro del bajo de "cerrado" a "objetivo".
 * ─────────────────────────────────────────────────────────────────*/
static float transOut    = 0.0f;
static float transInProg = 0.0f;
static constexpr int kTransInBars = 4;

/* ── Cutoff efectivo del bajo (calculado en AudioCallback) ── */
static float bassCutoffEff = 420.0f;

/* ── Gain de drums (para pull suave en FILTER) ── */
static float drumsGainEff = 0.9f;

static inline uint32_t StepLength(int s)
{
    int32_t sw = (int32_t)cur.swing;
    if(!sw) return kStepSamples;
    return ((s & 1) == 0) ? kStepSamples + (uint32_t)sw
                           : (kStepSamples > (uint32_t)sw ? kStepSamples - sw : 1u);
}

static void EnterSection()
{
    cur          = SECTIONS[secIdx];
    transOut     = 0.0f;
    transInProg  = (cur.bassPat >= 0) ? 1.0f : 0.0f; /* filter-in si hay bajo */
    revFbTgt     = cur.revFb;
    dlyFbTgt     = cur.dlyFb;
    rideGain     = 0.0f;
    bassCutoffEff = (cur.bassPat >= 0) ? 80.0f : cur.bassCutoff; /* empieza cerrado */
    drumsGainEff  = 0.9f;
    bass.SetResonance(cur.bassReso);

    if(cur.flags & FLAG_CRASH)
        drums.Trigger(TR909::INST_CRASH, 0.85f);

    /* Avisar al monitor del cambio de sección */
    monSec        = secIdx;
    monSecChanged = true;
}

/* ═══════════════════════════════════════════════════════════════════
 *  SEQUENCER TICK  (llamado una vez por paso, al inicio del step)
 * ═══════════════════════════════════════════════════════════════════ */
static void SequencerTick()
{
    /* ── Inicio de compás ── */
    if(step16 == 0){
        /* ¿Cambio de sección? */
        if(secBar >= cur.bars){
            secIdx = (secIdx + 1) % NUM_SECTIONS;
            secBar = 0;
            EnterSection();
        }

        /* Actualizar transOut */
        if(cur.transOutBars > 0){
            int barsLeft = (int)cur.bars - (int)secBar; /* incluyendo este compás */
            if(barsLeft <= (int)cur.transOutBars)
                transOut = Clampf(1.0f - (float)(barsLeft - 1) / (float)cur.transOutBars,
                                  0.0f, 1.0f);
            else
                transOut = 0.0f;
        } else {
            transOut = 0.0f;
        }

        /* Decay del filter-in sweep */
        if(transInProg > 0.0f){
            transInProg -= 1.0f / (float)kTransInBars;
            if(transInProg < 0.0f) transInProg = 0.0f;
        }
    }

    /* ── ¿Strip mode activo? suprimir hats/clap en transición saliente ── */
    const bool stripping = (cur.transMode == TMIX_STRIP) && (transOut > 0.45f);

    const bool buildup = (cur.flags & FLAG_BUILDUP);
    const float secProg =
        (cur.bars > 0) ? ((float)secBar + (float)step16/16.0f) / (float)cur.bars : 0.0f;

    /* ── KICK ── */
    if(Hit(cur.kick, step16))
        drums.Trigger(TR909::INST_KICK, 1.0f);

    /* ── SNARE (roll en buildup) ── */
    if(buildup){
        bool roll = (secProg < 0.5f) ? (step16 % 4 == 0)
                  : (secProg < 0.8f) ? (step16 % 2 == 0) : true;
        if(roll)
            drums.Trigger(TR909::INST_SNARE, 0.35f + 0.65f * secProg);
    } else if(!stripping && Hit(cur.snare, step16)){
        float v = (step16==4 || step16==12) ? 0.9f : 0.45f;
        drums.Trigger(TR909::INST_SNARE, v);
    }

    /* ── CLAP ── */
    if(!stripping && Hit(cur.clap, step16))
        drums.Trigger(TR909::INST_CLAP, 0.85f);

    /* ── HI-HAT cerrado ── */
    if(!stripping && Hit(cur.hhc, step16))
        drums.Trigger(TR909::INST_HIHAT_C, (step16%2==0) ? 0.6f : 0.4f);

    /* ── HI-HAT abierto ── */
    if(!stripping && Hit(cur.hho, step16))
        drums.Trigger(TR909::INST_HIHAT_O, 0.7f);

    /* ── RIDE con fade-in ── */
    if(!stripping && Hit(cur.ride, step16)){
        rideGain += (1.0f - rideGain) * 0.02f;
        drums.SetVolume(TR909::INST_RIDE, 0.55f * rideGain);
        drums.Trigger(TR909::INST_RIDE, 0.6f);
    }

    /* ── TOMS tribales ── */
    if(cur.flags & FLAG_TOMS){
        if(step16==2 || step16==11) drums.Trigger(TR909::INST_LOW_TOM, 0.7f);
        if(step16==6 || step16==14) drums.Trigger(TR909::INST_MID_TOM, 0.6f);
        if(step16==3 || step16==9 || step16==13) drums.Trigger(TR909::INST_HI_PERC, 0.5f);
        if(step16==1 || step16==7)  drums.Trigger(TR909::INST_SHAKER, 0.5f);
    }

    /* ── FUNK: rimshot + perc en síncopas ── */
    if(cur.flags & FLAG_FUNK){
        if(step16==3 || step16==7 || step16==11) drums.Trigger(TR909::INST_RIMSHOT, 0.45f);
        if(step16==6 || step16==14)              drums.Trigger(TR909::INST_HI_PERC, 0.4f);
    }

    /* ── FINALE: crash en cada compás (limpio, sin solapamiento) ── */
    if(cur.flags & FLAG_FINALE){
        if(step16 == 0)  drums.Trigger(TR909::INST_CRASH,   0.42f);
        if(step16 == 10) drums.Trigger(TR909::INST_LOW_TOM, 0.48f);
    }

    /* ── MELODÍA FM ── */
    if(cur.melPat >= 0){
        const Melody& m = MEL_BANK[cur.melPat];
        uint8_t hi = m.hi[step16];
        if(hi) FmNoteOn(hi, ((step16%4)==0) ? 0.9f : 0.6f, cur.fmPreset);
        uint8_t lo = m.lo[step16];
        if(lo) FmNoteOn(lo, 0.65f, cur.fmPreset);
    }

    /* ── BAJO 303 ── */
    if(cur.bassPat >= 0){
        const BassPat& bp = BASS_BANK[cur.bassPat];
        uint8_t note = bp.note[step16];
        if(note)
            bass.NoteOn(note, bp.acc[step16] != 0, bp.slide[step16] != 0);
    }

    ledLevel = (step16 % 4 == 0) ? 1.0f : 0.4f;
}

/* ═══════════════════════════════════════════════════════════════════
 *  AUDIO CALLBACK  — mezcla + automatización de transiciones
 * ═══════════════════════════════════════════════════════════════════ */
void AudioCallback(AudioHandle::InputBuffer  /*in*/,
                   AudioHandle::OutputBuffer  out,
                   size_t                     size)
{
    /* ── Automatización de FX según transOut + transMode ─────────────
     *
     *  TMIX_FILTER: el bass LPF cierra progresivamente (sweep out).
     *               Al entrar la nueva sección, transInProg abre el filtro.
     *               Los drums se atenúan un poco (simula pull del EQ).
     *
     *  TMIX_ECHO:   delay fb sube a 0.82 → eco que se pierde.
     *               Reverb wash adicional moderado.
     *
     *  TMIX_WASH:   reverb feedback sube a 0.95 → todo se disuelve en
     *               una cola enorme. El bass también cierra suavemente.
     *
     *  TMIX_STRIP:  supresor de hats/clap ya activo en SequencerTick.
     *               Reverb sube un poco para añadir tensión armónica.
     *               Bass cierra 30% para limpiar el espectro.
     * ──────────────────────────────────────────────────────────────── */

    /* Calcular cutoff efectivo y objetivos FX (por bloque, ~1 ms) */
    float tRevFb = cur.revFb;
    float tDlyFb = cur.dlyFb;

    if(transOut > 0.0f){
        switch(cur.transMode){
            case TMIX_FILTER:
                /* Cierra el bajo de cutoff → 80 Hz */
                bassCutoffEff += (Lerp(cur.bassCutoff, 80.0f, transOut) - bassCutoffEff) * 0.03f;
                /* Pull suave de drums (como bajar el EQ mid del canal) */
                drumsGainEff = 0.9f * (1.0f - transOut * 0.3f);
                /* Reverb sube ligeramente */
                tRevFb = cur.revFb + (0.87f - cur.revFb) * transOut * 0.6f;
                break;

            case TMIX_ECHO:
                /* Delay feedback sube → eco que se pierde */
                tDlyFb = cur.dlyFb + (0.82f - cur.dlyFb) * transOut;
                /* Reverb wash moderado */
                tRevFb = cur.revFb + (0.90f - cur.revFb) * transOut * 0.7f;
                /* Bass cierra 40% también */
                bassCutoffEff += (Lerp(cur.bassCutoff, cur.bassCutoff * 0.5f, transOut * 0.7f)
                                  - bassCutoffEff) * 0.03f;
                break;

            case TMIX_WASH:
                /* Reverb sube a 0.95 → cola enorme */
                tRevFb = cur.revFb + (0.95f - cur.revFb) * transOut;
                /* Delay también sube un poco */
                tDlyFb = cur.dlyFb + (cur.dlyFb * 1.3f - cur.dlyFb) * transOut * 0.5f;
                /* Bass cierra suave */
                bassCutoffEff += (Lerp(cur.bassCutoff, 120.0f, transOut * 0.6f)
                                  - bassCutoffEff) * 0.02f;
                break;

            case TMIX_STRIP:
                /* Tensión: reverb sube, bass cierra 30% */
                tRevFb = cur.revFb + (0.88f - cur.revFb) * transOut * 0.5f;
                bassCutoffEff += (Lerp(cur.bassCutoff, cur.bassCutoff * 0.6f, transOut * 0.5f)
                                  - bassCutoffEff) * 0.02f;
                break;

            default:
                bassCutoffEff += (cur.bassCutoff - bassCutoffEff) * 0.05f;
                break;
        }
    } else if(transInProg > 0.0f){
        /* Filter-in sweep: el bajo abre de 80 Hz → target de la sección */
        float openCutoff = Lerp(80.0f, cur.bassCutoff, 1.0f - transInProg);
        bassCutoffEff += (openCutoff - bassCutoffEff) * 0.04f;
        drumsGainEff   = 0.9f;
    } else {
        /* Estado estable: lerp rápido al target */
        bassCutoffEff += (cur.bassCutoff - bassCutoffEff) * 0.05f;
        drumsGainEff   = 0.9f;
    }

    /* Aplicar cutoff al bajo (por bloque es suficiente) */
    bass.SetCutoff(Clampf(bassCutoffEff, 40.0f, 18000.0f));

    /* Lerp de reverb/delay */
    revFb += (tRevFb - revFb) * 0.015f;
    dlyFb += (tDlyFb - dlyFb) * 0.015f;
    reverb.SetFeedback(Clampf(revFb, 0.0f, 0.99f));

    /* Ganancia máster — FINALE ya tiene suficiente energía sin empuje extra */
    float gainTgt = 1.0f;
    masterGain += (gainTgt - masterGain) * 0.001f;

    /* ── Bucle de samples ── */
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

        /* Ganancia de drums: reducida en FINALE para evitar saturación
         * (gallop kick + 16th hats + ride + crash acumulan mucho RMS) */
        float dGain = drumsGainEff * ((cur.flags & FLAG_FINALE) ? 0.70f : 1.0f);
        float drumMix = drums.Process() * dGain;

        /* Delay ping-pong */
        size_t rpL = (dlyWp + DLY_SIZE - dlyTimeL) % DLY_SIZE;
        size_t rpR = (dlyWp + DLY_SIZE - dlyTimeR) % DLY_SIZE;
        float dL   = dlyBufL[rpL];
        float dR   = dlyBufR[rpR];

        float fmMix = 0.0f;
        for(int v = 0; v < NUM_FM; v++)
            fmMix += fmv[v].Process();
        fmMix *= 0.30f;   /* escalado global; anthem melody activa 8 voces */

        float bassMix = bass.Process() * 0.85f;

        /* Alimentar delay (cross-feed L↔R) */
        dlyBufL[dlyWp] = drumMix * 0.14f + fmMix * 0.18f + dR * dlyFb;
        dlyBufR[dlyWp] = drumMix * 0.14f + fmMix * 0.18f + dL * dlyFb;
        dlyWp = (dlyWp + 1) % DLY_SIZE;

        float dryL = drumMix * 0.85f + fmMix * 0.80f + bassMix + dL * 0.40f;
        float dryR = drumMix * 0.85f + fmMix * 0.80f + bassMix + dR * 0.40f;

        /* Reverb sobre FM + colas del delay.
         * Bypass en FLAG_FINALE: ReverbSc es la op más cara (~20% CPU/smp)
         * y en el drop ya hay suficiente energía sin cola de reverb. */
        float wetL = 0.0f, wetR = 0.0f;
        if(!(cur.flags & FLAG_FINALE))
            reverb.Process(fmMix * 0.80f + dL * 0.35f, fmMix * 0.80f + dR * 0.35f, &wetL, &wetR);

        float outL = (dryL + wetL * 0.40f) * masterGain;
        float outR = (dryR + wetR * 0.40f) * masterGain;

        float sL = tanhf(outL * 0.7f);
        float sR = tanhf(outR * 0.7f);
        out[0][i] = sL;
        out[1][i] = sR;

        /* Pico para el VU (monitor) */
        float a = fabsf(sL);
        if(a > monVuPeak) monVuPeak = a;
    }

    /* Publicar estado para el monitor (sólo stores; sin printf aquí) */
    monVuPeak  *= 0.6f;
    monVU       = monVuPeak;
    monBar      = (uint32_t)secBar;
    monTransOut = transOut;
    monMode     = cur.transMode;
    monCutoff   = bassCutoffEff;
    monRev      = revFb;
    monDly      = dlyFb;
    monStep16   = step16;
    /* contar voces FM activas */
    uint8_t fmAct = 0;
    for(int v = 0; v < NUM_FM; v++) if(fmv[v].IsActive()) fmAct++;
    monFmVoices = fmAct;

    ledLevel *= 0.85f;
    hw.SetLed(ledLevel > 0.2f);
}

/* ═══════════════════════════════════════════════════════════════════
 *  MONITOR SERIAL — banner por sección + línea viva con VU ASCII
 * ═══════════════════════════════════════════════════════════════════ */

/* Renderiza un patrón de 16 pasos (bitmask) en "X . . X ..." */
static void RenderPattern(uint16_t pat, char* buf)
{
    int p = 0;
    for(int s = 0; s < 16; s++){
        buf[p++] = Hit(pat, s) ? 'X' : '.';
        buf[p++] = ' ';
        if((s & 3) == 3 && s != 15) buf[p++] = '|', buf[p++] = ' ';
    }
    buf[p] = '\0';
}

/* hw.PrintLine emite solo \n; los terminales serie raw necesitan \r\n.
 * Este wrapper antepone \r para devolver el cursor a la columna 0 y
 * evitar el efecto escalera diagonal. */
#define PL(...) hw.PrintLine("\r" __VA_ARGS__)

/* Barra VU ASCII: nivel 0..1 → "##########----------" (width chars) */
static void RenderBar(float level, int width, char fill, char* buf)
{
    int n = (int)(level * (float)width + 0.5f);
    if(n < 0) n = 0; if(n > width) n = width;
    int p = 0;
    for(int i = 0; i < width; i++) buf[p++] = (i < n) ? fill : '-';
    buf[p] = '\0';
}

/* Color por sección (género/energía) para el banner y la línea viva */
static const char* SecColor(int idx)
{
    const Section& s = SECTIONS[idx];
    if(s.flags & FLAG_FINALE)  return C(A_BRED);   /* drop final: rojo  */
    if(s.flags & FLAG_BUILDUP) return C(A_BYEL);   /* riser: amarillo   */
    if(s.flags & FLAG_TOMS)    return C(A_MAG);    /* tribal: magenta   */
    if(s.flags & FLAG_FUNK)    return C(A_BMAG);   /* funky: magenta br */
    switch(idx % 6){                               /* resto: rota tonos */
        case 0: return C(A_BCYN);
        case 1: return C(A_BGRN);
        case 2: return C(A_BBLU);
        case 3: return C(A_CYN);
        case 4: return C(A_GRN);
        default:return C(A_BLU);
    }
}

/* Barra VU con gradiente: verde (<60%) -> amarillo (<85%) -> rojo */
static void RenderVuColor(float level, int width, char* buf)
{
    int n = (int)(level * (float)width + 0.5f);
    if(n < 0) n = 0; if(n > width) n = width;
    int p = 0;
    const char* lastCol = "";
    for(int i = 0; i < width; i++){
        float frac = (float)i / (float)width;
        const char* col;
        if(i >= n)            col = C(A_DIM);              /* vacío */
        else if(frac < 0.60f) col = C(A_BGRN);
        else if(frac < 0.85f) col = C(A_BYEL);
        else                  col = C(A_BRED);
        if(col != lastCol){                                /* sólo cambia color cuando hace falta */
            const char* q = col;
            while(*q) buf[p++] = *q++;
            lastCol = col;
        }
        buf[p++] = (i < n) ? '#' : '-';
    }
    const char* r = C(A_RST);
    while(*r) buf[p++] = *r++;
    buf[p] = '\0';
}

/* Render indicador de paso actual en 16 posiciones */
static void RenderStepPos(int stp, char* buf)
{
    int p = 0;
    for(int s = 0; s < 16; s++){
        buf[p++] = (s == stp) ? '>' : '.';
        if((s & 3) == 3 && s != 15){ buf[p++] = '|'; }
    }
    buf[p] = '\0';
}

/* Barra de progreso de compás dentro de sección: "=====>----" */
static void RenderProgress(int bar, int bars, int width, char* buf)
{
    int n = (bars > 1) ? ((bar - 1) * width / (bars - 1)) : width;
    if(n < 0) n = 0; if(n > width) n = width;
    int p = 0;
    for(int i = 0; i < width; i++)
        buf[p++] = (i < n) ? (i == n-1 ? '>' : '=') : '-';
    buf[p] = '\0';
}

/* Banner completo al entrar una sección */
static void MonitorBanner(int idx)
{
    if(!kMonitor) return;
    const Section& s = SECTIONS[idx];
    char kp[52], rp[52];
    RenderPattern(s.kick, kp);
    RenderPattern(s.ride, rp);

    int swMs10 = (int)((float)s.swing / SAMPLE_RATE * 10000.0f);

    /* separador visual proporcional a la sección */
    const char* energy = (s.flags & FLAG_FINALE) ? "!!!!!!" :
                         (s.flags & FLAG_BUILDUP) ? ">>>..." :
                         (s.flags & FLAG_CRASH)   ? "***..." : "------";
    const char* col = SecColor(idx);

    PL("");
    PL("%s+==================================================+%s", col, C(A_RST));
    PL("%s|%s %s[%2d/%2d]%s %s%-26s%s%3d bars %s|%s",
       col, C(A_RST), C(A_BOLD), idx+1, NUM_SECTIONS, C(A_RST),
       col, SEC_NAME[idx], C(A_RST), (int)s.bars, col, C(A_RST));
    PL("%s|%s  %s%s%s  BPM=%d  block=%lu smp/step             %s|%s",
       col, C(A_RST), C(A_BWHT), energy, C(A_RST),
       (int)BPM, (unsigned long)kStepSamples, col, C(A_RST));
    PL("%s+==================================================+%s", col, C(A_RST));
    if(s.swing)
        PL("  %sswing%s  T+=%-3d T-=%-3d smp  (~%d.%d ms)",
           C(A_DIM), C(A_RST), (int)s.swing, (int)s.swing, swMs10/10, swMs10%10);
    PL("  %skick%s : %s%s%s", C(A_BBLU), C(A_RST), C(A_BWHT), kp, C(A_RST));
    PL("  %sride%s : %s%s%s", C(A_BCYN), C(A_RST), C(A_DIM), rp, C(A_RST));
    if(s.bassPat >= 0)
        PL("  %sbass%s : pat#%d  fc=%dHz  Q=%.2f",
           C(A_BGRN), C(A_RST), (int)s.bassPat, (int)s.bassCutoff, s.bassReso);
    else
        PL("  %sbass%s : --", C(A_DIM), C(A_RST));
    if(s.melPat >= 0)
        PL("  %smel%s  : pat#%d  preset=%d  fm_voices<=%d",
           C(A_BMAG), C(A_RST), (int)s.melPat, (int)s.fmPreset, NUM_FM);
    else
        PL("  %smel%s  : --", C(A_DIM), C(A_RST));
    PL("  %sFX%s   : %s%s%s", C(A_BYEL), C(A_RST), C(A_YEL), SEC_FX[idx], C(A_RST));
    if(s.transOutBars)
        PL("  %smix>>%s: %s%s%s  (%d bars out)",
           C(A_BRED), C(A_RST), C(A_BRED), MIX_NAME[s.transMode], C(A_RST),
           (int)s.transOutBars);
    PL("  reverb=%s%s%s  rev=%.2f  dly=%.2f",
       (s.flags & FLAG_FINALE) ? C(A_BRED) : C(A_BGRN),
       (s.flags & FLAG_FINALE) ? "BYPASS" : "ON", C(A_RST),
       s.revFb, s.dlyFb);
    PL("%s--------------------------------------------------%s", C(A_DIM), C(A_RST));
}

/* Línea viva cada compás */
static void MonitorLive()
{
    if(!kMonitor) return;
    int    idx   = monSec;
    float  vu    = monVU;
    float  tout  = monTransOut;
    int    bar   = (int)monBar + 1;
    int    bars  = (int)SECTIONS[idx].bars;
    int    stp   = monStep16;
    int    fmv_n = (int)monFmVoices;
    int    beat  = stp / 4 + 1;   /* 1-4 */

    char vubar[160], stepbuf[24], progbuf[14];
    RenderVuColor(vu, 16, vubar);
    RenderStepPos(stp, stepbuf);
    RenderProgress(bar, bars, 12, progbuf);

    const char* col   = SecColor(idx);
    /* color de fm voices: verde pocas, amarillo medio, rojo saturado */
    const char* fmCol = (fmv_n >= 7) ? C(A_BRED) : (fmv_n >= 5) ? C(A_BYEL) : C(A_BGRN);
    /* beat 1 destacado (downbeat) */
    const char* beatCol = (stp == 0) ? C(A_BWHT) : C(A_DIM);

    if(tout > 0.001f){
        char tbar[14];
        RenderBar(tout, 10, '>', tbar);
        PL(" %sB%d%s s%02d [%s%s%s] VU[%s] [%s%s%s] %s>>%s%s %s%s%s",
           beatCol, beat, C(A_RST), stp,
           col, stepbuf, C(A_RST), vubar,
           col, progbuf, C(A_RST),
           C(A_BRED), MIX_NAME[monMode], C(A_RST),
           C(A_BRED), tbar, C(A_RST));
    } else {
        PL(" %sB%d%s s%02d [%s%s%s] VU[%s] [%s%s%s] fc=%d rev%.2f fm:%s%d%s",
           beatCol, beat, C(A_RST), stp,
           col, stepbuf, C(A_RST), vubar,
           col, progbuf, C(A_RST),
           (int)monCutoff, monRev, fmCol, fmv_n, C(A_RST));
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════════════ */
int main()
{
    __asm volatile("VMRS r0, FPSCR\n"
                   "ORR  r0, r0, #(1<<24)|(1<<25)\n"
                   "VMSR FPSCR, r0" ::: "r0");
    *(volatile uint32_t*)0xE000EF3Cu |= (1u << 24) | (1u << 25);

    hw.Init();
    hw.SetAudioBlockSize(AUDIO_BLOCK);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    /* USB serial (false = no bloquear esperando terminal) */
    if(kMonitor) hw.StartLog(false);

    drums.Init(SAMPLE_RATE);
    drums.kick.SetDrive(1.0f);
    drums.kick.SetDecay(0.55f);
    drums.kick.SetCompression(1.0f);
    drums.hihatO.SetDecay(1.6f);
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

    for(int v = 0; v < NUM_FM; v++){
        fmv[v].Init(SAMPLE_RATE);
        ApplyPreset(fmv[v], PRE_BELL);
    }

    bass.Init(SAMPLE_RATE);
    bass.SetWaveform(TB303::WAVE_SAW);
    bass.SetCutoff(420.0f);
    bass.SetResonance(0.82f);
    bass.SetEnvMod(0.8f);
    bass.SetDecay(0.12f);
    bass.SetAccent(0.85f);
    bass.SetOverdrive(0.5f);
    bass.SetVolume(0.8f);

    reverb.Init(SAMPLE_RATE);
    reverb.SetFeedback(0.80f);
    reverb.SetLpFreq(8500.0f);

    for(size_t i = 0; i < DLY_SIZE; i++){ dlyBufL[i] = 0.0f; dlyBufR[i] = 0.0f; }

    secIdx = 0; secBar = 0;
    EnterSection();

    hw.StartAudio(AudioCallback);

    /* Banner de bienvenida */
    if(kMonitor){
        if(kAnsi) hw.PrintLine(A_CLR);                /* limpia pantalla */
        PL("");
        PL("%s%s###################################################%s", C(A_BOLD), C(A_BRED), C(A_RST));
        PL("%s%s#%s   %sRED808 JOURNEY%s  -  live monitor  (USB serial) %s%s#%s",
           C(A_BOLD), C(A_BRED), C(A_RST), C(A_BWHT), C(A_RST), C(A_BOLD), C(A_BRED), C(A_RST));
        PL("%s%s#%s   %s18 secciones / ~12.6 min / 132 BPM%s            %s%s#%s",
           C(A_BOLD), C(A_BRED), C(A_RST), C(A_BCYN), C(A_RST), C(A_BOLD), C(A_BRED), C(A_RST));
        PL("%s%s#%s   %sFM ring-mod + TR909 + TB303 + reverb/delay%s    %s%s#%s",
           C(A_BOLD), C(A_BRED), C(A_RST), C(A_BGRN), C(A_RST), C(A_BOLD), C(A_BRED), C(A_RST));
        PL("%s%s###################################################%s", C(A_BOLD), C(A_BRED), C(A_RST));
    }

    /* ── Monitor loop: banner al cambiar de sección + línea viva ── */
    uint32_t liveTick = 0;
    while(1){
        if(monSecChanged){
            monSecChanged = false;
            MonitorBanner(monSec);
            liveTick = 0;
        }
        /* Línea viva ~cada 500 ms (2 Hz) */
        if(kMonitor && (++liveTick >= 5)){
            liveTick = 0;
            MonitorLive();
        }
        System::Delay(100);
    }
}

/* ── Fault handler SOS ── */
static void FaultDelay(uint32_t ms)
{
    volatile uint32_t cycles = ms * 240000u;
    while(cycles--) __asm volatile("nop");
}
static void FaultSosLoop(void)
{
    __disable_irq();
    while(1){
        for(int i=0;i<3;i++){hw.SetLed(true);FaultDelay(120);hw.SetLed(false);FaultDelay(120);}
        FaultDelay(250);
        for(int i=0;i<3;i++){hw.SetLed(true);FaultDelay(450);hw.SetLed(false);FaultDelay(150);}
        FaultDelay(250);
        for(int i=0;i<3;i++){hw.SetLed(true);FaultDelay(120);hw.SetLed(false);FaultDelay(120);}
        FaultDelay(1500);
    }
}
extern "C" void HardFault_Handler(void) { FaultSosLoop(); }
extern "C" void MemManage_Handler(void) { FaultSosLoop(); }
extern "C" void BusFault_Handler(void)  { FaultSosLoop(); }
extern "C" void UsageFault_Handler(void){ FaultSosLoop(); }
