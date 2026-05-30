/* ═══════════════════════════════════════════════════════════════════
 *  DEMO BELLS — "RED808 Journey" · Daisy Seed standalone
 * ─────────────────────────────────────────────────────────────────
 *  Un viaje de ~9.9 min que recorre estilos electrónicos modernos y
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
 *  Recorrido (loop infinito, ~9.9 min/vuelta · "tight set"):
 *     1  Detroit intro          bells hipnóticas + whoosh largo de entrada
 *     2  Detroit groove         kick + ride + bells + bass (drop con crash)
 *     3  Breakdown              mini-respiro 4 bars antes del acid
 *     4  Acid house             303 ácido wormy + pluck FM
 *     5  Acid peak              303 abierto al máximo
 *     6  UK garage / 2-step     Fred again..: shuffle, EP cálido
 *     7  Organic house emotivo  4x4 suave, EP, octavas
 *     8  Deep house             swing, claps, stabs
 *     9  Funky electro          slap bass, rimshots, síncopa
 *    10  Micro-break            sin kick, fc=200, reverb wash (CONTRASTE)
 *    11  Minimal techno         sparse, dub delay profundo
 *    12  Trance supersaw        supersaw arp FM + sub, reverb enorme
 *    13  Tribal percussion      toms + perc + stab chords
 *    14  Buildup                snare roll + riser de reverb
 *    15  Peak-time drop         anthem riff (mel8) + pluck, máxima energía
 *    16  Final buildup          riser + gallop kick + snare roll
 *    17  FINAL DROP             driving bass16 + peak riff, crash/compás (SLAM)
 *    18  APOTEOSIS              anthem mel8 + bass16, tom fills, LOCURA!!!
 *    19  Reset                  silencio breve → vuelve al inicio
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
#include "synth/tr808.h"
#include "synth/tr505.h"
#include "synth/tb303.h"
#include "synth/sh101.h"
#include "synth/fm2op.h"
#include "synth/wavetable_osc.h"

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
 *  MOTORES — los 7 engines instanciados
 *    · Kits TR9/8/505, TB303, SH101, FM, WavetableOsc → SRAM normal
 *      (sus constructores corren antes de hw.Init(); en SDRAM → HardFault).
 *    · ReverbSc y buffers grandes → SDRAM (constructor trivial).
 *  Por sección se SELECCIONA un engine de cada tipo (drum/bass/lead) y
 *  sólo ése se procesa en el callback → coste de CPU ~constante, pero a lo
 *  largo del viaje suenan los 7.
 * ═══════════════════════════════════════════════════════════════════ */
static TR909::Kit   drums909;
static TR808::Kit   drums808;
static TR505::Kit   drums505;
static TB303::Synth bass303;
static SH101::Synth bassSH;
static WavetableOsc wt;
DSY_SDRAM_BSS static ReverbSc reverb;

/* ── Selectores de engine (índices en SECTIONS) ── */
enum DrumKit : uint8_t { DK_909 = 0, DK_808, DK_505 };
enum BassEng : uint8_t { BE_303 = 0, BE_SH101 };
enum LeadEng : uint8_t { LE_FM  = 0, LE_WT };
static const char* const DK_NAME[3] = { "TR909", "TR808", "TR505" };
static const char* const BE_NAME[2] = { "TB303", "SH101" };
static const char* const LE_NAME[2] = { "FM2OP", "WTOSC" };

/* Engine activo (se fija en EnterSection) */
static uint8_t curDrumKit = DK_909;
static uint8_t curBassEng = BE_303;
static uint8_t curLeadEng = LE_FM;

/* ── Instrumentos genéricos de batería → mapeados a cada kit ──
 *  Todos los kits comparten KICK..HI_TOM (0-7). A partir de ahí divergen;
 *  mapeamos cada genérico al equivalente más cercano de cada máquina. */
enum GInst : uint8_t {
    G_KICK=0, G_SNARE, G_CLAP, G_HHC, G_HHO, G_LTOM, G_MTOM,
    G_RIDE, G_CRASH, G_RIM, G_SHK, G_PERC, G_COUNT
};
/*                          KICK SNR CLP HHC HHO LTM MTM RIDE CRSH RIM SHK PERC */
static const uint8_t MAP909[G_COUNT] = { 0,  1,  2,  3,  4,  5,  6,  8,   9, 10, 11, 13 };
static const uint8_t MAP808[G_COUNT] = { 0,  1,  2,  3,  4,  5,  6, 15,  15, 13, 12, 10 };
static const uint8_t MAP505[G_COUNT] = { 0,  1,  2,  3,  4,  5,  6,  9,   9, 10, 11, 13 };

static inline void DrumTrig(uint8_t g, float vel)
{
    switch(curDrumKit){
        case DK_808: drums808.Trigger(MAP808[g], vel); break;
        case DK_505: drums505.Trigger(MAP505[g], vel); break;
        default:     drums909.Trigger(MAP909[g], vel); break;
    }
}
static inline float DrumProcess()
{
    switch(curDrumKit){
        case DK_808: return drums808.Process();
        case DK_505: return drums505.Process();
        default:     return drums909.Process();
    }
}

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

/* ── LEAD router: FM2Op (8 voces) o WavetableOsc (polifónico interno) ──
 *  El preset FM se mapea a una forma de onda WT cuando el lead es WTOSC. */
static uint8_t WtWaveForPreset(uint8_t preset)
{
    switch(preset){
        case PRE_BELL:    return WT_WAVE_SOFTSINE;
        case PRE_PLUCK:   return WT_WAVE_SAW;
        case PRE_LEAD:    return WT_WAVE_SAW;
        case PRE_MARIMBA: return WT_WAVE_SINE;
        case PRE_STAB:    return WT_WAVE_PULSE25;
        case PRE_KEYS:    return WT_WAVE_ORGAN;
        case PRE_SUPER:   return WT_WAVE_SOFTSQ;
        default:          return WT_WAVE_SAW;
    }
}
static void LeadNoteOn(uint8_t midiNote, float vel, uint8_t preset)
{
    if(curLeadEng == LE_WT)
        wt.NoteOn(midiNote, vel, (float)WtWaveForPreset(preset));
    else
        FmNoteOn(midiNote, vel, preset);
}
static inline float LeadProcess()
{
    if(curLeadEng == LE_WT)
        return wt.Process();
    float m = 0.0f;
    for(int v = 0; v < NUM_FM; v++) m += fmv[v].Process();
    return m;
}
static inline int LeadVoiceCount()
{
    if(curLeadEng == LE_WT){
        /* WavetableOsc no expone el contador; estimamos por NUM */
        return 0;  /* se rellena en callback si hace falta */
    }
    int n = 0;
    for(int v = 0; v < NUM_FM; v++) if(fmv[v].IsActive()) n++;
    return n;
}

/* ── BASS router: TB303 (acc/slide) o SH101 (vel) ── */
static inline void BassNoteOn(uint8_t note, bool acc, bool slide)
{
    if(curBassEng == BE_SH101)
        bassSH.NoteOn(note, acc ? 1.0f : 0.7f);
    else
        bass303.NoteOn(note, acc, slide);
}
static inline float BassProcess()
{
    return (curBassEng == BE_SH101) ? bassSH.Process() : bass303.Process();
}
static inline void BassSetCutoff(float fc)
{
    if(curBassEng == BE_SH101) bassSH.params.cutoff = Clampf(fc, 40.0f, 18000.0f);
    else                       bass303.SetCutoff(Clampf(fc, 40.0f, 18000.0f));
}
static inline void BassSetResonance(float q)
{
    if(curBassEng == BE_SH101) bassSH.params.resonance = Clampf(q, 0.0f, 0.95f);
    else                       bass303.SetResonance(q);
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
{  16, KICK_NONE,  SNR_NONE, SNR_NONE, HHC_NONE, HHO_NONE, RIDE_NONE,  -1,  0, PRE_BELL,  420, 0.82f, 0.80f, 0.45f,  0, FLAG_CRASH,      8, TMIX_WASH   }, /* 1  Detroit intro    */
{  16, KICK_FOUR,  SNR_NONE, SNR_NONE, HHC_NONE, HHO_OFF,  RIDE_8TH,    0,  0, PRE_BELL,  420, 0.82f, 0.72f, 0.45f,  0, FLAG_CRASH,      6, TMIX_WASH   }, /* 2  Detroit groove   */
{   4, KICK_NONE,  SNR_NONE, SNR_NONE, HHC_NONE, HHO_NONE, RIDE_NONE,  -1,  0, PRE_BELL,  420, 0.82f, 0.90f, 0.55f,  0, FLAG_NONE,       2, TMIX_FILTER }, /* 3  Breakdown        */
{  24, KICK_FOUR,  SNR_NONE, SNR_BACK, HHC_OFF,  HHO_NONE, RIDE_NONE,   1,  1, PRE_PLUCK, 900, 0.90f, 0.45f, 0.40f,  0, FLAG_CRASH,      6, TMIX_FILTER }, /* 4  Acid house       */
{  16, KICK_FOUR,  SNR_NONE, SNR_BACK, HHC_16TH, HHO_OFF,  RIDE_NONE,   1,  1, PRE_PLUCK,1500, 0.94f, 0.40f, 0.50f,  0, FLAG_NONE,       6, TMIX_ECHO   }, /* 5  Acid peak        */
{  16, KICK_2STEP, SNR_2STEP,SNR_NONE, HHC_GARAGE,HHO_OFF, RIDE_NONE,   6,  7, PRE_KEYS,  780, 0.55f, 0.62f, 0.55f, 22, (FLAG_FUNK|FLAG_CRASH), 4, TMIX_STRIP  }, /* 6  UK garage 2-step */
{  16, KICK_FOUR,  SNR_BACK, SNR_NONE, HHC_OFF,  HHO_OFF,  RIDE_NONE,   2,  6, PRE_KEYS,  650, 0.55f, 0.72f, 0.55f,  0, FLAG_NONE,       6, TMIX_WASH   }, /* 7  Organic house    */
{  24, KICK_FOUR,  SNR_NONE, SNR_BACK, HHC_OFF,  HHO_OFF,  RIDE_NONE,   2,  2, PRE_STAB,  800, 0.70f, 0.55f, 0.45f, 18, FLAG_CRASH,      6, TMIX_FILTER }, /* 8  Deep house       */
{  24, KICK_FUNK,  SNR_GHOST,SNR_NONE, HHC_GARAGE,HHO_NONE,RIDE_NONE,   7, -1, PRE_PLUCK, 900, 0.80f, 0.48f, 0.45f, 14, FLAG_FUNK,       6, TMIX_ECHO   }, /* 9  Funky electro    */
{   4, KICK_NONE,  SNR_NONE, SNR_NONE, HHC_NONE, HHO_NONE, RIDE_NONE,  -1,  0, PRE_BELL,  200, 0.60f, 0.95f, 0.45f,  0, FLAG_NONE,       0, TMIX_NONE   }, /* 10 Micro-break      */
{  20, KICK_FOUR,  SNR_NONE, SNR_NONE, HHC_NONE, HHO_OFF,  RIDE_NONE,   3,  3, PRE_MARIMBA,650,0.60f, 0.78f, 0.62f,  0, FLAG_NONE,       6, TMIX_WASH   }, /* 11 Minimal          */
{  28, KICK_FOUR,  SNR_NONE, SNR_NONE, HHC_OFF,  HHO_OFF,  RIDE_8TH,    5,  4, PRE_SUPER, 420, 0.82f, 0.88f, 0.55f,  0, FLAG_CRASH,      8, TMIX_WASH   }, /* 12 Trance melódico  */
{  16, KICK_FOUR,  SNR_NONE, SNR_NONE, HHC_OFF,  HHO_NONE, RIDE_NONE,   0,  2, PRE_STAB,  500, 0.70f, 0.60f, 0.50f,  8, (FLAG_TOMS|FLAG_CRASH), 6, TMIX_STRIP  }, /* 13 Tribal perc      */
{  16, KICK_FOUR,  SNR_BACK, SNR_NONE, HHC_16TH, HHO_NONE, RIDE_NONE,  -1,  4, PRE_LEAD,  420, 0.82f, 0.92f, 0.55f,  0, FLAG_BUILDUP,    0, TMIX_NONE   }, /* 14 Buildup          */
{  32, KICK_FOUR,  SNR_NONE, SNR_BACK, HHC_16TH, HHO_OFF,  RIDE_8TH,    5,  8, PRE_PLUCK,1100, 0.88f, 0.65f, 0.45f,  0, FLAG_CRASH,      4, TMIX_STRIP  }, /* 15 Peak drop        */
{  16, KICK_GALLOP,SNR_BACK, SNR_NONE, HHC_16TH, HHO_NONE, RIDE_NONE,   -1,  3, PRE_PLUCK, 420, 0.82f, 0.60f, 0.22f,  0, FLAG_BUILDUP,    0, TMIX_NONE   }, /* 16 Final buildup    */
{  16, KICK_FOUR,  SNR_NONE, SNR_BACK, HHC_NONE, HHO_NONE, RIDE_8TH,    8,  5, PRE_PLUCK, 1100,0.86f, 0.40f, 0.20f,  0, FLAG_FINALE,    0, TMIX_NONE   }, /* 17 FINAL DROP       */
{  16, KICK_FOUR,  SNR_BACK, SNR_NONE, HHC_NONE, HHO_NONE, RIDE_8TH,    8,  8, PRE_PLUCK, 1300,0.88f, 0.45f, 0.22f,  0, FLAG_FINALE,    8, TMIX_WASH   }, /* 18 APOTEOSIS        */
{   8, KICK_NONE,  SNR_NONE, SNR_NONE, HHC_NONE, HHO_NONE, RIDE_NONE,  -1,  0, PRE_BELL,  420, 0.82f, 0.88f, 0.55f,  0, FLAG_NONE,       0, TMIX_NONE   }, /* 19 Reset            */
};
static constexpr int NUM_SECTIONS = (int)(sizeof(SECTIONS)/sizeof(SECTIONS[0]));

/* ── Nombres y "fórmula" de cada sección (para el monitor serial) ── */
static const char* const SEC_NAME[NUM_SECTIONS] = {
    "DETROIT INTRO", "DETROIT GROOVE", "BREAKDOWN", "ACID HOUSE",
    "ACID PEAK", "UK GARAGE 2-STEP", "ORGANIC HOUSE", "DEEP HOUSE",
    "FUNKY ELECTRO", "MICRO-BREAK", "MINIMAL TECHNO",
    "TRANCE SUPERSAW", "TRIBAL PERC", "BUILDUP", "PEAK DROP",
    "FINAL BUILDUP", "FINAL DROP", "APOTEOSIS", "RESET"
};
static const char* const SEC_FX[NUM_SECTIONS] = {
    "ring-mod bell: y=sin(wc.t)*sin(wm.t), r=1.41",
    "ring-mod bell + 4x4 kick + ride 8th",
    "wash: reverb fb->0.95, all dissolves",
    "acid: ladder LPF, Q=0.90, env->fc",
    "acid peak: fc=1500Hz, Q=0.94 (self-osc)",
    "garage: T_odd=T-22smp shuffle, EP keys",
    "organic: 4x4 EP keys, bp2 octaves, no swing",
    "house stab: additive algo1, swing 18smp",
    "funk: slap bass + rimshot syncopa",
    "break: no kick, fc=200Hz, reverb wash 0.95",
    "minimal: dub delay fb->0.62, sparse",
    "trance: supersaw arp 16th, detune 9, rolling octave bass (pat5)",
    "tribal: toms+perc, stab chords, swing 12smp",
    "riser: snare roll, density f(progress)",
    "peak: anthem riff mel8 + pluck FM short",
    "riser: pluck FM (short), gallop kick, snare roll",
    "DROP: driving bass16 + peak riff, crash/bar, slam",
    "APOTEOSIS: anthem mel8 + bass16, tom fills, locura!!!",
    "reset -> loop back to intro"
};
static const char* const MIX_NAME[5] = {
    "CUT", "FILTER-SWEEP", "ECHO-OUT", "REVERB-WASH", "STRIP-KICK"
};

/* ── LA HISTORIA (fusion emocion + musica) ────────────────────────
 *  Una frase por seccion que cuenta QUE SIENTES y POR QUE: el gesto
 *  musical concreto (motor, filtro, ritmo) que provoca la emocion.
 *  El publico flipa al entender lo que pasa por dentro mientras baila.
 *  ASCII puro para que ningun terminal serie lo rompa.
 * ─────────────────────────────────────────────────────────────────*/
static const char* const SEC_STORY[NUM_SECTIONS] = {
    /* 1  */ "Nace en la oscuridad: campanas ring-mod del 808 flotan sin ritmo, un whoosh de reverb te arrastra dentro",
    /* 2  */ "Primer latido: el 4x4 entra con crash, el ride marca el 8avo y el 303 respira grave. La ciudad se mueve",
    /* 3  */ "Contienes el aliento: cae el kick y solo queda la reverb (fb 0.90) disolviendolo todo",
    /* 4  */ "La chispa acida: el 303 serpentea con slides y el filtro ladder (Q 0.90) muerde. Empieza el viaje",
    /* 5  */ "El acido hierve: el 303 abre a 1500 Hz al borde de la auto-oscilacion (Q 0.94). No puedes parar",
    /* 6  */ "Las caderas mandan: 2-step del 505, shuffle de 22 muestras, teclas calidas del SH101. Groove de calle",
    /* 7  */ "Respira: 4x4 suave, organo wavetable y octavas del SH101. Calor humano, las manos se buscan",
    /* 8  */ "Te sumerges: stabs aditivos, swing 18, claps del 808. Profundo e hipnotico, sin fondo",
    /* 9  */ "Sonries: slap bass con slides y rimshots a contratiempo. El cuerpo juega, el funk manda",
    /* 10 */ "Pausa cosmica: campanas del 808 flotan en reverb 0.95, el sub a 200 Hz se disuelve. Todos se miran... que viene?",
    /* 11 */ "Pulso hipnotico: marimba escasa y dub delay al 0.62 que rebota en el vacio. Minimal",
    /* 12 */ "Manos al cielo: el supersaw FM (detune 9) sube en arpegio sobre reverb gigante. Lagrimas de felicidad",
    /* 13 */ "La tribu: toms y perc del 808, acordes stab, swing 8. Fuego y tierra bajo los pies",
    /* 14 */ "Sube... sube...: el redoble de snare se acelera y la reverb crece. La tension lo invade todo",
    /* 15 */ "EL CLIMAX: el riff anthem (mel8) estalla sobre pluck FM corto. Todos saltan a la vez!",
    /* 16 */ "Ultima subida: kick galopante y snare en redoble. El corazon a mil por hora",
    /* 17 */ "EL DROP golpea SIN AVISO: bajo motor de 16avos + riff de pico, crash en cada compas. Catarsis!",
    /* 18 */ "APOTEOSIS: la melodia anthem vuela sobre el bajo motor con redobles de toms. No se lo creen!!!",
    /* 19 */ "La calma: vuelven las campanas y la reverb larga. Nada sera igual... y vuelve a empezar",
};

/* ── CURVA DE ENERGIA (dinamica narrativa) ────────────────────────
 *  Ganancia master objetivo por seccion. NUNCA supera 1.0 (los picos
 *  ya estan al limite), solo BAJA los momentos calmados para que el
 *  set respire como una novela: intro intimo, subidas que crecen,
 *  drops que golpean por contraste. Asi "emociona" de verdad.
 * ─────────────────────────────────────────────────────────────────*/
static const float SEC_GAIN[NUM_SECTIONS] = {
    0.92f,  /* 1  intro (arranca alto, sin susurros) */
    0.96f,  /* 2  primer groove                      */
    0.94f,  /* 3  breakdown (corte de estilo, full)  */
    0.97f,  /* 4  acid house                         */
    1.00f,  /* 5  acid peak                          */
    0.97f,  /* 6  garage                             */
    0.96f,  /* 7  organic                            */
    0.97f,  /* 8  deep house                         */
    0.98f,  /* 9  funky                              */
    0.94f,  /* 10 micro-break (corte breve, full)    */
    0.96f,  /* 11 minimal                            */
    1.00f,  /* 12 trance, clímax                     */
    1.00f,  /* 13 tribal                             */
    0.98f,  /* 14 buildup                            */
    1.00f,  /* 15 PEAK DROP                          */
    0.98f,  /* 16 final buildup                      */
    1.00f,  /* 17 FINAL DROP                         */
    1.00f,  /* 18 APOTEOSIS                          */
    0.92f,  /* 19 reset (cierre, no bajón)           */
};

/* ═══════════════════════════════════════════════════════════════════
 *  SELECCIÓN DE ENGINE POR SECCIÓN (tabla paralela a SECTIONS)
 *  drum: DK_909/808/505 · bass: BE_303/SH101 · lead: LE_FM/WT
 *  A lo largo del viaje suenan los 7 engines.
 * ═══════════════════════════════════════════════════════════════════ */
struct EngineSel { uint8_t drum, bass, lead; };
static const EngineSel SEC_ENGINE[NUM_SECTIONS] = {
    { DK_808, BE_303,   LE_FM },  /* 1  Detroit intro   */
    { DK_808, BE_303,   LE_FM },  /* 2  Detroit groove  */
    { DK_808, BE_303,   LE_FM },  /* 3  Breakdown       */
    { DK_909, BE_303,   LE_FM },  /* 4  Acid house      */
    { DK_909, BE_303,   LE_FM },  /* 5  Acid peak       */
    { DK_505, BE_SH101, LE_FM },  /* 6  UK garage       */
    { DK_808, BE_SH101, LE_WT },  /* 7  Organic house   ← WT organ */
    { DK_808, BE_SH101, LE_FM },  /* 8  Deep house      */
    { DK_505, BE_303,   LE_FM },  /* 9  Funky electro   */
    { DK_909, BE_303,   LE_FM },  /* 10 Micro-break     */
    { DK_505, BE_SH101, LE_WT },  /* 11 Minimal         ← WT */
    { DK_909, BE_303,   LE_FM },  /* 12 Trance supersaw */
    { DK_808, BE_303,   LE_FM },  /* 13 Tribal (congas) */
    { DK_909, BE_303,   LE_FM },  /* 14 Buildup         */
    { DK_909, BE_303,   LE_FM },  /* 15 Peak drop       */
    { DK_909, BE_303,   LE_FM },  /* 16 Final buildup   */
    { DK_909, BE_303,   LE_FM },  /* 17 Final drop      */
    { DK_909, BE_303,   LE_FM },  /* 18 Apoteosis       */
    { DK_808, BE_303,   LE_FM },  /* 19 Reset           */
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
static volatile float    monInGain     = 1.0f;   /* fade-in drums 0→1  */
static volatile float    monOutGain    = 1.0f;   /* fade-out drums 1→0 */
static volatile uint8_t  monMode       = 0;
static volatile float    monCutoff     = 420.0f;
static volatile float    monRev        = 0.80f;
static volatile float    monDly        = 0.45f;
static volatile int      monStep16     = 0;      /* paso actual 0-15       */
static volatile uint8_t  monFmVoices   = 0;      /* voces del lead activas */
static volatile uint8_t  monDrum       = 0;      /* DrumKit activo         */
static volatile uint8_t  monBass       = 0;      /* BassEng activo         */
static volatile uint8_t  monLead       = 0;      /* LeadEng activo         */
static float monVuPeak = 0.0f;

/* ── Estado de transición ─────────────────────────────────────────
 *  transOut:    0→1 durante los últimos transOutBars de la sección.
 *               Controla el sweep de filtro/echo/wash/strip salientes.
 *  transInProg: 1→0 durante los primeros kTransInBars tras entrar.
 *               Barre el filtro del bajo de "cerrado" a "objetivo".
 * ─────────────────────────────────────────────────────────────────*/
static float transOut    = 0.0f;
static float transInProg = 0.0f;
static constexpr int kTransInBars = 6;   /* bars para abrir el filtro del bajo */

/* ── Crossfade de entrada/salida ──────────────────────────────────
 *  Cada modo TMIX tiene un carácter físico distinto:
 *
 *  FILTER : EQ-out clásico — LPF cierra a 80 Hz (sub desaparece, tensión).
 *           Drums y lead no bajan.  Al entrar, transInProg reabre el filtro.
 *
 *  ECHO   : el lead muere primero (cuadrado de outGain — en ~2 bars).
 *           El delay sube a near-infinity → ecos interminables.
 *           Los drums bajan a la mitad pero siguen sonando.
 *           Entrada: drums emergen del espacio de eco (inGain arranca en 0.35).
 *
 *  WASH   : todo cae lentamente. Los drums también entran en la reverb
 *           (drumWashSend) → se disuelven en la cola, no desaparecen.
 *           Entrada muy suave desde 0.12 — la sección emerge del reverb.
 *
 *  STRIP  : el kick se queda SOLO, lead a 0.  Hats ya desaparecen
 *           en SequencerTick (stripping).  El silencio es solo en
 *           melodía — la percusión sigue.  Entrada: SLAM.
 *
 *  NONE   : corte seco + crash.  SLAM siempre.
 *
 *  REGLA CRÍTICA: nunca silencio total.  Si los drums salen,
 *  el reverb/delay llena el hueco.  Si el bajo se cierra, el kick aguanta.
 * ─────────────────────────────────────────────────────────────────*/
static float outGain      = 1.0f;
static float inGain       = 1.0f;
static float leadGain     = 1.0f;
static float inGainStep   = 1.0f / (4.0f * 16.0f);   /* runtime: bars×16 */
static float leadGainStep = 1.0f / (6.0f * 16.0f);
/* Envío de drums a la reverb durante WASH (0 normalmente) */
static float drumWashSend = 0.0f;

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
    drumsGainEff  = 0.9f;
    outGain       = 1.0f;
    drumWashSend  = 0.0f;

    /* ── ENERGÍA CONSTANTE: el beat NUNCA baja de volumen ────────────
     *  Los drums entran SIEMPRE a tope — el kick es continuo de sección
     *  a sección (4x4 perpetuo).  La transición es solo TIMBRE (filtro)
     *  y COLOR (reverb/delay), jamás volumen.
     *  El único elemento que hace un micro-fade es la MELODÍA, y solo
     *  para que no aparezca con un click — 2 compases, no es un bajón. */
    inGain     = 1.0f;
    inGainStep = 1.0f;

    int prevIdx  = (secIdx + NUM_SECTIONS - 1) % NUM_SECTIONS;
    bool slam = (SECTIONS[prevIdx].flags & FLAG_BUILDUP) || (cur.flags & FLAG_FINALE);

    if(slam){
        /* DROP: todo de golpe, incluida la melodía y el sub lleno */
        leadGain     = 1.0f;
        leadGainStep = 1.0f;
    } else {
        /* La melodía entra en ~2 compases (anti-click, no es bajón) */
        leadGain     = 0.55f;
        leadGainStep = 1.0f / (2.0f * 16.0f);
    }

    /* Bajo: filter-in timbral (abre de cerrado al objetivo).  En el
     *  drop golpea lleno; el resto abre rápido (no es bajada de nivel,
     *  el volumen del bajo es constante — solo cambia el brillo). */
    if(cur.bassPat >= 0){
        if(slam){
            transInProg   = 0.0f;
            bassCutoffEff = cur.bassCutoff;
        } else {
            transInProg   = 1.0f;
            bassCutoffEff = 200.0f;   /* arranca medio-abierto, no muerto */
        }
    }

    /* Seleccionar engines de esta sección. Silenciar el lead saliente
     * para que no queden voces colgando al cambiar de motor. */
    if(curLeadEng == LE_WT) wt.AllNotesOff();
    curDrumKit = SEC_ENGINE[secIdx].drum;
    curBassEng = SEC_ENGINE[secIdx].bass;
    curLeadEng = SEC_ENGINE[secIdx].lead;

    BassSetResonance(cur.bassReso);

    if(cur.flags & FLAG_CRASH)
        DrumTrig(G_CRASH, 0.85f);

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
        DrumTrig(G_KICK, 1.0f);

    /* ── SNARE (roll en buildup) ── */
    if(buildup){
        bool roll = (secProg < 0.5f) ? (step16 % 4 == 0)
                  : (secProg < 0.8f) ? (step16 % 2 == 0) : true;
        if(roll)
            DrumTrig(G_SNARE, 0.35f + 0.65f * secProg);
    } else if(!stripping && Hit(cur.snare, step16)){
        float v = (step16==4 || step16==12) ? 0.9f : 0.45f;
        DrumTrig(G_SNARE, v);
    }

    /* ── CLAP ── */
    if(!stripping && Hit(cur.clap, step16))
        DrumTrig(G_CLAP, 0.85f);

    /* ── HI-HAT cerrado ── */
    if(!stripping && Hit(cur.hhc, step16))
        DrumTrig(G_HHC, (step16%2==0) ? 0.6f : 0.4f);

    /* ── HI-HAT abierto ── */
    if(!stripping && Hit(cur.hho, step16))
        DrumTrig(G_HHO, 0.7f);

    /* ── RIDE con fade-in (gain en la velocity, sin SetVolume) ── */
    if(!stripping && Hit(cur.ride, step16)){
        rideGain += (1.0f - rideGain) * 0.02f;
        DrumTrig(G_RIDE, 0.6f * rideGain);
    }

    /* ── TOMS tribales ── */
    if(cur.flags & FLAG_TOMS){
        if(step16==2 || step16==11) DrumTrig(G_LTOM, 0.7f);
        if(step16==6 || step16==14) DrumTrig(G_MTOM, 0.6f);
        if(step16==3 || step16==9 || step16==13) DrumTrig(G_PERC, 0.5f);
        if(step16==1 || step16==7)  DrumTrig(G_SHK, 0.5f);
    }

    /* ── FUNK: rimshot + perc en síncopas ── */
    if(cur.flags & FLAG_FUNK){
        if(step16==3 || step16==7 || step16==11) DrumTrig(G_RIM, 0.45f);
        if(step16==6 || step16==14)              DrumTrig(G_PERC, 0.4f);
    }

    /* ── FINALE: catarsis — crash/compás + fills de toms ascendentes ──
     *  Slam reforzado en el primer compás de cada fase, redoble de toms
     *  en el último compás que empuja a la siguiente fase / al reset.
     *  Energía máxima manteniendo el espectro limpio (sin solapar voces
     *  de drums en el mismo step). */
    if(cur.flags & FLAG_FINALE){
        bool lastBar = secProg > 0.90f;   /* fill de salida de la fase */
        /* crash de cada compás; el de la 1ª barra de la fase es un SLAM */
        if(step16 == 0)  DrumTrig(G_CRASH, (secBar == 0) ? 0.75f : 0.42f);
        if(step16 == 10 && !lastBar) DrumTrig(G_LTOM, 0.48f);
        if(lastBar){
            /* redoble ascendente low→mid + snare creciente */
            if(step16 == 2)  DrumTrig(G_LTOM, 0.55f);
            if(step16 == 6)  DrumTrig(G_LTOM, 0.62f);
            if(step16 == 9)  DrumTrig(G_MTOM, 0.68f);
            if(step16 == 12) DrumTrig(G_MTOM, 0.75f);
            if(step16 == 14) DrumTrig(G_SNARE, 0.7f);
        }
    }

    /* ── Fill de snare al cerrar la sección (anticipa la transición) ──
     *  En el ÚLTIMO compás de una sección con groove (no buildup/finale,
     *  con snare en el patrón) se acelera un mini-redoble en la 2ª mitad
     *  del compás → empuja hacia el cambio sin romper la energía. */
    if(!stripping && !(cur.flags & (FLAG_BUILDUP | FLAG_FINALE))
       && (int)secBar == (int)cur.bars - 1 && cur.snare != SNR_NONE){
        if(step16 == 8 || step16 == 10 || step16 == 12 || step16 == 14)
            DrumTrig(G_SNARE, 0.35f + 0.04f * (float)step16);
    }

    /* ── Crash de estructura cada 8 compases en secciones largas ──
     *  Las secciones de ≥20 compases (acid, deep, trance, peak drop…)
     *  reciben un crash suave cada 8 barras para marcar el tiempo y
     *  dar sensación de progresión interna sin cambiar de sección. */
    if(step16 == 0 && secBar > 0 && (secBar % 8 == 0) &&
       cur.bars >= 20 && !(cur.flags & (FLAG_BUILDUP | FLAG_FINALE)))
        DrumTrig(G_CRASH, 0.40f);

    /* ── MELODÍA (lead: FM o Wavetable) ── */
    if(cur.melPat >= 0){
        const Melody& m = MEL_BANK[cur.melPat];
        uint8_t hi = m.hi[step16];
        if(hi) LeadNoteOn(hi, ((step16%4)==0) ? 0.9f : 0.6f, cur.fmPreset);
        uint8_t lo = m.lo[step16];
        if(lo) LeadNoteOn(lo, 0.65f, cur.fmPreset);
    }

    /* ── BAJO (303 o SH101) ── */
    if(cur.bassPat >= 0){
        const BassPat& bp = BASS_BANK[cur.bassPat];
        uint8_t note = bp.note[step16];
        if(note)
            BassNoteOn(note, bp.acc[step16] != 0, bp.slide[step16] != 0);
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
     *  TMIX_FILTER: EQ-out: el bass LPF cierra hasta 80 Hz (sub desaparece).
     *               Al entrar la nueva sección, transInProg abre el filtro.
     *               La reverb sube ligeramente para llenar el espacio del sub.
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

    /* ── Fade-in del tema ENTRANTE ──────────────────────────────────
     *  inGain/leadGain suben con pasos calculados en EnterSection
     *  (velocidad y valor inicial dependen del modo de la sección anterior). */
    if(inGain   < 1.0f) inGain   = Clampf(inGain   + inGainStep,   0.0f, 1.0f);
    if(leadGain < 1.0f) leadGain = Clampf(leadGain + leadGainStep, 0.0f, 1.0f);

    /* ── Automatización por modo de transición SALIENTE ─────────────
     *  ENERGÍA CONSTANTE — REGLA DE ORO:
     *    drumsGainEff SIEMPRE 0.9.  El kick/groove NUNCA baja de volumen.
     *    Salimos a muerte y no paramos: 10 min sin un solo bajón.
     *  La transición la hacen el FILTRO del bajo (timbre) y el COLOR
     *  (reverb/delay).  El lead solo cede protagonismo (es melodía, no
     *  energía) y solo cuando aporta — nunca deja un hueco audible.
     * ─────────────────────────────────────────────────────────────── */
    drumsGainEff = 0.9f;     /* fijo: el beat no se toca jamás */
    drumWashSend = 0.0f;

    if(transOut > 0.0f){
        switch(cur.transMode){

            case TMIX_FILTER:
                /* EQ-out clásico de DJ: el LPF cierra (sub desaparece).
                 * Crea tensión quitando el bajo → el drop reventará más fuerte.
                 * La reverb sube un poco para llenar el espacio que deja el sub. */
                bassCutoffEff += (Lerp(cur.bassCutoff, 80.0f, transOut) - bassCutoffEff) * 0.04f;
                tRevFb = cur.revFb + (0.82f - cur.revFb) * transOut * 0.3f;
                break;

            case TMIX_ECHO:
                /* Delay throw: ecos que crecen sobre el groove intacto.
                 * El lead se va a ecos pero la batería sigue clavada. */
                tDlyFb = cur.dlyFb + (0.85f - cur.dlyFb) * transOut;
                tRevFb = cur.revFb + (0.86f - cur.revFb) * transOut * 0.5f;
                break;

            case TMIX_WASH:
                /* Reverb crece → cola que envuelve, sin tocar el volumen.
                 * Los drums se ENVÍAN a la reverb (se ensanchan, no bajan). */
                tRevFb = cur.revFb + (0.93f - cur.revFb) * transOut;
                tDlyFb = cur.dlyFb + (cur.dlyFb * 1.3f - cur.dlyFb) * transOut * 0.4f;
                drumWashSend = transOut * 0.22f;
                break;

            case TMIX_STRIP:
                /* Tensión rítmica: hats fuera (SequencerTick), bass se
                 * abre en brillo y la reverb sube.  El kick MANDA, a tope. */
                bassCutoffEff += (Lerp(cur.bassCutoff, cur.bassCutoff * 1.5f,
                                       transOut * 0.6f) - bassCutoffEff) * 0.025f;
                tRevFb = cur.revFb + (0.88f - cur.revFb) * transOut * 0.5f;
                break;

            default:
                break;
        }
    } else if(transInProg > 0.0f){
        /* Filter-in del bajo entrante (timbral, el volumen ya es pleno) */
        float openCutoff = Lerp(200.0f, cur.bassCutoff, 1.0f - transInProg);
        bassCutoffEff += (openCutoff - bassCutoffEff) * 0.05f;
    } else {
        /* Estable */
        bassCutoffEff += (cur.bassCutoff - bassCutoffEff) * 0.05f;
    }
    outGain = 1.0f;   /* el saliente nunca baja de volumen */

    /* Aplicar cutoff al bajo activo (por bloque es suficiente) */
    BassSetCutoff(bassCutoffEff);

    /* Lerp de reverb/delay */
    revFb += (tRevFb - revFb) * 0.015f;
    dlyFb += (tDlyFb - dlyFb) * 0.015f;
    reverb.SetFeedback(Clampf(revFb, 0.0f, 0.99f));

    /* Ganancia máster — curva de energía narrativa por sección.
     * El set respira: intro íntimo → drops que golpean por contraste.
     * Nunca supera 1.0, así que no añade clipping. Lerp lento (~1 compás)
     * para que la subida/bajada de nivel sea musical, no un escalón. */
    float gainTgt = SEC_GAIN[secIdx];
    masterGain += (gainTgt - masterGain) * 0.0008f;

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

        /* ── Mezcla: ENERGÍA CONSTANTE ──────────────────────────────
         * drumsGainEff fijo a 0.9 (el beat no baja jamás).  inGain=1.0
         * siempre (drums entran a tope).  El único micro-fade es leadGain
         * (2 compases, anti-click de la melodía).  La transición vive en
         * el FILTRO del bajo y el COLOR (reverb/delay), no en el volumen. */
        float dGain = drumsGainEff
                    * ((cur.flags & FLAG_FINALE) ? 0.70f : 1.0f)
                    * inGain;
        float drumMix = DrumProcess() * dGain;

        /* Delay ping-pong */
        size_t rpL = (dlyWp + DLY_SIZE - dlyTimeL) % DLY_SIZE;
        size_t rpR = (dlyWp + DLY_SIZE - dlyTimeR) % DLY_SIZE;
        float dL   = dlyBufL[rpL];
        float dR   = dlyBufR[rpR];

        /* Lead: outGain controla salida saliente; leadGain la entrada */
        float lFade  = (leadGain < 1.0f) ? leadGain : outGain;
        float fmMix  = LeadProcess() * 0.30f * lFade;

        float bassMix = BassProcess() * 0.85f;

        /* Alimentar delay (cross-feed L↔R) */
        dlyBufL[dlyWp] = drumMix * 0.14f + fmMix * 0.18f + dR * dlyFb;
        dlyBufR[dlyWp] = drumMix * 0.14f + fmMix * 0.18f + dL * dlyFb;
        dlyWp = (dlyWp + 1) % DLY_SIZE;

        float dryL = drumMix * 0.85f + fmMix * 0.80f + bassMix + dL * 0.40f;
        float dryR = drumMix * 0.85f + fmMix * 0.80f + bassMix + dR * 0.40f;

        /* Reverb:
         * - Bypass en FLAG_FINALE excepto si drumWashSend>0 (WASH activo
         *   en la salida de APOTEOSIS: los drums se disuelven en la cola). */
        float wetL = 0.0f, wetR = 0.0f;
        bool doReverb = !(cur.flags & FLAG_FINALE) || (drumWashSend > 0.001f);
        if(doReverb)
            reverb.Process(fmMix * 0.80f + dL * 0.35f + drumMix * drumWashSend,
                           fmMix * 0.80f + dR * 0.35f + drumMix * drumWashSend,
                           &wetL, &wetR);

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
    monInGain   = inGain;
    monOutGain  = outGain;
    monMode     = cur.transMode;
    monCutoff   = bassCutoffEff;
    monRev      = revFb;
    monDly      = dlyFb;
    monStep16   = step16;
    /* contar voces del lead activo */
    uint8_t lvAct = 0;
    if(curLeadEng == LE_FM)
        for(int v = 0; v < NUM_FM; v++) if(fmv[v].IsActive()) lvAct++;
    monFmVoices = lvAct;
    monDrum     = curDrumKit;
    monBass     = curBassEng;
    monLead     = curLeadEng;

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
    if(n < 0) n = 0;
    if(n > width) n = width;
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
    if(n < 0) n = 0;
    if(n > width) n = width;
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
    if(n < 0) n = 0;
    if(n > width) n = width;
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
    /* ── La historia: una novela que se baila ── */
    PL("  %s\"%s\"%s", C(A_BYEL), SEC_STORY[idx], C(A_RST));
    PL("  %senergia%s: %s%d%%%s", C(A_DIM), C(A_RST),
       C(A_BWHT), (int)(SEC_GAIN[idx] * 100.0f + 0.5f), C(A_RST));
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
    /* ── Rack de engines activos en esta sección ── */
    const EngineSel& e = SEC_ENGINE[idx];
    PL("  %sENGINES%s drum=%s%s%s  bass=%s%s%s  lead=%s%s%s",
       C(A_BWHT), C(A_RST),
       C(A_BCYN), DK_NAME[e.drum], C(A_RST),
       (s.bassPat >= 0) ? C(A_BGRN) : C(A_DIM),
       (s.bassPat >= 0) ? BE_NAME[e.bass] : "--", C(A_RST),
       (s.melPat  >= 0) ? C(A_BMAG) : C(A_DIM),
       (s.melPat  >= 0) ? LE_NAME[e.lead] : "--", C(A_RST));
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

    float ig = monInGain;
    float og = monOutGain;
    if(tout > 0.001f || og < 0.99f){
        /* Fade-out activo: mostrar barras de crossfade */
        char tbar[14], obar[14];
        RenderBar(tout, 8, '>', tbar);
        RenderBar(1.0f - og, 8, '-', obar);
        PL(" %sB%d%s s%02d [%s%s%s] VU[%s] [%s%s%s] %s>>%s%s OUT[%s%s%s] fc=%d",
           beatCol, beat, C(A_RST), stp,
           col, stepbuf, C(A_RST), vubar,
           col, progbuf, C(A_RST),
           C(A_BRED), MIX_NAME[monMode], C(A_RST),
           C(A_BRED), obar, C(A_RST),
           (int)monCutoff);
    } else if(ig < 0.99f){
        /* Fade-in activo: mostrar progreso de entrada */
        char ibar[14];
        RenderBar(ig, 8, '+', ibar);
        PL(" %sB%d%s s%02d [%s%s%s] VU[%s] [%s%s%s] %sIN[%s%s]%s fc=%d v:%s%d%s",
           beatCol, beat, C(A_RST), stp,
           col, stepbuf, C(A_RST), vubar,
           col, progbuf, C(A_RST),
           C(A_BGRN), ibar, C(A_RST),
           C(A_RST), (int)monCutoff, fmCol, fmv_n, C(A_RST));
    } else {
        PL(" %sB%d%s s%02d [%s%s%s] VU[%s] [%s%s%s] %s%s/%s/%s%s fc=%d v:%s%d%s",
           beatCol, beat, C(A_RST), stp,
           col, stepbuf, C(A_RST), vubar,
           col, progbuf, C(A_RST),
           C(A_DIM), DK_NAME[monDrum], BE_NAME[monBass], LE_NAME[monLead], C(A_RST),
           (int)monCutoff, fmCol, fmv_n, C(A_RST));
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

    /* ── TR909 (kit principal, afinado al detalle) ── */
    drums909.Init(SAMPLE_RATE);
    drums909.kick.SetDrive(1.0f);
    drums909.kick.SetDecay(0.55f);
    drums909.kick.SetCompression(1.0f);
    drums909.hihatO.SetDecay(1.6f);
    drums909.SetVolume(TR909::INST_KICK,    1.3f);
    drums909.SetVolume(TR909::INST_SNARE,   0.7f);
    drums909.SetVolume(TR909::INST_HIHAT_C, 0.5f);
    drums909.SetVolume(TR909::INST_HIHAT_O, 0.6f);
    drums909.SetVolume(TR909::INST_CLAP,    0.5f);
    drums909.SetVolume(TR909::INST_RIDE,    0.5f);
    drums909.SetVolume(TR909::INST_LOW_TOM, 0.7f);
    drums909.SetVolume(TR909::INST_MID_TOM, 0.7f);
    drums909.SetVolume(TR909::INST_HI_PERC, 0.5f);
    drums909.SetVolume(TR909::INST_SHAKER,  0.5f);
    drums909.SetMasterVolume(0.9f);

    /* ── TR808 (graves redondos, kick boom) ── */
    drums808.Init(SAMPLE_RATE);
    drums808.SetVolume(TR808::INST_KICK, 1.3f);
    drums808.SetMasterVolume(0.9f);

    /* ── TR505 (lo-fi, percusivo) ── */
    drums505.Init(SAMPLE_RATE);
    drums505.SetVolume(TR505::INST_KICK, 1.2f);
    drums505.SetMasterVolume(0.9f);

    for(int v = 0; v < NUM_FM; v++){
        fmv[v].Init(SAMPLE_RATE);
        ApplyPreset(fmv[v], PRE_BELL);
    }

    /* ── Wavetable lead ── */
    wt.Init(SAMPLE_RATE);
    wt.SetFilter(6000.0f, 0.7f);

    /* ── TB303 (bajo ácido principal) ── */
    bass303.Init(SAMPLE_RATE);
    bass303.SetWaveform(TB303::WAVE_SAW);
    bass303.SetCutoff(420.0f);
    bass303.SetResonance(0.82f);
    bass303.SetEnvMod(0.8f);
    bass303.SetDecay(0.12f);
    bass303.SetAccent(0.85f);
    bass303.SetOverdrive(0.5f);
    bass303.SetVolume(0.8f);

    /* ── SH101 (bajo alternativo, más redondo) ── */
    bassSH.Init(SAMPLE_RATE);
    bassSH.params.waveform  = 0;     /* saw */
    bassSH.params.subLevel  = 0.4f;
    bassSH.params.cutoff    = 600.0f;
    bassSH.params.resonance = 0.4f;
    bassSH.params.vcaDecay  = 0.4f;
    bassSH.params.volume    = 0.8f;

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
        PL("%s%s#%s   %s19 secciones / ~9.9 min / 132 BPM%s             %s%s#%s",
           C(A_BOLD), C(A_BRED), C(A_RST), C(A_BCYN), C(A_RST), C(A_BOLD), C(A_BRED), C(A_RST));
        PL("%s%s#%s   %s7 engines: 909 808 505 303 SH101 FM WT%s        %s%s#%s",
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
