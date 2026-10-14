#include "SampleInstrument.h"

namespace artilody
{

SampleInstrument::SampleInstrument()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (int i = 0; i < 24; ++i)
        synth.addVoice (new juce::SamplerVoice());
}

bool SampleInstrument::loadFile (const juce::File& f, int root, juce::String& error)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr)
    {
        error = "Unsupported or unreadable audio file: " + f.getFileName();
        return false;
    }

    juce::BigInteger notes;
    notes.setRange (0, 128, true);
    const double maxSeconds = juce::jmin (120.0, (double) reader->lengthInSamples / juce::jmax (1.0, reader->sampleRate) + 0.1);
    auto* sound = new juce::SamplerSound (f.getFileNameWithoutExtension(), *reader, notes, root, 0.002, 0.25, maxSeconds);

    synth.allNotesOff (0, false);
    synth.clearSounds();
    synth.addSound (sound);
    file = f;
    rootNote = root;
    return true;
}

} // namespace artilody
