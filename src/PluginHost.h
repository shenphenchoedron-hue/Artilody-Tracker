#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>

namespace artilody
{

/** Owns the plug-in format manager, the known plug-in list and plug-in editor windows. */
class PluginHost final : private juce::ChangeListener
{
public:
    explicit PluginHost (juce::ApplicationProperties& props);
    ~PluginHost() override;

    static PluginHost& get();

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;

    /** Opens the Plug-in Manager; with startScanAll it immediately scans all formats for new plug-ins. */
    void showPluginManager (bool startScanAll = false);

    std::unique_ptr<juce::AudioPluginInstance> createPlugin (const juce::PluginDescription&, double sampleRate, int blockSize, juce::String& error);

    /** Fills a popup menu with plug-ins grouped by format and vendor. Item ids are baseId + index into 'out'. */
    void addPluginsToMenu (juce::PopupMenu& menu, bool instruments, int baseId, juce::Array<juce::PluginDescription>& out) const;

    void showEditor (juce::AudioProcessor* p, const juce::String& title);
    void closeEditor (juce::AudioProcessor* p);
    void closeAllEditors();

    static juce::String formatShortName (const juce::String& formatName);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void saveList();

    class PluginWindow;
    juce::ApplicationProperties& properties;
    std::map<juce::AudioProcessor*, std::unique_ptr<PluginWindow>> windows;
    std::unique_ptr<juce::DocumentWindow> managerWindow;
};

} // namespace artilody
