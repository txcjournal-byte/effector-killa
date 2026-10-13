// TrapWin CHOPPER - tempo-synced stutter / slice-repeat engine (plain C++, no JUCE).
//
// A 16-step sequencer locked to the host's musical position (PPQ). Each step picks an effect that is
// applied to the live input. Everything reads from an always-recording input history, so latency is 0:
//   Off        dry signal (bit-exact)
//   Repeat     captures a slice at the start of the run and retriggers it (slice = repeat size)
//   Reverse    plays the audio leading up to the run backwards (mirrored around the run start)
//   Tape Stop  varispeed slow-down to a halt over the run
//   Gate       mute
//   Pitch Down octave-down pitch shift that keeps the rhythm (two-grain shifter)
//   Half-Speed half-speed playback from the run start (octave down + slow-mo)
// Consecutive steps with the same effect form one "run" (a 4-step tape stop is one long stop).
// A run never crosses the pattern start. Every discontinuity (run change, repeat wrap, transport jump)
// is hidden by a short raised-cosine crossfade (~3 ms).
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace chopper
{

enum class Fx : uint8_t { off = 0, repeat, reverse, tapeStop, gate, pitchDown, halfSpeed };
constexpr int numFx = 7;
constexpr int numSteps = 16;

inline const char* fxName (Fx f)
{
    static const char* n[] = { "Off", "Repeat", "Reverse", "Tape Stop", "Gate", "Pitch Down", "Half-Speed" };
    return n[(int) f];
}
inline const char* fxShort (Fx f)
{
    static const char* n[] = { "OFF", "RPT", "REV", "STOP", "GATE", "PITCH", "HALF" };
    return n[(int) f];
}

// step rate (sequencer resolution)
constexpr int numRates = 4;
inline const char* const rateNames[numRates] = { "1/8", "1/16", "1/32", "1/16T" };
inline double rateBeats (int r)
{
    static const double b[numRates] = { 0.5, 0.25, 0.125, 1.0 / 6.0 };
    return b[r < 0 ? 0 : (r >= numRates ? numRates - 1 : r)];
}

// repeat slice size
constexpr int numSizes = 6;
inline const char* const sizeNames[numSizes] = { "1/4", "1/8", "1/16", "1/32", "1/64", "1/16T" };
inline double sizeBeats (int s)
{
    static const double b[numSizes] = { 1.0, 0.5, 0.25, 0.125, 0.0625, 1.0 / 6.0 };
    return b[s < 0 ? 0 : (s >= numSizes ? numSizes - 1 : s)];
}

using Pattern = std::array<Fx, numSteps>;

// pattern <-> text (state + presets): one char per step, '.' = Off
constexpr const char* fxChars = ".RVTGPH";
inline std::string patternToString (const Pattern& p)
{
    std::string s;
    for (auto f : p) s += fxChars[(int) f];
    return s;
}
// accepts 16 chars (unknown chars -> Off); returns false and leaves p untouched on a wrong length
inline bool patternFromString (const std::string& s, Pattern& p)
{
    if (s.size() != (size_t) numSteps) return false;
    for (int i = 0; i < numSteps; ++i)
    {
        Fx f = Fx::off;
        for (int k = 0; k < numFx; ++k)
            if (s[(size_t) i] == fxChars[k]) f = (Fx) k;
        p[(size_t) i] = f;
    }
    return true;
}

struct Params
{
    int rate = 1;            // index into rateNames
    int size = 3;            // index into sizeNames
    float swing = 0.0f;      // 0..1 (0 = straight, 1 = 75 % swing)
    float mix = 1.0f;        // 0..1
    float outputDb = 0.0f;
    bool hold = false;       // forces Repeat while true
};

struct HostTime
{
    bool playing = false;
    bool hasPpq = false;
    double ppq = 0.0;
    double bpm = 0.0;        // <= 0: unknown (free-run uses 120 BPM)
};

class Engine
{
public:
    static constexpr double fadeMs = 3.0;
    static constexpr double historySeconds = 8.0;
    static constexpr double minBpmForBuffers = 40.0;   // slices at slower tempos are clamped

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const Params& p) { params = p; }
    void setPattern (const Pattern& p) { pattern = p; }
    void process (float* const* ch, int numCh, int n, const HostTime& t);

    // UI / test feedback (written on the audio thread)
    int getCurrentStep() const { return uiStep.load (std::memory_order_relaxed); }
    Fx getCurrentFx() const { return (Fx) uiFx.load (std::memory_order_relaxed); }
    double getStepPhase() const { return uiStepPhase.load (std::memory_order_relaxed); }
    double getBpm() const { return uiBpm.load (std::memory_order_relaxed); }
    int getLatencySamples() const { return 0; }
    int getFadeSamples() const { return fadeLen; }

    // number of samples of a repeat slice for the given size / tempo (rounded, clamped)
    int sliceSamples (int sizeIndex, double bpm) const;

private:
    struct Voice
    {
        Fx fx = Fx::off;
        int64_t runStart = 0;    // absolute sample index where the run (or slice) starts
        int64_t runLen = 1;      // samples (tape stop curve length)
        int64_t wrapBase = 0;    // repeat: elapsed samples at the last slice restart
        int buf = 0;             // repeat: capture buffer
        int fade = 0;            // remaining fade-out samples (fading voices)
        int fadeTotal = 1;
        float startGain = 1.0f;
        bool active = false;
    };

    float readHist (int c, int64_t absIndex) const;
    float readHistFrac (int c, double absPos) const;
    float renderVoice (const Voice& v, int c) const;
    void startVoice (Fx fx, int64_t runStart, int64_t runLen, int numCh);
    void pushFade (int len);
    int64_t runLenFrom (int step, Fx fx, double stepB, double sw, double spb) const;

    double sr = 44100.0;
    Params params;
    Pattern pattern {};

    int histLen = 0;               // power of two
    int64_t histMask = 0;
    std::vector<float> hist[2];
    int capLen = 0;
    std::vector<float> cap[2][2];  // [buffer][channel]
    int64_t capFilled[2] { 0, 0 };
    int lastCapBuf = 0;

    int64_t now = 0;               // absolute sample counter
    Voice current;
    static constexpr int maxFades = 4;
    std::array<Voice, maxFades> fading {};
    int fadeInPos = 1 << 30, fadeInLen = 1;
    int fadeLen = 132;
    double grainLen = 2048.0;
    int64_t maxRun = 1;

    // sequencer state
    int curStep = -1;
    double lastBeat = -1.0e9;
    double freeBeat = 0.0;
    bool wasPlaying = false;
    bool lastHold = false;
    bool runFromHold = false;

    // smoothed controls
    float mixS = 1.0f, gainS = 1.0f;
    bool snapControls = true;

    std::atomic<int> uiStep { 0 }, uiFx { 0 };
    std::atomic<double> uiStepPhase { 0.0 }, uiBpm { 120.0 };
};

} // namespace chopper
