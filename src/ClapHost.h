#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace artilody
{

/** A small CLAP host implemented as a JUCE AudioPluginFormat so CLAP plug-ins
    can be scanned, listed and instantiated exactly like VST3 and AU plug-ins. */
class ClapPluginFormat final : public juce::AudioPluginFormat
{
public:
    ClapPluginFormat() = default;

    static juce::String getFormatName() { return "CLAP"; }
    juce::String getName() const override { return getFormatName(); }

    void findAllTypesForFile (juce::OwnedArray<juce::PluginDescription>& results, const juce::String& fileOrIdentifier) override;
    bool fileMightContainThisPluginType (const juce::String& fileOrIdentifier) override;
    juce::String getNameOfPluginFromIdentifier (const juce::String& fileOrIdentifier) override;
    bool pluginNeedsRescanning (const juce::PluginDescription&) override;
    bool doesPluginStillExist (const juce::PluginDescription&) override;
    bool canScanForPlugins() const override { return true; }
    bool isTrivialToScan() const override { return false; }
    juce::StringArray searchPathsForPlugins (const juce::FileSearchPath&, bool recursive, bool allowAsync) override;
    juce::FileSearchPath getDefaultLocationsToSearch() override;
    bool requiresUnblockedMessageThreadDuringCreation (const juce::PluginDescription&) const override { return false; }

protected:
    void createPluginInstance (const juce::PluginDescription&, double initialSampleRate,
                               int initialBufferSize, PluginCreationCallback) override;
};

} // namespace artilody
