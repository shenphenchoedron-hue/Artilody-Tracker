#pragma once

#include "AppContext.h"

namespace artilody
{

class PatternGrid;

/** Left column: the song sequence (order list). */
class SequenceList final : public juce::Component, private juce::Timer
{
public:
    explicit SequenceList (AppContext&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void selectSlot (int slot);
    void insertNewPattern();

private:
    void timerCallback() override;
    int slotAt (juce::Point<int>) const;
    void showSlotMenu (int slot);

    AppContext& ctx;
    IconButton addButton { "add", Theme::Icon::Plus };
    int scroll = 0;
    int lastPlaying = -1;
    static constexpr int rowH = 30, headerH = 36;
};

/** Row numbers next to the pattern grid (follows the grid's vertical scroll). */
class RowNumbers final : public juce::Component
{
public:
    RowNumbers (AppContext&, PatternGrid&);
    void paint (juce::Graphics&) override;

private:
    AppContext& ctx;
    PatternGrid& grid;
};

/** Right column: pattern list and pattern properties. */
class RightPanel final : public juce::Component
{
public:
    explicit RightPanel (AppContext&);
    ~RightPanel() override;

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class PatternList;

    AppContext& ctx;
    std::unique_ptr<PatternList> patternList;
    IconButton addPatternButton { "add", Theme::Icon::Plus };
    IconButton patternMenuButton { "menu", Theme::Icon::Dots };

    juce::TextEditor nameEditor, notesEditor;
    juce::ComboBox lengthBox;
    juce::OwnedArray<juce::TextButton> colourButtons;

    juce::Rectangle<int> patternsCard, propsCard, helpCard;
    bool updating = false;
};

} // namespace artilody
