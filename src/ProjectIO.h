#pragma once

#include "AudioEngine.h"

namespace artilody
{
namespace ProjectIO
{
    /** Project files are plain XML, hence the .artixml extension. */
    constexpr const char* fileExtension = ".artixml";

    /** Extension used by Artilody Tracker 1.0.0 - 1.0.2. Still opened, saved as .artixml. */
    constexpr const char* legacyFileExtension = ".artilody";

    /** Wildcard for the open dialog (current + legacy extension). */
    constexpr const char* openWildcard = "*.artixml;*.artilody";

    /** True if the file has a project extension (current or legacy). */
    inline bool isProjectFile (const juce::File& f) { return f.hasFileExtension ("artixml;artilody"); }

    bool save (AudioEngine&, const juce::File&, juce::String& error);
    bool load (AudioEngine&, const juce::File&, juce::String& error);

    /** Creates the startup demo song using the built-in genre instruments. */
    void createDemoSong (AudioEngine&);

    /** Creates a swing jazz test song (quartet + vibraphone solo). */
    void createJazzDemo (AudioEngine&);

    /** Creates an empty project with a few tracks preloaded with Pop instruments. */
    void createEmptySong (AudioEngine&);

    void setSoundFontInstrument (StripProcessor&, const juce::String& genre, const juce::String& presetName);
}
}
