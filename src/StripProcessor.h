#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BuiltinFX.h"
#include "SoundFontInstrument.h"
#include "SampleInstrument.h"

namespace artilody
{

enum class InstrumentKind
{
    None,
    SoundFont,
    Sample,
    Plugin
};

struct PluginSlot
{
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    std::atomic<bool> bypassed { false };
};

/** The DSP side of a mixer channel strip: instrument -> plug-in FX -> EQ -> Comp -> Reverb -> pan/volume. */
class StripProcessor
{
public:
    explicit StripProcessor (bool isMasterStrip);
    ~StripProcessor();

    const bool isMaster;

    //==============================================================================
    InstrumentKind getInstrumentKind() const noexcept { return instrumentKind; }
    juce::AudioProcessor* getInstrument() const noexcept { return instrument.get(); }
    SoundFontInstrument* getSoundFont() const noexcept;
    SampleInstrument* getSampler() const noexcept;
    juce::AudioPluginInstance* getInstrumentPlugin() const noexcept;
    juce::String getInstrumentName() const;
    juce::String getInstrumentTypeName() const;

    /** Message thread. Returns the previous instrument so the caller can close windows before it dies. */
    std::unique_ptr<juce::AudioProcessor> setInstrument (std::unique_ptr<juce::AudioProcessor>, InstrumentKind);

    //==============================================================================
    int getNumFx() const noexcept { return (int) fx.size(); }
    PluginSlot* getFx (int i) const noexcept { return juce::isPositiveAndBelow (i, (int) fx.size()) ? fx[(size_t) i].get() : nullptr; }
    void insertFx (std::unique_ptr<juce::AudioPluginInstance>, int index = -1);
    std::unique_ptr<PluginSlot> removeFx (int index);
    void moveFx (int from, int to);

    //==============================================================================
    BuiltinEQ eq;
    BuiltinCompressor comp;
    BuiltinReverb reverb;

    BuiltinEffect* getBuiltin (int i) noexcept
    {
        return i == 0 ? static_cast<BuiltinEffect*> (&eq) : (i == 1 ? static_cast<BuiltinEffect*> (&comp) : static_cast<BuiltinEffect*> (&reverb));
    }

    std::atomic<float> volumeDb { 0.0f };
    std::atomic<float> pan { 0.0f };    // -1 .. 1
    std::atomic<bool> mute { false }, solo { false };
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };

    // UI state persisted with the project
    bool fxCollapsed = false;

    //==============================================================================
    void prepare (double sampleRate, int blockSize, juce::AudioPlayHead* playHead);
    void release();
    void setNonRealtime (bool);

    /** Prepares a processor with the strip's current settings (message thread, no lock needed). */
    void prepareProcessor (juce::AudioProcessor& p, bool isInstrument);

    /** Audio thread. For tracks the buffer is cleared and filled by the instrument.
        For the master strip the buffer contains the summed tracks. */
    void process (juce::AudioBuffer<float>& io, juce::MidiBuffer& midi, int numSamples, bool audible);

    juce::CriticalSection lock;

    juce::ValueTree builtinsToValueTree() const;
    void builtinsFromValueTree (const juce::ValueTree&);

private:
    void runProcessor (juce::AudioProcessor& p, juce::AudioBuffer<float>& io, juce::MidiBuffer& midi, int numSamples);
    void ensureScratch (int channels);

    std::unique_ptr<juce::AudioProcessor> instrument;
    InstrumentKind instrumentKind = InstrumentKind::None;
    std::vector<std::unique_ptr<PluginSlot>> fx;

    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer emptyMidi;
    double sampleRate = 44100.0;
    int blockSize = 512;
    juce::AudioPlayHead* playHead = nullptr;
    juce::SmoothedValue<float> gainL, gainR;
};

} // namespace artilody
