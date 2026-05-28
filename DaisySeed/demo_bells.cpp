/* ═══════════════════════════════════════════════════════════════════
 *  DEMO BELLS — Daisy Seed standalone showcase
 * ─────────────────────────────────────────────────────────────────
 *  Demo autónoma (SIN ESP32) que muestra el potencial del Daisy Seed:
 *    · Síntesis FM 2-operador (FM2Op) → campanas / glockenspiel
 *    · Polifonía de 12 voces
 *    · Reverb estéreo (ReverbSc de DaisySP) con cola larga
 *    · Secuencia musical automática (arpegios + melodía)
 *
 *  Suena solo al arrancar. El LED late con la música.
 *
 *  Build:   build_daisy.ps1 -DemoBells   (o make DEMO_BELLS=1)
 *           → build/DemoBells.bin
 *  Flash:   flash_bells.ps1              (DFU, QSPI @ 0x90040000)
 *           NO necesita samples.bin (síntesis pura).
 * ═══════════════════════════════════════════════════════════════════ */

#include "daisy_seed.h"
#define USE_DAISYSP_LGPL
#include "daisysp.h"
#include "synth/fm2op.h"
#include <math.h>

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

static constexpr float  SAMPLE_RATE = 48000.0f;
static constexpr size_t AUDIO_BLOCK = 48;

/* ── Reverb estéreo (en SDRAM, igual que el firmware principal) ── */
DSY_SDRAM_BSS static ReverbSc reverb;

/* ═══════════════════════════════════════════════════════════════════
 *  POLIFONÍA — 12 voces FM2Op con asignación round-robin
 * ═══════════════════════════════════════════════════════════════════ */
static constexpr int NUM_VOICES = 12;
static FM2Op::Synth voices[NUM_VOICES];
static uint8_t      voiceNext = 0;

/* Preset de campana — aplicado a una voz */
static void ApplyBellPreset(FM2Op::Synth& v)
{
    v.params.algo     = 0;        /* FM puro M→C                       */
    v.params.ratio    = 3.5f;     /* ratio inarmónico → timbre metálico */
    v.params.index    = 6.0f;     /* índice alto → brillo de campana    */
    v.params.feedback = 0.10f;
    v.params.velSens  = 0.6f;

    /* Carrier: ataque instantáneo, decay largo (cola de campana) */
    v.params.cAtk = 0.001f;
    v.params.cDec = 3.2f;
    v.params.cSus = 0.0f;
    v.params.cRel = 1.5f;

    /* Modulator: decae más rápido que el carrier → el brillo se apaga
     * antes que el tono fundamental (comportamiento real de campana). */
    v.params.mAtk = 0.001f;
    v.params.mDec = 0.9f;
    v.params.mSus = 0.0f;
    v.params.mRel = 0.4f;

    v.params.volume = 0.7f;
}

static void NoteOnPoly(uint8_t midiNote, float vel)
{
    FM2Op::Synth& v = voices[voiceNext];
    ApplyBellPreset(v);          /* re-aplica preset (robo de voz limpio) */
    v.NoteOn(midiNote, vel);
    voiceNext = (uint8_t)((voiceNext + 1) % NUM_VOICES);
}

/* ═══════════════════════════════════════════════════════════════════
 *  SECUENCIADOR — melodía automática de campanas
 * ═══════════════════════════════════════════════════════════════════ */

/* Escala pentatónica mayor (suena "bonita" en cualquier orden) sobre C.
 * Notas MIDI: C5 D5 E5 G5 A5 C6 ...                                    */
static const uint8_t kScale[] = {
    72, 74, 76, 79, 81, 84, 86, 88, 91, 93, 96
};
static constexpr int kScaleLen = (int)(sizeof(kScale) / sizeof(kScale[0]));

/* Patrón melódico (índices en la escala). -1 = silencio.
 * 32 pasos → frase que se repite y va subiendo/bajando. */
static const int8_t kPattern[] = {
    0, 2, 4, 2,  4, 6, 4, 2,
    1, 3, 5, 3,  5, 7, 5, 3,
    6, 8, 10, 8, 6, 4, 2, 0,
    4, 2, 0, 2,  4, 6, 8, -1
};
static constexpr int kPatternLen = (int)(sizeof(kPattern) / sizeof(kPattern[0]));

static constexpr float BPM       = 96.0f;
/* Semicorcheas (4 por beat). samples por paso = SR * 60 / BPM / 4 */
static const uint32_t kStepSamples =
    (uint32_t)(SAMPLE_RATE * 60.0f / BPM / 4.0f);

static uint32_t stepCounter = 0;
static int      stepIndex   = 0;
static uint32_t barCounter  = 0;   /* nº de frases completadas */

static float ledLevel = 0.0f;      /* envelope visual para el LED */

static void SequencerTick()
{
    int8_t sel = kPattern[stepIndex];
    if(sel >= 0 && sel < kScaleLen)
    {
        uint8_t note = kScale[sel];

        /* Cada 2 frases, transponer una octava arriba para variar */
        if((barCounter & 1u) && note <= 108) note += 12;

        /* Velocidad con leve acento en los tiempos fuertes */
        float vel = ((stepIndex & 3) == 0) ? 0.95f : 0.65f;
        NoteOnPoly(note, vel);

        /* Acorde ocasional: añadir la quinta en el primer paso de cada frase */
        if(stepIndex == 0 && sel + 2 < kScaleLen)
            NoteOnPoly(kScale[sel + 2], vel * 0.7f);

        ledLevel = vel;            /* destello del LED en cada nota */
    }

    stepIndex++;
    if(stepIndex >= kPatternLen)
    {
        stepIndex = 0;
        barCounter++;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  AUDIO CALLBACK
 * ═══════════════════════════════════════════════════════════════════ */
void AudioCallback(AudioHandle::InputBuffer  /*in*/,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        /* ── Avance del secuenciador (sample-accurate) ── */
        if(stepCounter == 0) SequencerTick();
        stepCounter++;
        if(stepCounter >= kStepSamples) stepCounter = 0;

        /* ── Sumar todas las voces ── */
        float dry = 0.0f;
        for(int v = 0; v < NUM_VOICES; v++)
            dry += voices[v].Process();

        dry *= 0.22f;              /* headroom para 12 voces             */

        /* ── Reverb estéreo ── */
        float wetL, wetR;
        reverb.Process(dry, dry, &wetL, &wetR);

        /* Mezcla dry/wet: campanas necesitan bastante reverb */
        float outL = dry * 0.55f + wetL * 0.65f;
        float outR = dry * 0.55f + wetR * 0.65f;

        /* Soft clip de seguridad */
        outL = tanhf(outL);
        outR = tanhf(outR);

        out[0][i] = outL;
        out[1][i] = outR;
    }

    /* ── LED: decaimiento suave del destello (1 update por bloque) ── */
    ledLevel *= 0.90f;
    hw.SetLed(ledLevel > 0.08f);
}

/* ═══════════════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════════════ */
int main()
{
    /* FPU Flush-to-Zero + Default-NaN (evita denormales en la cola de reverb) */
    __asm volatile("VMRS r0, FPSCR\n"
                   "ORR  r0, r0, #(1<<24)|(1<<25)\n"
                   "VMSR FPSCR, r0" ::: "r0");
    *(volatile uint32_t*)0xE000EF3Cu |= (1u << 24) | (1u << 25);

    hw.Init();
    hw.SetAudioBlockSize(AUDIO_BLOCK);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    /* ── Init voces ── */
    for(int v = 0; v < NUM_VOICES; v++)
    {
        voices[v].Init(SAMPLE_RATE);
        ApplyBellPreset(voices[v]);
    }

    /* ── Init reverb (cola larga, brillo controlado) ── */
    reverb.Init(SAMPLE_RATE);
    reverb.SetFeedback(0.85f);     /* cola larga, "catedral"            */
    reverb.SetLpFreq(9000.0f);     /* amortigua agudos del reverb       */

    /* Pequeño retardo antes de empezar para evitar el "pop" de arranque */
    hw.StartAudio(AudioCallback);

    /* El secuenciador corre dentro del AudioCallback; el main loop solo
     * mantiene el sistema vivo. */
    while(1)
    {
        System::Delay(100);
    }
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
