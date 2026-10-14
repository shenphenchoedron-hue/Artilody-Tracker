#include "StripProcessor.h"

namespace artilody
{

StripProcessor::StripProcessor (bool isMasterStrip) : isMaster (isMasterStrip)
{
    scratch.setSize (2, 4096);
    gainL.reset (44100.0, 0.02);
    gainR.reset (44100.0, 0.02);
}

StripProcessor::~StripProcessor()
{
    release();
}

SoundFontInstrument* StripProcessor::getSoundFont() const noexcept
{
    return instrumentKind == InstrumentKind::SoundFont ? static_cast<SoundFontInstrument*> (instrument.get()) : nullptr;
}

SampleInstrument* StripProcessor::getSampler() const noexcept
{
    return instrumentKind == InstrumentKind::Sample ? static_cast<SampleInstrument*> (instrument.get()) : nullptr;
}

juce::AudioPluginInstance* StripProcessor::getInstrumentPlugin() const noexcept
{
    return instrumentKind == InstrumentKind::Plugin ? static_cast<juce::AudioPluginInstance*> (instrument.get()) : nullptr;
}

juce::String StripProcessor::getInstrumentName() const
{
    if (instrument == nullptr) return "No instrument";
    return instrument->getName();
}

juce::String StripProcessor::getInstrumentTypeName() const
{
    if (auto* sf = getSoundFont())
        return sf->genre.isNotEmpty() ? sf->genre + " / SoundFont" : juce::String ("SoundFont");
    if (getSampler() != nullptr)
        return "Sampler (audio file)";
    if (auto* p = getInstrumentPlugin())
        return p->getPluginDescription().pluginFormatName.replace ("AudioUnit", "AU");
    return "Choose...";
}

void StripProcessor::ensureScratch (int channels)
{
    channels = juce::jmax (2, channels);
    if (scratch.getNumChannels() < channels || scratch.getNumSamples() < blockSize)
    {
        const juce::ScopedLock sl (lock);
        scratch.setSize (juce::jmax (channels, scratch.getNumChannels()), juce::jmax (blockSize, scratch.getNumSamples()), false, true, true);
    }
}

void StripProcessor::prepareProcessor (juce::AudioProcessor& p, bool isInstrument)
{
    // try a plain stereo layout on the main buses
    auto layout = p.getBusesLayout();
    if (layout.outputBuses.size() > 0)
        layout.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    if (layout.inputBuses.size() > 0)
        layout.inputBuses.getReference (0) = isInstrument ? layout.inputBuses[0] : juce::AudioChannelSet::stereo();
    if (layout != p.getBusesLayout() && p.checkBusesLayoutSupported (layout))
        p.setBusesLayout (layout);

    p.setPlayHead (playHead);
    p.setRateAndBufferSizeDetails (sampleRate, blockSize);
    p.prepareToPlay (sampleRate, blockSize);
    ensureScratch (juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()));
}

std::unique_ptr<juce::AudioProcessor> StripProcessor::setInstrument (std::unique_ptr<juce::AudioProcessor> newInstrument, InstrumentKind kind)
{
    if (newInstrument != nullptr)
        prepareProcessor (*newInstrument, true);
    else
        kind = InstrumentKind::None;

    std::unique_ptr<juce::AudioProcessor> old;
    {
        const juce::ScopedLock sl (lock);
        old = std::move (instrument);
        instrument = std::move (newInstrument);
        instrumentKind = kind;
    }
    if (old != nullptr)
        old->releaseResources();
    return old;
}

void StripProcessor::insertFx (std::unique_ptr<juce::AudioPluginInstance> plugin, int index)
{
    if (plugin == nullptr) return;
    prepareProcessor (*plugin, false);

    auto slot = std::make_unique<PluginSlot>();
    slot->plugin = std::move (plugin);

    const juce::ScopedLock sl (lock);
    if (index < 0 || index > (int) fx.size())
        index = (int) fx.size();
    fx.insert (fx.begin() + index, std::move (slot));
}

std::unique_ptr<PluginSlot> StripProcessor::removeFx (int index)
{
    std::unique_ptr<PluginSlot> s;
    {
        const juce::ScopedLock sl (lock);
        if (! juce::isPositiveAndBelow (index, (int) fx.size()))
            return {};
        s = std::move (fx[(size_t) index]);
        fx.erase (fx.begin() + index);
    }
    s->plugin->releaseResources();
    return s;
}

void StripProcessor::moveFx (int from, int to)
{
    const juce::ScopedLock sl (lock);
    if (! juce::isPositiveAndBelow (from, (int) fx.size()) || ! juce::isPositiveAndBelow (to, (int) fx.size()) || from == to)
        return;
    auto s = std::move (fx[(size_t) from]);
    fx.erase (fx.begin() + from);
    fx.insert (fx.begin() + to, std::move (s));
}

//==============================================================================
void StripProcessor::prepare (double sr, int bs, juce::AudioPlayHead* ph)
{
    sampleRate = sr;
    blockSize = juce::jmax (16, bs);
    playHead = ph;

    gainL.reset (sr, 0.02);
    gainR.reset (sr, 0.02);

    eq.prepare (sr, blockSize);
    comp.prepare (sr, blockSize);
    reverb.prepare (sr, blockSize);

    ensureScratch (2);
    if (instrument != nullptr)
        prepareProcessor (*instrument, true);
    for (auto& s : fx)
        prepareProcessor (*s->plugin, false);

    scratch.setSize (scratch.getNumChannels(), juce::jmax (blockSize, 16), false, true, false);
}

void StripProcessor::release()
{
    if (instrument != nullptr)
        instrument->releaseResources();
    for (auto& s : fx)
        if (s->plugin != nullptr)
            s->plugin->releaseResources();
}

void StripProcessor::setNonRealtime (bool b)
{
    if (instrument != nullptr) instrument->setNonRealtime (b);
    for (auto& s : fx) s->plugin->setNonRealtime (b);
}

void StripProcessor::runProcessor (juce::AudioProcessor& p, juce::AudioBuffer<float>& io, juce::MidiBuffer& midi, int n)
{
    if (p.isSuspended())
        return;

    const int ins = p.getTotalNumInputChannels();
    const int outs = p.getTotalNumOutputChannels();
    const int numCh = juce::jmax (ins, outs, 2);
    if (numCh > scratch.getNumChannels() || n > scratch.getNumSamples())
        return;

    for (int c = 0; c < numCh; ++c)
        scratch.clear (c, 0, n);

    if (ins == 1)
    {
        scratch.copyFrom (0, 0, io, 0, 0, n);
        scratch.addFrom (0, 0, io, 1, 0, n);
        scratch.applyGain (0, 0, n, 0.5f);
    }
    else if (ins >= 2)
    {
        scratch.copyFrom (0, 0, io, 0, 0, n);
        scratch.copyFrom (1, 0, io, 1, 0, n);
    }

    juce::AudioBuffer<float> view (scratch.getArrayOfWritePointers(), numCh, n);
    {
        const juce::ScopedLock pl (p.getCallbackLock());
        p.processBlock (view, midi);
    }

    if (outs >= 2)
    {
        io.copyFrom (0, 0, scratch, 0, 0, n);
        io.copyFrom (1, 0, scratch, 1, 0, n);
    }
    else if (outs == 1)
    {
        io.copyFrom (0, 0, scratch, 0, 0, n);
        io.copyFrom (1, 0, scratch, 0, 0, n);
    }
}

void StripProcessor::process (juce::AudioBuffer<float>& io, juce::MidiBuffer& midi, int n, bool audible)
{
    const juce::ScopedLock sl (lock);

    if (! isMaster)
    {
        io.clear (0, n);
        if (instrument != nullptr)
            runProcessor (*instrument, io, midi, n);
    }

    for (auto& s : fx)
        if (! s->bypassed.load())
            runProcessor (*s->plugin, io, emptyMidi, n);

    float* L = io.getWritePointer (0);
    float* R = io.getWritePointer (1);

    if (eq.enabled.load())     eq.process (L, R, n);
    if (comp.enabled.load())   comp.process (L, R, n);
    if (reverb.enabled.load()) reverb.process (L, R, n);

    const float vol = audible ? juce::Decibels::decibelsToGain (volumeDb.load(), -70.0f) : 0.0f;
    const float p = juce::jlimit (-1.0f, 1.0f, pan.load());
    gainL.setTargetValue (vol * (p > 0.0f ? 1.0f - p : 1.0f));
    gainR.setTargetValue (vol * (p < 0.0f ? 1.0f + p : 1.0f));

    float pkL = 0.0f, pkR = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        L[i] *= gainL.getNextValue();
        R[i] *= gainR.getNextValue();
        pkL = juce::jmax (pkL, std::abs (L[i]));
        pkR = juce::jmax (pkR, std::abs (R[i]));
    }

    if (pkL > peakL.load()) peakL = pkL;
    if (pkR > peakR.load()) peakR = pkR;
}

//==============================================================================
juce::ValueTree StripProcessor::builtinsToValueTree() const
{
    juce::ValueTree v ("BUILTINS");
    v.appendChild (eq.toValueTree(), nullptr);
    v.appendChild (comp.toValueTree(), nullptr);
    v.appendChild (reverb.toValueTree(), nullptr);
    return v;
}

void StripProcessor::builtinsFromValueTree (const juce::ValueTree& v)
{
    for (auto c : v)
    {
        auto n = c.getProperty ("name").toString();
        if (n == eq.name) eq.fromValueTree (c);
        else if (n == comp.name) comp.fromValueTree (c);
        else if (n == reverb.name) reverb.fromValueTree (c);
    }
}

} // namespace artilody
