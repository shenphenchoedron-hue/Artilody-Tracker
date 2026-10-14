#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace artilody
{

namespace Theme
{
    // light chrome
    const juce::Colour background   { 0xffedf1f7 };
    const juce::Colour panel        { 0xffffffff };
    const juce::Colour panelAlt     { 0xfff5f7fb };
    const juce::Colour border       { 0xffdde4ee };
    const juce::Colour text         { 0xff16233b };
    const juce::Colour textDim      { 0xff6a7891 };
    const juce::Colour textFaint    { 0xffa3aec0 };
    const juce::Colour accent       { 0xff1fc4b4 };
    const juce::Colour accentBlue   { 0xff2f6fed };
    const juce::Colour selection    { 0xffe3edff };
    const juce::Colour record       { 0xffff4d5e };

    // dark pattern editor
    const juce::Colour gridBg       { 0xff19212f };
    const juce::Colour gridRow      { 0xff1e2737 };
    const juce::Colour gridRowAlt   { 0xff1b2433 };
    const juce::Colour gridBeat     { 0xff242f43 };
    const juce::Colour gridHeader   { 0xff151c28 };
    const juce::Colour gridLine     { 0xff2b364a };
    const juce::Colour gridText     { 0xffd3dbea };
    const juce::Colour gridDim      { 0xff47546b };
    const juce::Colour gridCursor   { 0xff33415b };

    juce::Font uiFont (float height, int weight = 400);
    juce::Font monoFont (float height, bool bold = false);

    juce::Image logoFull();
    juce::Image logoMark();

    void drawCard (juce::Graphics&, juce::Rectangle<float> r, float radius = 10.0f);
    void drawShadow (juce::Graphics&, juce::Rectangle<float> r, float radius);

    /** Simple gradient badge icon derived from a name (used for instrument thumbnails). */
    void drawInstrumentIcon (juce::Graphics&, juce::Rectangle<float> r, const juce::String& seed, juce::Colour base);

    enum class Icon { Play, Stop, Record, Rewind, Plus, Dots, Gear, Save, Folder, Chevron, ChevronUp, ChevronDown, Close, Power, Mixer, Plug, Export, Grid, Follow, Undo, Redo, Loop };
    void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float> r, juce::Colour c);
}

//==============================================================================
class ArtilodyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ArtilodyLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    int getDefaultScrollbarWidth() override { return 10; }

    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
};

//==============================================================================
/** Flat icon button used throughout the UI. */
class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& name, Theme::Icon i) : juce::Button (name), icon (i) {}

    Theme::Icon icon;
    juce::Colour fill { Theme::panelAlt }, iconColour { Theme::text }, onFill { Theme::accent }, onIcon { juce::Colours::white };
    float radius = 8.0f;
    bool drawBackground = true;

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

} // namespace artilody
