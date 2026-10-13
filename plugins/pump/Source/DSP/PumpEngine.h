// TrapWin PUMP – rhythmic volume shaper / sidechain ducker.
// Plain C++17 so it can be tested headless; wrapped by JUCE in PluginProcessor.
#pragma once

#include <array>
#include <vector>

namespace pump
{

// One breakpoint of the gain curve. x = position in the cycle (0..1), y = gain (0..1).
// tension bends the segment that STARTS at this point (-1 = fast then slow, 0 = linear, +1 = slow then fast).
struct Point
{
    float x = 0.0f, y = 1.0f, tension = 0.0f;
};

class Curve
{
public:
    Curve();
    void setPoints (std::vector<Point> pts); // sorted & clamped internally, first point forced to x = 0
    const std::vector<Point>& getPoints() const { return points; }
    float evaluate (float x) const;          // x in [0,1)

    static Curve preset (int index);         // factory shapes, see presetName()
    static const char* presetName (int index);
    static int numPresets();

private:
    std::vector<Point> points;
};

enum class Trigger { sync = 0, midi = 1, audio = 2 };
enum class Band { full = 0, low = 1, high = 2 };

// Cycle length in beats (quarter notes) for each RATE choice.
constexpr std::array<double, 9> rateBeats { 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 2.0 / 3.0, 1.0 / 3.0, 1.5 };
constexpr std::array<const char*, 9> rateNames { "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/4." };

struct Params
{
    int rate = 3;                 // index into rateBeats
    Trigger trigger = Trigger::sync;
    Band band = Band::full;
    float crossoverHz = 120.0f;   // used when band != full
    float depth = 1.0f;           // 0..1 how far the curve pulls down
    float smoothMs = 2.0f;        // anti-click slew
    float threshold = 0.25f;      // audio trigger level (linear peak)
    float mix = 1.0f;             // 0..1
    float outputDb = 0.0f;
};

struct HostTime
{
    bool playing = false;
    double ppq = 0.0;     // position at the start of the block, in quarter notes
    double bpm = 120.0;
};

class Biquad
{
public:
    void setLowpass (double sr, double f, double q);
    void setHighpass (double sr, double f, double q);
    void reset() { z1 = z2 = 0.0; }
    float process (float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

private:
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

class Engine
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const Params& p);
    void setCurve (const Curve& c) { curve = c; }

    // MIDI note-ons inside the coming block (sample offsets). Cleared after process().
    void addNoteOn (int sampleOffset) { if (numNotes < (int) noteOffsets.size()) noteOffsets[(size_t) numNotes++] = sampleOffset; }

    // main: in/out channels. sidechain may be null (audio trigger then listens to the main input).
    void process (float* const* main, int numChannels, const float* const* sidechain, int numScChannels,
                  int numSamples, const HostTime& time);

    float getPhase() const { return (float) phase; }   // for the UI playhead
    float getCurrentGain() const { return lastGain; }  // 0..1 for the UI meter
    bool  hasTriggered() const { return triggeredFlag; }

private:
    void updateFilters();

    double sr = 44100.0;
    Params params;
    Curve curve;

    // phase in cycles (0..1). In MIDI/AUDIO mode the cycle runs once per trigger and holds at the end.
    double phase = 0.0;
    bool oneShotRunning = false;

    std::array<int, 128> noteOffsets {};
    int numNotes = 0;

    // audio trigger detector
    float detEnv = 0.0f, detFloor = 0.0f;
    int holdoff = 0;

    float gainSmoothed = 1.0f, smoothCoef = 1.0f, lastGain = 1.0f;
    float mixSm = 1.0f, outSm = 1.0f;
    bool triggeredFlag = false;

    struct Split { Biquad lp1, lp2, hp1, hp2; };
    std::array<Split, maxChannels> split;
    float lastCrossover = -1.0f;
};

} // namespace pump
