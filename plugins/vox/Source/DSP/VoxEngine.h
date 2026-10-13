// TrapWin VOX DEMON – pitch, formant and hard-tune engine.
//
// Pitch-synchronous overlap-add (TD-PSOLA):
//  * a YIN pitch tracker runs on the (decimated) mono sum,
//  * analysis marks are placed one period apart on the input,
//  * synthesis marks are placed (period / pitchRatio) apart on the output,
//  * each synthesis mark copies the nearest analysis grain (2 periods, Hann window).
// Because grains keep their own waveform, the formants (voice colour) stay put when the pitch moves.
// FORMANT resamples every grain by its own factor, so voice colour moves independently of pitch.
//
// Plain C++17 so it can be tested headless; wrapped by JUCE in PluginProcessor.
#pragma once

#include <array>
#include <deque>
#include <vector>

namespace vox
{

enum class TuneMode { off = 0, chromatic = 1, scale = 2 };

constexpr int numScales = 6;
constexpr std::array<const char*, numScales> scaleNames { "Major", "Minor", "Harmonic Minor", "Minor Pentatonic", "Dorian", "Phrygian" };
constexpr std::array<const char*, 12> keyNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

struct Params
{
    float pitchSemis = 0.0f;     // -24..+24
    float formantSemis = 0.0f;   // -12..+12
    TuneMode tune = TuneMode::off;
    int key = 0;                 // 0 = C
    int scale = 1;               // index into scaleNames
    float retuneMs = 40.0f;      // 0 = instant (robot), up to 400 ms (natural)
    bool midiControl = false;    // held MIDI note sets the target pitch
    float mix = 1.0f;
    float outputDb = 0.0f;
};

class YinTracker
{
public:
    void prepare (double analysisRate);
    // returns f0 in Hz, 0 when unvoiced
    float analyse (const float* window, int size, float& confidence);
    int windowSize() const { return winSize; }

private:
    double rate = 24000.0;
    int winSize = 512, tauMin = 20, tauMax = 400;
    std::vector<float> diff;
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const Params& p) { params = p; }

    // MIDI: note >= 0 = a note is held, -1 = none
    void setMidiNote (int note) { midiNote = note; }

    void process (float* const* channels, int numChannels, int numSamples);

    int getLatencySamples() const { return latency; }

    // UI
    float getDetectedHz() const { return uiDetected; }
    float getTargetHz() const { return uiTarget; }

private:
    void analysisStep();
    void addGrain (long long synthMark);
    float pitchRatioFor (float detectedHz);

    double sr = 48000.0;
    Params params;
    int midiNote = -1;

    // ring buffers (absolute indices modulo size)
    static constexpr int maxChannels = 2;
    int ringSize = 1 << 16;
    std::array<std::vector<float>, maxChannels> in, outAcc;
    std::vector<float> winAcc;
    long long writePos = 0;          // absolute index of the next input sample
    int latency = 0;
    int numCh = 2;

    // pitch tracking (decimated)
    YinTracker yin;
    int decim = 2, hop = 256, hopCounter = 0;
    std::vector<float> decimBuf;
    long long decimWrite = 0;
    float decimLp = 0.0f, decimLp2 = 0.0f;
    float currentPeriod = 300.0f;    // in samples at sr
    bool voiced = false;
    float lastF0 = 0.0f;
    float currentRatio = 1.0f;

    // marks
    struct Mark { long long pos; float period; bool voiced; float ratio; };
    std::deque<Mark> analysisMarks;
    long long nextAnalysis = 0;
    long long nextSynth = 0;

    // tuning state
    float smoothedTargetMidi = -1.0f;
    float retuneCoef = 1.0f;

    // output
    std::vector<float> dryDelay[maxChannels];
    float mixSm = 1.0f, outSm = 1.0f;

    float uiDetected = 0.0f, uiTarget = 0.0f;
    int minPeriod = 40, maxPeriod = 700;
};

// helpers (public for tests)
float snapToScale (float midi, int key, int scale, bool chromatic);
float hzToMidi (float hz);
float midiToHz (float midi);

} // namespace vox
