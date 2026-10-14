#pragma once

#include "../BuiltinFX.h"
#include "../Theme.h"

namespace artilody
{

/** Interactive parametric EQ editor.

    - Click on the curve (or double-click anywhere) to add a band, then drag it:
      left/right = frequency, up/down = gain. Hold Shift for fine adjustment.
    - Mouse wheel over a band changes its Q (bandwidth). Double-click a band to delete it.
    - Right-click a band to change its type (bell, shelves, cuts, notch).
    - The row under the graph shows the selected band with exact, editable values.
*/
class EQEditor final : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    EQEditor (BuiltinEQ&, juce::Colour accent, std::function<void()> onChange);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static juce::Colour bandColour (int band);

private:
    // coordinate mapping
    float freqToX (float f) const;
    float xToFreq (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;
    juce::Point<float> handlePosition (int band) const;
    int bandAt (juce::Point<float>) const;
    bool isNearCurve (juce::Point<float>) const;

    void addBandAt (juce::Point<float>);
    void selectBand (int band);
    void changed();
    void enableIfEdited();   // switches the EQ on when a band is edited
    void updateControls();
    void showBandMenu (int band);
    void updateCurveCache();   // rebuilds the curve paths only when a band or the size changed

    struct CurveCache
    {
        bool valid = false;
        juce::Rectangle<float> plot;
        double sampleRate = 0;
        std::array<BuiltinEQ::BandSettings, BuiltinEQ::kMaxBands> key;
        std::array<juce::Path, BuiltinEQ::kMaxBands> bandPaths;
        juce::Path total;
    } cache;

    BuiltinEQ& eq;
    juce::Colour accent;
    std::function<void()> onChange;

    juce::ToggleButton enable { "Enabled" };
    juce::TextButton resetButton { "Reset" };

    juce::Rectangle<int> graphArea, controlsArea;
    juce::Rectangle<float> plot;   // the area inside the graph used for the curve

    int selected = -1, hovered = -1, dragging = -1;
    int justAdded = -1;            // band created by the current click (so a double-click does not delete it)
    juce::uint32 justAddedTime = 0;
    juce::Point<float> dragStartMouse;
    BuiltinEQ::BandSettings dragStartBand;
    juce::Point<float> hoverPos;
    bool hoverInPlot = false;
    bool nearCurve = false;         // mouse is over the line (shows the "+" marker)
    juce::Point<float> markerPos;

    juce::ComboBox typeBox;
    juce::Slider freqSlider, gainSlider, qSlider;
    juce::TextButton deleteButton { "Delete band" };
    juce::Label hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQEditor)
};

} // namespace artilody
