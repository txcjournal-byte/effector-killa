/*
    TrapWin BEND DEMON
    InternalSynth.h - the built-in MPE synth (2.0), so BEND DEMON makes sound without loading a
    third-party plugin.

    * Reads the same MIDI the hosted synth would get: per-channel pitch bend (MPE member channels,
      or channel 1 in Mono/Legacy mode), channel pressure, CC74.
    * 16 voices. Per voice: polyBLEP saw / square, sine, 3-voice unison, 2-op FM (bell),
      TPT state-variable low-pass, amp + filter ADSR, optional saturation and pitch "punch".
      Pitch follows the channel bend with a ~1.5 ms smoother (no zipper noise at control rate).
    * Pressure -> amplitude + cutoff, timbre (CC74) -> cutoff.
    * Global: stereo chorus, juce::Reverb, tanh soft clip.
    * Six factory sounds tuned for trap: Glide Lead, Pluck, 808, Pad, Bell, Sub Bass.

    Audio thread only after prepare(); no allocation in render().
*/
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace bd
{

class InternalSynth
{
public:
    enum Sound { glideLead = 0, pluck, eightOhEight, pad, bell, subBass, numSounds };
    static const char* getSoundName (int sound) noexcept;

    static constexpr int numVoices = 16;

    InternalSynth();

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;                         // silent, immediate

    void setSound (int sound) noexcept;            // new notes use it; the FX switch immediately
    int  getSound() const noexcept                 { return sound; }

    /** memberRange: bend range of channels 2..16 (and of channel 1 when legacy == true).
        In MPE mode channel 1 is the master channel: its bend (range masterRange) adds to all notes. */
    void setBendRanges (float memberRange, float masterRange, bool legacyChannel1) noexcept;

    /** Releases every voice (with its release time). */
    void allNotesOff() noexcept;

    /** Renders `num` samples and ADDS them to out[startSample ...]. MIDI positions are relative to
        startSample (0 .. num-1). */
    void render (juce::AudioBuffer<float>& out, int startSample, int num, const juce::MidiBuffer& midi) noexcept;

    int getNumActiveVoices() const noexcept;

    struct Def
    {
        float saw = 1.0f, square = 0.0f, sine = 0.0f, sub = 0.0f, pulseWidth = 0.5f;
        float unison = 0.0f;        // detune in cents for the 3-voice unison (0 = single osc)
        float fmRatio = 0.0f, fmIndex = 0.0f, fmDecay = 1.0f;   // 2-op FM sine (bell)
        float cutoff = 2000.0f, keyTrack = 0.5f, resonance = 0.2f;
        float envAmount = 2.0f;     // octaves
        float timbreAmount = 2.0f;  // octaves at timbre 0 -> 1
        float pressureCut = 1.0f;   // octaves at full pressure
        float pressureAmp = 0.4f;   // 0..1
        juce::ADSR::Parameters amp { 0.005f, 0.3f, 0.8f, 0.25f }, filter { 0.002f, 0.4f, 0.3f, 0.3f };
        float drive = 0.0f;         // 0 = clean
        float punchSemis = 0.0f, punchTime = 0.03f;
        float chorus = 0.0f, reverb = 0.0f;
        float gain = 0.5f;
    };

    static const Def& getDef (int sound) noexcept;

private:
    struct Voice
    {
        bool  active = false;
        int   channel = 1, note = 60;
        float velocity = 1.0f;
        int64_t age = 0;
        double phase[3] {}, subPhase = 0.0, sinePhase = 0.0, modPhase = 0.0;
        float semis = 60.0f;          // smoothed pitch (MIDI note units)
        float pressure = 0.5f, timbre = 0.5f;
        float ic1 = 0.0f, ic2 = 0.0f; // SVF state
        float punch = 0.0f, fmEnv = 1.0f, filtLast = 0.0f;
        bool  released = false;
        int   coefPhase = 0;
        juce::ADSR ampEnv, filtEnv;
        const Def* def = nullptr;

        // per-16-sample coefficients
        float inc[3] {}, subInc = 0.0f, sineInc = 0.0f, modInc = 0.0f;
        float g = 0.1f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    };

    void handle (const juce::MidiMessage&) noexcept;
    void noteOn (int ch, int note, float vel) noexcept;
    void noteOff (int ch, int note) noexcept;
    float targetPitch (const Voice&) const noexcept;
    void updateCoefficients (Voice&) noexcept;
    float renderVoice (Voice&) noexcept;
    void renderSegment (juce::AudioBuffer<float>&, int start, int num) noexcept;

    std::array<Voice, numVoices> voices;
    std::array<float, 17> chanBend {}, chanPressure {}, chanTimbre {};
    float memberRange = 48.0f, masterRange = 2.0f;
    bool  legacy1 = false;

    double sampleRate = 44100.0;
    int sound = glideLead;
    int64_t ageCounter = 0;
    float pitchCoef = 0.05f, ctrlCoef = 0.01f;

    // FX
    juce::Reverb reverb;
    std::vector<float> chorusBuf[2];
    int chorusPos = 0;
    double chorusPhase = 0.0;
    float chorusMix = 0.0f, reverbMix = 0.0f, chorusMixTarget = 0.0f, reverbMixTarget = 0.0f;
    juce::AudioBuffer<float> scratch;
};

} // namespace bd
