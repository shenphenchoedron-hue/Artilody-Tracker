#pragma once

#include "AppContext.h"

namespace artilody
{

class LevelMeter final : public juce::Component
{
public:
    void setLevels (float l, float r);
    void paint (juce::Graphics&) override;
    bool horizontal = false;

private:
    float levelL = 0.0f, levelR = 0.0f, holdL = 0.0f, holdR = 0.0f;
};

/** Callout editor for the built-in EQ / Compressor / Reverb. */
class BuiltinEditor final : public juce::Component, private juce::Timer
{
public:
    BuiltinEditor (BuiltinEffect&, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    BuiltinEffect& fx;
    juce::Colour accent;
    juce::ToggleButton enable { "Enabled" };
    juce::TextButton resetButton { "Reset" };
    juce::OwnedArray<juce::Slider> knobs;
    juce::Rectangle<int> curveArea;
};

/** One mixer channel strip (track or master). */
class MixerStrip final : public juce::Component, private juce::Timer
{
public:
    MixerStrip (AppContext&, StripProcessor&, int trackIndex);
    ~MixerStrip() override;

    void setTrackIndex (int t) { track = t; repaint(); }
    int getTrackIndex() const  { return track; }
    StripProcessor& getStrip() { return strip; }

    void setCompact (bool);
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    std::function<void()> onLayoutChanged;

    // actions
    void showInstrumentMenu();
    void showAddFxMenu();
    void showTrackMenu();
    void chooseAudioSample();
    bool loadAudioSample (const juce::File&);

private:
    class InstrumentButton;
    class FxPanel;

    void timerCallback() override;
    juce::Colour colour() const;
    juce::String title() const;
    juce::Rectangle<int> headerArea() const { return getLocalBounds().removeFromTop (30); }

    void loadSoundFontPreset (const juce::String& genre, int bank, int program, bool drums, const juce::String& name, std::shared_ptr<SoundFontFile> font);
    void loadPluginInstrument (const juce::PluginDescription&);
    void addPluginFx (const juce::PluginDescription&);
    void renameTrack();

    AppContext& ctx;
    StripProcessor& strip;
    int track;
    bool compact = false;

    IconButton menuButton { "menu", Theme::Icon::Dots };
    std::unique_ptr<InstrumentButton> instrumentButton;
    std::unique_ptr<FxPanel> fxPanel;
    juce::Viewport fxViewport;
    juce::Slider fader { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider panKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider compactFader { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    LevelMeter meter, compactMeter;
    juce::TextButton muteButton { "M" }, soloButton { "S" };
    juce::Label dbLabel;
    juce::Rectangle<int> dbRow;

    juce::Array<juce::PluginDescription> menuPlugins;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerStrip)
};

} // namespace artilody
