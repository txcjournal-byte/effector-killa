// TrapWin AUTO KILLA - real-time vocal pitch correction engine.
//
// Signal flow
//  * a YIN pitch tracker runs causally on a decimated (~24 kHz) mono sum,
//  * the detected pitch is snapped to the allowed notes (key + scale, custom notes, auto-detected key
//    or held MIDI notes) with hysteresis, then glided by RETUNE, softened by HUMANIZE / THRESHOLD,
//    and VIBRATO + TRANSPOSE are added -> one pitch ratio per analysis mark,
//  * pitch-synchronous overlap-add (TD-PSOLA): analysis marks follow the glottal pulses on the input,
//    synthesis marks are spaced (period / ratio) on the output and every synthesis mark copies the
//    nearest analysis grain. Grains keep their own waveform, so the formants stay put; THROAT resamples
//    each grain to move the formants on purpose.
//
// Two modes with a fixed, reported latency each:
//  * LIVE (tracking): grains are capped at 6 ms each side -> ~16.5 ms total latency, pitch floor 70 Hz.
//  * MIX: full two-period grains down to 60 Hz -> ~42 ms, smoother on low voices and big shifts.
//
// Plain C++17, no allocation / locks in process(). Wrapped by JUCE in PluginProcessor.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace akill
{

enum class Mode { live = 0, mix = 1 };

constexpr int numScales = 9;
constexpr int scaleChromatic = 7;
constexpr int scaleCustom = 8;
constexpr std::array<const char*, numScales> scaleNames { "Major", "Minor", "Harmonic Minor", "Major Pentatonic",
                                                          "Minor Pentatonic", "Phrygian", "Dorian", "Chromatic", "Custom" };
constexpr std::array<const char*, 12> keyNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

struct Params
{
    Mode mode = Mode::mix;
    int key = 9;                   // 0 = C
    int scale = 1;                 // index into scaleNames
    int customMask = 0xFFF;        // bit n = pitch class n (absolute, bit 0 = C) for scale == Custom
    float retuneMs = 20.0f;        // 0 = instant (robotic hard tune) .. 400 ms (very natural)
    float humanize = 0.0f;         // 0..1: keeps the singer's own vibrato / drift around the corrected note
    float thresholdCents = 100.0f; // notes further than this from a target are left alone; >= 100 = correct all
    float vibrato = 0.0f;          // 0..1 synthetic vibrato depth (1 = +-50 cents)
    float vibratoRateHz = 5.5f;
    float transpose = 0.0f;        // semitones, -12..+12
    float throatSemis = 0.0f;      // formant shift, -12..+12 semitones
    bool preserveFormants = true;  // off: formants follow the pitch shift (resampled character)
    bool midiTarget = false;       // held MIDI notes become the allowed notes
    bool autoKey = false;          // use the key detected from the last ~10 s of singing
    float mix = 1.0f;
    float outputDb = 0.0f;
};

// ---- helpers (public for tests / UI)
float hzToMidi (float hz);
float midiToHz (float midi);
int scaleMask (int scale, int key, int customMask);          // 12-bit mask of allowed absolute pitch classes
int snapToMask (float midi, int mask, int previous, float hysteresis); // nearest allowed note (-1 if mask empty)

// Key detector: Krumhansl-Kessler profile correlation on a pitch-class histogram.
struct KeyGuess { int key = -1; bool minor = false; float score = 0.0f; };
KeyGuess detectKey (const float* histogram12);

class PitchTracker
{
public:
    void prepare (int maxTau);
    // x has at least window + tauMax + 2 samples. Returns period in samples (0 = none) and the aperiodicity
    // (cumulative mean normalised difference at the chosen lag, 0 = perfectly periodic).
    float analyse (const float* x, int window, int tauMin, int tauMax, float& aperiodicity);

private:
    std::vector<float> d, dn;
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const Params& p);
    // held MIDI notes as a pitch-class mask (0 = none held)
    void setMidiMask (int mask) { midiMask = mask & 0xFFF; }

    void process (float* const* channels, int numChannels, int numSamples);

    int getLatencySamples() const { return latencyTotal; }
    static int latencyFor (Mode m, double sampleRate);
    Mode getActiveMode() const { return active; }

    // ---- UI / metering (written on the audio thread, read anywhere)
    struct Frame { float detected, corrected; int target; };
    static constexpr int frameCapacity = 1024;
    static constexpr double frameSeconds = 0.010;    // one frame every 10 ms of audio
    // copies the newest n frames (oldest first); returns the number copied
    int readFrames (Frame* dest, int n) const;

    std::atomic<float> uiDetectedMidi { 0.0f };       // 0 = unvoiced
    std::atomic<float> uiCorrectedMidi { 0.0f };
    std::atomic<int> uiTargetNote { -1 };
    std::atomic<int> uiDetectedKey { -1 };            // auto key result (-1 = not enough singing yet)
    std::atomic<bool> uiDetectedMinor { false };
    std::atomic<float> uiKeyProgress { 0.0f };        // 0..1 amount of pitch history collected
    std::atomic<float> uiCorrectionCents { 0.0f };

    // effective allowed-note mask in use (for the UI keyboard)
    std::atomic<int> uiMask { 0xFFF };

private:
    struct ModeConfig
    {
        int capOut = 0, capIn = 0, sel = 0, lBase = 0, total = 0;
        int tauMax = 0, tauMin = 0, window = 0, hop = 0;
        int unvoiced = 0;     // grain spacing when nothing is voiced
        int layLag = 0;       // marks are laid this far behind the newest sample
        int minPeriod = 0, maxPeriod = 0;
    };
    ModeConfig configFor (Mode m) const;
    void applyMode (Mode m);
    void analysisStep();
    void updateCorrection (float f0Hz, bool isVoiced);
    void layMarks();
    void addGrain (double s);
    float readIn (int ch, double pos) const;
    void pushFrame();
    void pushHist (bool v);

    double sr = 48000.0;
    Params params;
    int midiMask = 0;
    Mode active = Mode::mix, pending = Mode::mix;
    ModeConfig cfg;
    int latencyTotal = 0;

    // rings (absolute indices & mask)
    static constexpr int maxChannels = 2;
    int ringSize = 1 << 16, ringMask = (1 << 16) - 1;
    std::array<std::vector<float>, maxChannels> in, acc;
    std::vector<float> wacc, epochSig;
    long long writePos = 0;
    int numCh = 2;

    // tracker
    PitchTracker yin;
    int decim = 2;
    double arate = 24000.0;
    std::vector<float> dbuf, ywin;
    int dmask = 0;
    long long dwrite = 0;
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float run (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void flush() { if (z1 * z1 < 1e-30f) z1 = 0; if (z2 * z2 < 1e-30f) z2 = 0; }
    };
    Biquad aa1, aa2;      // anti-alias lowpass before decimation
    float ep1 = 0, ep2 = 0, epc = 0.1f;
    int hopCounter = 0;
    bool voiced = false;
    float periodSamples = 300.0f;
    float lastF0 = 0.0f;
    int octaveHolds = 0;

    // correction state
    float slowMidi = 0.0f;
    float correction = 0.0f;      // smoothed, semitones
    int targetNote = -1;
    float vibPhase = 0.0f, vibEnv = 0.0f;
    float ratioNow = 1.0f;        // ratio applied to newly laid marks
    float corrNow = 0.0f;         // correction + vibrato for the UI
    float detNow = 0.0f;
    int maskNow = 0xFFF;

    // auto key
    static constexpr int keyBins = 500;   // 10 s of 20 ms bins
    std::array<int8_t, keyBins> keyRing {};
    int keyRingPos = 0, keyRingFill = 0;
    std::array<float, 12> keyHist {}, binVotes {};
    int binHops = 0, binsSinceDetect = 0, voicedBins = 0;
    double binTime = 0.0;
    float vibTime = 0.0f;
    KeyGuess keyGuess;

    // analysis history: one entry per hop, stamped with the input time the analysis window is centred on
    struct Hist { double t; float period; float ratio; bool voiced; };
    static constexpr int histCap = 512;
    std::array<Hist, histCap> hist {};
    long long histHead = 0;
    double pendingCentre = 0.0;
    Hist lookup (double pos) const;

    // marks
    struct Mark { double pos; float period; bool voiced; };
    static constexpr int markCap = 2048;
    std::array<Mark, markCap> marks {};
    long long markHead = 0, markTail = 0;   // [tail, head)
    double nextMark = 0.0;
    long long cursor = 0;                   // search start
    double nextSynth = 0.0;
    float throatSm = 1.0f;

    // output
    float mixSm = 1.0f, outSm = 1.0f;
    float modeGain = 1.0f;
    bool switching = false;

    // UI frames
    std::array<std::atomic<float>, frameCapacity> frameDet {}, frameCorr {};
    std::array<std::atomic<int>, frameCapacity> frameTarget {};
    std::atomic<uint32_t> frameCount { 0 };
    int frameCounter = 0, frameInterval = 480;
};

} // namespace akill
