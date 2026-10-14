#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace artilody
{

/** Simple built-in sampler: plays an audio file chromatically across the keyboard. */
class SampleInstrument final : public juce::AudioProcessor
{
public:
    SampleInstrument();

    static juce::String getWildcard() { return "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3"; }

    /** Message thread. rootNote is the MIDI note at which the sample plays at original pitch. */
    bool loadFile (const juce::File& file, int rootNote, juce::String& error);

    juce::File getFile() const { return file; }
    int getRootNote() const noexcept { return rootNote; }

    const juce::String getName() const override { return file.existsAsFile() ? file.getFileNameWithoutExtension() : juce::String ("Sampler"); }
    void prepareToPlay (double sampleRate, int) override { synth.setCurrentPlaybackSampleRate (sampleRate); }
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        buffer.clear();
        synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
    }
    double getTailLengthSeconds() const override { return 0.5; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return getName(); }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    juce::Synthesiser synth;
    juce::File file;
    int rootNote = 60;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleInstrument)
};

} // namespace artilody
