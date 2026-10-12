// Plugin lifecycle: fast construction / destruction, no hangs, safe teardown while audio runs.
// (The editor side - open / close with pending menus and callbacks - is exercised by
//  EffectorKillaSnapshot --lifecycle, which has a GUI and runs under xvfb.)
#include "TestHelpers.h"
#include "PluginProcessor.h"
#include <thread>

using namespace ektest;

class LifecycleTest : public juce::UnitTest
{
public:
    LifecycleTest() : juce::UnitTest ("Plugin lifecycle", "lifecycle") {}

    static double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

    static void renderBlocks (EffectorKillaAudioProcessor& p, int blocks, int n = 256)
    {
        juce::AudioBuffer<float> buf (2, n);
        juce::MidiBuffer midi;
        for (int b = 0; b < blocks; ++b)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i) buf.setSample (c, i, 0.25f * std::sin ((float) (b * n + i) * 0.031f));
            p.processBlock (buf, midi);
        }
    }

    void runTest() override
    {
        beginTest ("processor create / destroy 50x is fast");
        {
            // first instance pays the process-wide warm-up (factory JSON, preset trim cache)
            const double t0 = nowMs();
            { EffectorKillaAudioProcessor first; }
            const double firstMs = nowMs() - t0;

            double worstCtor = 0.0, worstDtor = 0.0, total = 0.0;
            for (int i = 0; i < 50; ++i)
            {
                const double a = nowMs();
                auto p = std::make_unique<EffectorKillaAudioProcessor>();
                const double b = nowMs();
                p.reset();
                const double c = nowMs();
                worstCtor = juce::jmax (worstCtor, b - a);
                worstDtor = juce::jmax (worstDtor, c - b);
                total += c - a;
            }
            logMessage ("  first ctor+dtor " + juce::String (firstMs, 2) + " ms; 50x avg ctor+dtor "
                        + juce::String (total / 50.0, 2) + " ms, worst ctor " + juce::String (worstCtor, 2)
                        + " ms, worst dtor " + juce::String (worstDtor, 2) + " ms");
           #if ! JUCE_DEBUG
            expect (worstCtor < 30.0, "processor ctor took " + juce::String (worstCtor, 2) + " ms");
            expect (worstDtor < 10.0, "processor dtor took " + juce::String (worstDtor, 2) + " ms");
           #endif
        }

        beginTest ("prepare / release re-entrancy");
        {
            EffectorKillaAudioProcessor p;
            p.releaseResources();                 // before any prepare
            renderBlocks (p, 2);                  // process without prepare must not crash
            p.prepareToPlay (44100.0, 512);
            p.prepareToPlay (48000.0, 256);       // twice, different settings
            renderBlocks (p, 8);
            const int latency = p.getLatencySamples();
            p.releaseResources();
            p.releaseResources();                 // twice
            renderBlocks (p, 2);                  // after release (some hosts do this)
            p.prepareToPlay (48000.0, 4096);
            renderBlocks (p, 4, 4096);
            expectEquals (p.getLatencySamples(), latency);
            p.prepareToPlay (96000.0, 1);
            renderBlocks (p, 64, 1);
            p.releaseResources();
        }

        beginTest ("teardown right after audio stops (audio thread busy until the host stops it)");
        {
            for (int round = 0; round < 10; ++round)
            {
                auto p = std::make_unique<EffectorKillaAudioProcessor>();
                p->prepareToPlay (48000.0, 256);
                std::atomic<bool> run { true };
                std::atomic<int> blocks { 0 };
                std::thread audio ([&]
                {
                    juce::AudioBuffer<float> buf (2, 256);
                    juce::MidiBuffer midi;
                    while (run.load())
                    {
                        for (int c = 0; c < 2; ++c)
                            for (int i = 0; i < 256; ++i) buf.setSample (c, i, 0.2f * std::sin ((float) i * 0.07f));
                        p->processBlock (buf, midi);
                        ++blocks;
                    }
                });
                // message thread keeps changing the rack while audio runs
                for (int k = 0; k < 6; ++k)
                {
                    if (k % 2 == 0) p->kill(); else p->stepPreset (1);
                    juce::Thread::sleep (2);
                }
                // the host stops processing, then deletes the plugin - must not block
                run.store (false);
                audio.join();
                p->releaseResources();
                const double a = nowMs();
                p.reset();
                const double dt = nowMs() - a;
                expect (blocks.load() > 0);
                expect (dt < 100.0, "dtor after audio took " + juce::String (dt, 2) + " ms");
            }
        }

        beginTest ("re-prepare while the audio thread runs (wrapper holds the callback lock)");
        {
            EffectorKillaAudioProcessor p;
            p.prepareToPlay (44100.0, 256);
            std::atomic<bool> run { true };
            std::atomic<int> blocks { 0 };
            std::atomic<bool> finite { true };
            std::thread audio ([&]
            {
                juce::AudioBuffer<float> buf (2, 256);
                juce::MidiBuffer midi;
                while (run.load())
                {
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < 256; ++i) buf.setSample (c, i, 0.2f * std::sin ((float) i * 0.05f));
                    {
                        const juce::ScopedLock sl (p.getCallbackLock());   // as the VST3 / AU wrappers do
                        p.processBlock (buf, midi);
                    }
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < 256; ++i)
                            if (! std::isfinite (buf.getSample (c, i))) finite = false;
                    ++blocks;
                }
            });
            const double rates[] = { 48000.0, 96000.0, 44100.0 };
            for (int k = 0; k < 9; ++k)
            {
                p.prepareToPlay (rates[k % 3], 128 << (k % 3));
                if (k % 3 == 1) p.kill();
                juce::Thread::sleep (3);
            }
            run.store (false);
            audio.join();
            expect (blocks.load() > 0);
            expect (finite.load(), "output stays finite across re-prepares");
        }

        beginTest ("destroy without release / with pending crossfade");
        {
            for (int round = 0; round < 10; ++round)
            {
                auto p = std::make_unique<EffectorKillaAudioProcessor>();
                p->prepareToPlay (44100.0, 128);
                renderBlocks (*p, 3, 128);
                p->kill();               // new chain pending, never consumed
                renderBlocks (*p, 1, 128); // crossfade running
                p->stepPreset (1);       // another pending chain
                const double a = nowMs();
                p.reset();               // no releaseResources()
                expect (nowMs() - a < 100.0);
            }
        }

        beginTest ("state survives recreate");
        {
            juce::MemoryBlock mb;
            juce::String name;
            {
                EffectorKillaAudioProcessor p;
                p.loadFactory (3, 1, false);
                p.kill();
                name = p.getMeta().name;
                p.getStateInformation (mb);
            }
            EffectorKillaAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (q.getMeta().name, name);
            juce::MemoryBlock mb2;
            q.getStateInformation (mb2);
            expect (mb == mb2, "state round trip after recreate");
        }
    }
};

static LifecycleTest lifecycleTest;
