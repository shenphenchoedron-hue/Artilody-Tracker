#pragma once

#include "AppContext.h"
#include "TopBar.h"
#include "TrackArea.h"
#include "SidePanels.h"

namespace artilody
{

class MainComponent final : public juce::Component,
                            private juce::AsyncUpdater,
                            private juce::ChangeListener
{
public:
    explicit MainComponent (juce::ApplicationProperties&);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void requestQuit (std::function<void (bool okToQuit)>);
    std::function<void (const juce::String&)> onTitleChanged;

    // commands
    void newProject();
    void openProject();
    void saveProject (bool saveAs, std::function<void (bool)> done = {});
    void exportWav();
    void showAudioSettings();
    void showMainMenu();
    void addTrack();
    void playSong (bool fromStart);
    void playPattern();
    void stopPlayback();
    void loadFile (const juce::File&);
    bool renderTo (const juce::File&);
    bool roundTripTest (const juce::File&);
    bool saveJazzDemo (const juce::File& projectFile, const juce::File& wavFile);

private:
    void handleAsyncUpdate() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void structureChanged();
    void patternChanged();
    void selectTrack (int t);
    void scrollToTrack (int t);
    void updateTitle();
    void chooseDefaultSoundFont();
    void showAbout();
    void resetEditorState();

    juce::ApplicationProperties& properties;
    AudioEngine engine;
    AppContext ctx { engine };

    juce::TooltipWindow tooltips { this, 600 };
    TopBar topBar { ctx };
    SequenceList sequenceList { ctx };
    juce::Viewport trackViewport;
    TrackArea trackArea { ctx };
    RowNumbers rowNumbers { ctx, trackArea.grid };
    std::unique_ptr<MixerStrip> masterStrip;
    RightPanel rightPanel { ctx };

    juce::TextButton addTrackButton { "+  Add Track" }, mixerButton { "Hide Mixer" }, fxListsButton { "Collapse FX lists" };
    juce::TextButton colsMinus { "-" }, colsPlus { "+" };
    IconButton mixerCollapseButton { "mixer", Theme::Icon::ChevronDown };
    juce::Rectangle<int> toolbarArea, patternInfoArea, colsLabelArea;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastDirectory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace artilody
