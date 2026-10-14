#pragma once

#include "AppContext.h"
#include "CellPickers.h"

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

    /** MIDI keyboard input (edit mode): writes notes at the cursor; held keys form chords across note columns. */
    void enterMidiNote (int note, int velocity, bool isNoteOn);
    void moveCursorRow (int delta);
    /** Scrolls the view (moves the cursor) without wrapping around at the first / last row. */
    void scrollRows (int delta);
    void setCursor (int track, int column, int field, int row);

    std::function<void()> onViewChanged;          // scroll / cursor moved
    std::function<void()> onCursorTrackChanged;
    std::function<void (int track)> onRequestTrackVisible;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
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

    bool handleHexKey (const juce::KeyPress&);
    void writeNote (int note, int velocity, bool advance);
    int previewInstrument() const;
    /** Opens the pop-up editor for the cursor's field (note, instrument, volume or effect). */
    void showFieldPicker (int field, juce::Rectangle<int> screenArea);
    void showNotePicker (juce::Rectangle<int> screenArea);
    /** Applies an edit to the cursor cell as one undo step and moves down by the edit step. */
    void editCursorCell (const juce::String& undoName, const std::function<void (Cell&)>& edit);

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

    std::vector<int> midiHeld;                 // MIDI notes currently held (for chord entry)
    int chordRow = -1, chordTrack = -1, chordNextColumn = 0;

    std::vector<std::vector<Cell>> clipboard; // [column][row]

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternGrid)
};

} // namespace artilody
