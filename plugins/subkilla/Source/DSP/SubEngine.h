// TrapWin SUB KILLA - 808 / sub-bass synth engine (plain C++, no JUCE).
//
// Signal flow per sample:
//   voices (sine body + pitch "punch" envelope + sub-octave + click, amp ADSR, glide)
//   -> 2x oversampled saturation (Tube / Tape / Hard / Fold, drive, post-gain compensation, dry/wet)
//   -> resonant low-pass (opened by the amp envelope)  -> DC block
//   -> stereo width (side signal derived only from the band above ~200 Hz, sub stays mono)
//   -> output gain -> optional soft limiter (ceiling -0.3 dBFS)
//
// Real-time safe: all memory is fixed-size members, nothing is allocated in process().
#pragma once

#include <array>
#include <cstdint>

namespace subkilla
{

enum class VoiceMode : int { mono = 0, legato, poly };
enum class SatType : int { tube = 0, tape, hard, fold };

constexpr int maxVoices = 4;
constexpr float bendRangeSemis = 12.0f;
constexpr float limiterCeiling = 0.9661f;          // -0.3 dBFS

constexpr const char* voiceModeNames[] = { "Mono", "Legato", "Poly" };
constexpr const char* satTypeNames[] = { "Tube", "Tape", "Hard", "Fold" };

struct Params
{
    VoiceMode mode = VoiceMode::legato;
    float glideMs = 90.0f;        // 0 .. 2000 (constant-time portamento)
    float tuneSemis = 0.0f;       // -24 .. +24
    float fineCents = 0.0f;       // -100 .. +100
    float punchSemis = 14.0f;     // pitch envelope depth, 0 .. 48 st
    float punchMs = 45.0f;        // pitch envelope time (to 1 %), 5 .. 400 ms
    float click = 0.2f;           // transient layer, 0 .. 1
    float sub = 0.0f;             // sub-octave blend, 0 .. 1
    float drive = 0.2f;           // 0 .. 1
    SatType sat = SatType::tube;
    float distMix = 1.0f;         // 0 .. 1
    float cutoffHz = 20000.0f;    // 30 .. 20000
    float reso = 0.1f;            // 0 .. 1
    float fltEnv = 0.0f;          // 0 .. 1 (how far the amp envelope opens the filter)
    float attackMs = 1.0f;        // 0 .. 200
    float decayMs = 2500.0f;      // 30 .. 12000 (time to -60 dB)
    float sustain = 0.0f;         // 0 .. 1
    float releaseMs = 250.0f;     // 5 .. 5000 (time to -60 dB)
    float width = 0.0f;           // 0 .. 1
    float outputDb = 0.0f;        // -24 .. +12
    bool limiter = true;
};

struct Event
{
    enum Type : int { noteOn, noteOff, pitchBend, allNotesOff, sustainPedal };
    int offset = 0;     // sample position inside the block
    Type type = noteOn;
    int note = 0;
    float value = 0.0f; // velocity 0..1, bend -1..1, pedal 0/1
};

// streaming halfband FIR for 2x up / down sampling
class Halfband
{
public:
    static constexpr int taps = 31;
    void init();
    void reset();
    void up (float x, float& y0, float& y1);
    float down (float x0, float x1);

private:
    float push (float x, std::array<float, taps * 2>& buf, int& pos) const;
    std::array<float, taps> h {};
    std::array<float, taps * 2> upBuf {}, downBuf {};
    int upPos = 0, downPos = 0;
};

class Engine
{
public:
    Engine();
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setParams (const Params& p) { params = p; }
    const Params& getParams() const { return params; }

    // Renders n samples (adds nothing: overwrites L/R). Events must be sorted by offset.
    void process (float* left, float* right, int n, const Event* events, int numEvents);

    // ---- UI / test probes (written by the audio thread once per block)
    float getCurrentHz() const { return uiHz; }          // pitch of the most recent voice (0 = silent)
    float getCurrentNote() const { return uiNote; }      // fractional MIDI note incl. glide
    bool isGliding() const { return uiGliding; }
    float getAmpEnv() const { return uiEnv; }            // most recent voice amp env
    float getGainReductionDb() const { return uiGrDb; }
    int getActiveVoices() const;
    int getRetriggerCount() const { return retriggers; }

    static float saturate (SatType t, float x);
    float compensation (SatType t, float drive) const;

private:
    struct Voice
    {
        enum Stage { idle, attack, decay, release };
        Stage stage = idle;
        int note = -1;
        float vel = 1.0f;
        double phase = 0.0, subPhase = 0.0;
        float pitch = 0.0f, target = 0.0f, glideStep = 0.0f;
        float pitchEnv = 0.0f;
        float env = 0.0f, attackStep = 0.0f;
        float clickEnv = 0.0f;
        double clickPhase = 0.0;
        bool gate = false, sustained = false;
        std::uint64_t age = 0;
    };

    void handle (const Event& e);
    void noteOn (int note, float vel);
    void noteOff (int note);
    void allOff();
    void trigger (Voice& v, bool fromSilence);
    void slideTo (Voice& v, int note);
    void releaseVoice (Voice& v);
    void render (float* L, float* R, int start, int end);
    void updateCoefficients();

    Params params;
    double sr = 48000.0;
    std::array<Voice, maxVoices> voices {};
    std::uint64_t ageCounter = 0;
    int retriggers = 0;

    // mono / legato note stack (most recent last)
    std::array<int, 32> stack {};
    int stackSize = 0;
    bool pedal = false;

    // derived coefficients (per block)
    float punchCoef = 0.99f, decayCoef = 0.999f, releaseCoef = 0.999f, clickCoef = 0.99f;
    float smoothCoef = 0.001f;

    // smoothed parameters
    float sDriveGain = 1.0f, sWet = 0.0f, sComp = 1.0f, sCutoff = 20000.0f, sReso = 0.1f, sWidth = 0.0f,
          sOut = 1.0f, sSub = 0.0f, sClick = 0.2f, sTune = 0.0f, sBend = 0.0f, bendTarget = 0.0f, sFltEnv = 0.0f;

    // processing state
    Halfband os;
    float tapeLp = 0.0f, tapeCoef = 0.5f;
    float svfIc1 = 0.0f, svfIc2 = 0.0f;
    float dcX = 0.0f, dcY = 0.0f, dcCoef = 0.999f;
    float hp1[2] {}, hp2[2] {};        // 200 Hz high-pass (two TPT one-pole stages x2)
    float hpG = 0.0f;
    float ap[4] {}; float apC[4] {};    // decorrelating allpasses for the side signal
    float limGain = 1.0f, limRelease = 0.999f;
    std::uint32_t noise = 22222u;

    // gain compensation table [type][drive step]
    static constexpr int compSteps = 33;
    std::array<std::array<float, compSteps>, 4> compTable {};

    // UI probes
    float uiHz = 0.0f, uiNote = 0.0f, uiEnv = 0.0f, uiGrDb = 0.0f;
    bool uiGliding = false;
};

float driveGainFor (SatType t, float drive);   // linear pre-gain for a drive amount
const char* noteName (int midiNote);           // "C1", "F#2" ... (60 = C4)

} // namespace subkilla
