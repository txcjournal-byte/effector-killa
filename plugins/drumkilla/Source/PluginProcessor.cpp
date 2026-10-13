#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

K808Processor::K808Processor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", AudioChannelSet::stereo(), false)),
      apvts (*this, &undoManager, "K808", createParameterLayout()),
      presets (apvts)
{
    auto rawFor = [this] (const char* id)
    {
        auto* v = apvts.getRawParameterValue (id);
        jassert (v != nullptr);
        return v;
    };
    using namespace ParamIDs;
    raw.inputGain = rawFor (inputGain);
    raw.phaseInvert = rawFor (phaseInvert);
    raw.crossover = rawFor (crossoverFreq);
    raw.subMono = rawFor (subMono);
    raw.subCut = rawFor (subCut);
    raw.drive = rawFor (driveAmount);
    raw.satMode = rawFor (satMode);
    raw.focus = rawFor (midFocus);
    raw.focusFreq = rawFor (focusFreq);
    raw.duckDepth = rawFor (duckDepth);
    raw.duckRelease = rawFor (duckRelease);
    raw.clipDrive = rawFor (clipDrive);
    raw.clipKnee = rawFor (clipKnee);
    raw.ceiling = rawFor (clipCeiling);
    raw.phone = rawFor (phonePreview);
    raw.outputGain = rawFor (outputGain);
    raw.bypass = rawFor (bypass);
    raw.hit = rawFor (hit);

    bypassParam = apvts.getParameter (ParamIDs::bypass);

    // start on the default settings (not undoable)
    presets.init();
    undoManager.clearUndoHistory();
}

bool K808Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    const auto monoOrStereo = [] (const AudioChannelSet& s)
    { return s == AudioChannelSet::mono() || s == AudioChannelSet::stereo(); };

    if (! monoOrStereo (in) || ! monoOrStereo (out))
        return false;
    if (in.size() > out.size())                       // mono->mono, mono->stereo, stereo->stereo
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && ! monoOrStereo (sc))
            return false;
    }
    return true;
}

void K808Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
}

void K808Processor::reset()
{
    engine.reset();
}

double K808Processor::getTailLengthSeconds() const
{
    return 0.05;
}

EngineParams K808Processor::readParams() const
{
    EngineParams p;
    p.inputGainDb = raw.inputGain->load();
    p.phaseInvert = raw.phaseInvert->load() > 0.5f;
    p.crossoverHz = raw.crossover->load();
    p.subMono = raw.subMono->load() > 0.5f;
    p.subCut = raw.subCut->load() > 0.5f;
    p.drive = raw.drive->load();
    p.satMode = (int) raw.satMode->load();
    p.focusDb = raw.focus->load();
    p.focusHz = raw.focusFreq->load();
    p.duckDepth = raw.duckDepth->load();
    p.duckReleaseMs = raw.duckRelease->load();
    p.clipDriveDb = raw.clipDrive->load();
    p.clipKnee = raw.clipKnee->load();
    p.ceilingDb = raw.ceiling->load();
    p.phone = raw.phone->load() > 0.5f;
    p.outputDb = raw.outputGain->load();
    p.bypass = raw.bypass->load() > 0.5f;
    p.hit = raw.hit->load();
    return p;
}

void K808Processor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    auto main = getBusBuffer (buffer, true, 0);
    const auto inChannels = getMainBusNumInputChannels();
    const auto outChannels = getMainBusNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    if (outChannels == 0 || numSamples == 0)
        return;

    auto out = getBusBuffer (buffer, false, 0);

    // mono in -> stereo out: duplicate the input
    if (inChannels == 1 && outChannels == 2)
        out.copyFrom (1, 0, out, 0, 0, numSamples);

    ignoreUnused (main);
    const auto params = readParams();

    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled() && getChannelCountOfBus (true, 1) > 0)
    {
        const auto sidechain = getBusBuffer (buffer, true, 1);   // refers to the host's data, no copy
        engine.process (out, jmin (2, outChannels), &sidechain, params);
    }
    else
    {
        engine.process (out, jmin (2, outChannels), nullptr, params);
    }
}

void K808Processor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void K808Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml));
            presets.restoreFromState();
        }
}

AudioProcessorEditor* K808Processor::createEditor()
{
    return new K808Editor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new K808Processor();
}
