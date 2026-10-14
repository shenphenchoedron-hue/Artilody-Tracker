#pragma once

#include "AudioEngine.h"

namespace artilody
{
namespace ProjectIO
{
    constexpr const char* fileExtension = ".artilody";

    bool save (AudioEngine&, const juce::File&, juce::String& error);
    bool load (AudioEngine&, const juce::File&, juce::String& error);

    /** Creates the startup demo song using the built-in genre instruments. */
    void createDemoSong (AudioEngine&);

    /** Creates an empty project with a few tracks preloaded with Pop instruments. */
    void createEmptySong (AudioEngine&);

    void setSoundFontInstrument (StripProcessor&, const juce::String& genre, const juce::String& presetName);
}
}
