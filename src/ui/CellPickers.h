#pragma once

#include "../Theme.h"

namespace artilody
{

/** Common frame for the small pop-ups that edit one field of a pattern cell
    (title + close button, a subtitle row with a live readout, a body and a footer with
    [left action] ... [Cancel] [Insert]). Return = insert, Esc = close. */
class CellPickerBase : public juce::Component
{
public:
    CellPickerBase (const juce::String& title, const juce::String& subtitle, const juce::String& leftButtonText);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void parentHierarchyChanged() override;

protected:
    virtual void layoutBody (juce::Rectangle<int> body) = 0;
    virtual void paintBody (juce::Graphics&) {}
    virtual juce::String getReadout() const = 0;
    virtual void confirm() = 0;          // Insert / Return
    virtual void leftAction() = 0;       // Clear / Note off
    virtual bool bodyKeyPressed (const juce::KeyPress&) { return false; }
    virtual void closing() {}

    void close();
    juce::TextButton& getInsertButton() { return insertButton; }

    juce::Rectangle<int> bodyArea;

private:
    juce::String title, subtitle;
    juce::Rectangle<int> titleArea, subtitleArea;
    IconButton closeButton { "close", Theme::Icon::Close };
    juce::TextButton leftButton, cancelButton { "Cancel" }, insertButton { "Insert" };
};

//==============================================================================
/** Note field: octave buttons (1-8) and a one-octave keyboard. */
class NotePicker final : public CellPickerBase
{
public:
    static constexpr int minOctave = 1, maxOctave = 8;

    /** initialNote < 0 selects C in initialOctave. */
    NotePicker (int initialNote, int initialOctave);

    std::function<void (int note)> onInsert;                  // a note number, or -2 for note off
    std::function<void (int note, bool isDown)> onPreview;    // audition while a key is held

    int getSelectedNote() const { return octave * 12 + semitone; }

    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct Key { int semitone; bool black; juce::Rectangle<float> bounds; };

    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&) override;
    juce::String getReadout() const override;
    void confirm() override;
    void leftAction() override;
    bool bodyKeyPressed (const juce::KeyPress&) override;
    void closing() override { stopPreview(); }

    int keyAt (juce::Point<float>) const;
    void setOctave (int);
    void stopPreview();

    int octave = 4, semitone = 0, previewing = -1;
    juce::Rectangle<int> keyboardArea;
    std::vector<Key> keys;
    juce::OwnedArray<juce::TextButton> octaveButtons;
};

//==============================================================================
/** Ins field: pick which track's instrument plays the note. */
class InstrumentPicker final : public CellPickerBase
{
public:
    struct Entry { juce::String trackName, instrumentName; juce::Colour colour; };

    /** current: 1-based track number or 0 for empty; ownTrack: 1-based track of the cell. */
    InstrumentPicker (std::vector<Entry> tracks, int current, int ownTrack);
    ~InstrumentPicker() override;

    std::function<void (int ins)> onInsert;   // 1-based track number, or 0 = clear (track's own instrument)

private:
    class List;
    void layoutBody (juce::Rectangle<int>) override;
    juce::String getReadout() const override;
    void confirm() override;
    void leftAction() override;
    bool bodyKeyPressed (const juce::KeyPress&) override;

    std::vector<Entry> entries;
    int selected = 0, own = 1;   // 0-based selection
    juce::Viewport viewport;
    std::unique_ptr<List> list;
};

//==============================================================================
/** Vol field: a volume slider (00-7F) with dynamics presets. */
class VolumePicker final : public CellPickerBase
{
public:
    /** current: 0-127, or < 0 for empty. */
    explicit VolumePicker (int current);

    std::function<void (int vol)> onInsert;   // 0-127, or -1 = clear (default velocity)

private:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&) override;
    juce::String getReadout() const override;
    void confirm() override;
    void leftAction() override;
    bool bodyKeyPressed (const juce::KeyPress&) override;

    juce::Slider slider;
    juce::TextButton minus { "-" }, plus { "+" };
    juce::OwnedArray<juce::TextButton> presets;
    juce::Rectangle<int> meterArea;
};

//==============================================================================
/** FX field: choose an effect command and set its value with a control that fits the command. */
class EffectPicker final : public CellPickerBase
{
public:
    /** cmd 0 = none. */
    EffectPicker (char cmd, int value);

    std::function<void (char cmd, int value)> onInsert;   // cmd 0 = clear

private:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&) override;
    juce::String getReadout() const override;
    void confirm() override;
    void leftAction() override;

    bool bodyKeyPressed (const juce::KeyPress&) override;
    void selectCommand (int index);
    void updateValueControls();
    int currentValue() const;
    juce::String describeValue() const;

    struct Cmd { char letter; const char* name; int minV, maxV, defaultV; };
    static const std::vector<Cmd>& commands();

    int cmdIndex = 0;
    juce::OwnedArray<juce::TextButton> cmdButtons;
    juce::Slider valueSlider, retrigSpeed, retrigFall;
    juce::TextButton minus { "-" }, plus { "+" };
    juce::Rectangle<int> descArea, valueLabelArea;
};

} // namespace artilody
