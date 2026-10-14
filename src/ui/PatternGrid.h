#pragma once

#include "AppContext.h"

namespace artilody
{

/** The dark tracker grid: draws all tracks' note columns and handles editing. */
class PatternGrid final : public juce::Component, private juce::Timer
{
public:
    explicit PatternGrid (AppContext&);
    ~PatternGrid() override;

    void updateLayout();
    int getTrackX (int track) const;
    int getTrackWidth (int track) const;
    int getTotalWidth() const;

    int getFirstVisibleRow() const;
    int getNumVisibleRows() const { return juce::jmax (1, getHeight() / Layout::rowHeight); }
    int getDisplayedPlayRow() const;

    void enterMidiNote (int note, int velocity);
    void moveCursorRow (int delta);
    void setCursor (int track, int column, int field, int row);

    std::function<void()> onViewChanged;          // scroll / cursor moved
    std::function<void()> onCursorTrackChanged;
    std::function<void (int track)> onRequestTrackVisible;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override   { repaint(); }

    // editing commands (also used by menus)
    void copySelection (bool cut);
    void paste();
    void deleteSelectionOrCell();
    void transpose (int semitones);
    void interpolateVolumes();
    void selectAllInTrack();

private:
    struct HitResult { int track = -1, column = 0, field = 0, row = 0; };
    HitResult hitTest (juce::Point<int>) const;
    void timerCallback() override;

    float charWidth() const;
    int fieldCharOffset (int field) const;
    int fieldCharWidth (int field) const;

    void moveCursorField (int delta);
    void moveCursorTrack (int delta);
    void afterCursorMove (bool extendSelection);
    void notifyCursorTrack (int oldTrack);

    bool handleNoteKey (const juce::KeyPress&);
    bool handleHexKey (const juce::KeyPress&);
    void writeNote (int note, int velocity, bool advance);
    void previewNote (int note, float velocity);

    void insertRow();
    void deleteRow();

    struct ColRef { int track, column; };
    std::vector<ColRef> selectedColumns() const;
    void getSelectionRows (int& r0, int& r1) const;
    bool isSelected (int track, int column, int row) const;

    AppContext& ctx;
    std::vector<int> trackX;
    std::vector<int> trackW;
    int totalWidth = 0;

    int lastPlayRow = -1, lastPlaySeq = -1;
    int dragAnchorTrack = 0, dragAnchorCol = 0, dragAnchorRow = 0;

    struct HeldKey { int keyCode; int note; int track; };
    std::vector<HeldKey> heldKeys;

    std::vector<std::vector<Cell>> clipboard; // [column][row]

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternGrid)
};

} // namespace artilody
