#include "CellPickers.h"
#include "../Song.h"

namespace artilody
{

static juce::String hex2 (int v) { return juce::String::toHexString (v & 0xFF).paddedLeft ('0', 2).toUpperCase(); }

static void styleTextButton (juce::TextButton& b)
{
    b.setWantsKeyboardFocus (false);
}

//==============================================================================
CellPickerBase::CellPickerBase (const juce::String& t, const juce::String& sub, const juce::String& leftText)
    : title (t), subtitle (sub), leftButton (leftText)
{
    closeButton.setTooltip ("Close (Esc)");
    closeButton.onClick = [this] { close(); };
    closeButton.setWantsKeyboardFocus (false);
    addAndMakeVisible (closeButton);

    leftButton.onClick = [this] { leftAction(); };
    cancelButton.onClick = [this] { close(); };
    insertButton.setColour (juce::TextButton::buttonColourId, Theme::accentBlue);
    insertButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    insertButton.setTooltip ("Insert (Return)");
    insertButton.onClick = [this] { confirm(); };
    for (auto* b : { &leftButton, &cancelButton, &insertButton })
    {
        styleTextButton (*b);
        addAndMakeVisible (b);
    }
    setWantsKeyboardFocus (true);
}

void CellPickerBase::parentHierarchyChanged()
{
    // take keyboard focus once on screen so Return / Esc / arrows work straight away
    juce::Component::SafePointer<CellPickerBase> safe (this);
    juce::MessageManager::callAsync ([safe] { if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus(); });
}

void CellPickerBase::resized()
{
    auto r = getLocalBounds().reduced (16, 14);
    auto top = r.removeFromTop (32);
    closeButton.setBounds (top.removeFromRight (32));
    titleArea = top;
    r.removeFromTop (6);
    subtitleArea = r.removeFromTop (24);
    r.removeFromTop (8);

    auto footer = r.removeFromBottom (36);
    insertButton.setBounds (footer.removeFromRight (124));
    footer.removeFromRight (8);
    cancelButton.setBounds (footer.removeFromRight (90));
    leftButton.setBounds (footer.removeFromLeft (90));
    r.removeFromBottom (12);

    bodyArea = r;
    layoutBody (r);
}

void CellPickerBase::paint (juce::Graphics& g)
{
    g.setColour (Theme::text);
    g.setFont (Theme::uiFont (18.0f, 700));
    g.drawText (title, titleArea, juce::Justification::centredLeft);

    g.setColour (Theme::textFaint);
    g.setFont (Theme::uiFont (14.0f, 600));
    g.drawText (subtitle, subtitleArea, juce::Justification::centredLeft);

    g.setColour (Theme::accent.darker (0.35f));
    g.setFont (Theme::monoFont (20.0f, true));
    g.drawText (getReadout(), subtitleArea, juce::Justification::centredRight);

    paintBody (g);
}

bool CellPickerBase::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::returnKey) { confirm(); return true; }
    if (key.getKeyCode() == juce::KeyPress::escapeKey) { close(); return true; }
    return bodyKeyPressed (key);
}

void CellPickerBase::close()
{
    closing();
    if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
        box->dismiss();
}

//==============================================================================
// NotePicker
//==============================================================================
static const char* const kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static bool isBlackKey (int s) { return s == 1 || s == 3 || s == 6 || s == 8 || s == 10; }

NotePicker::NotePicker (int initialNote, int initialOctave)
    : CellPickerBase ("Choose note", "Octave", "Note off")
{
    if (initialNote >= 0) { octave = juce::jlimit (minOctave, maxOctave, initialNote / 12); semitone = initialNote % 12; }
    else                  { octave = juce::jlimit (minOctave, maxOctave, initialOctave); semitone = 0; }

    for (int o = minOctave; o <= maxOctave; ++o)
    {
        auto* b = octaveButtons.add (new juce::TextButton (juce::String (o)));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4711);
        b->setColour (juce::TextButton::buttonOnColourId, Theme::accent);
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        b->setToggleState (o == octave, juce::dontSendNotification);
        b->onClick = [this, o] { setOctave (o); };
        styleTextButton (*b);
        addAndMakeVisible (b);
    }
    getInsertButton().setButtonText ("Insert note");
    getInsertButton().setTooltip ("Insert the selected note (Return). Tip: double-click a key to insert it directly.");
    setSize (380, 300);
}

void NotePicker::layoutBody (juce::Rectangle<int> r)
{
    auto ob = r.removeFromTop (34);
    const int gap = 6;
    const int w = (ob.getWidth() - gap * (octaveButtons.size() - 1)) / octaveButtons.size();
    for (auto* b : octaveButtons) { b->setBounds (ob.removeFromLeft (w)); ob.removeFromLeft (gap); }
    r.removeFromTop (12);
    keyboardArea = r;

    keys.clear();
    auto area = keyboardArea.toFloat().reduced (5.0f);
    const float ww = area.getWidth() / 7.0f, bw = ww * 0.62f, bh = area.getHeight() * 0.6f;
    int white = 0;
    for (int s = 0; s < 12; ++s)
        if (! isBlackKey (s))
            keys.push_back ({ s, false, { area.getX() + ww * (float) white++, area.getY(), ww, area.getHeight() } });
    white = 0;
    for (int s = 0; s < 12; ++s)
    {
        if (! isBlackKey (s)) { ++white; continue; }
        keys.push_back ({ s, true, { area.getX() + ww * (float) white - bw * 0.5f, area.getY(), bw, bh } });
    }
}

juce::String NotePicker::getReadout() const { return noteToString (getSelectedNote()); }

int NotePicker::keyAt (juce::Point<float> p) const
{
    for (auto it = keys.rbegin(); it != keys.rend(); ++it)   // black keys are on top (last)
        if (it->bounds.contains (p))
            return it->semitone;
    return -1;
}

void NotePicker::paintBody (juce::Graphics& g)
{
    g.setColour (Theme::gridBg);
    g.fillRoundedRectangle (keyboardArea.toFloat(), 8.0f);

    const auto selColour = Theme::accent.withSaturation (0.55f).brighter (0.2f);
    for (auto& k : keys)
    {
        if (k.black) continue;
        auto b = k.bounds.reduced (1.5f, 0.0f);
        const bool sel = k.semitone == semitone, down = k.semitone == previewing;
        g.setColour (sel ? selColour : (down ? Theme::selection : juce::Colours::white));
        g.fillRoundedRectangle (b, 4.0f);
        if (sel)
        {
            g.setColour (selColour.darker (0.15f));
            g.fillRoundedRectangle (b.removeFromBottom (5.0f), 3.0f);
        }
        g.setColour (sel ? Theme::text : Theme::textDim);
        g.setFont (Theme::uiFont (15.0f, 500));
        g.drawText (kNoteNames[k.semitone], k.bounds.withTrimmedBottom (8.0f), juce::Justification::centredBottom);
    }
    for (auto& k : keys)
    {
        if (! k.black) continue;
        const bool sel = k.semitone == semitone, down = k.semitone == previewing;
        g.setColour (sel ? Theme::accent.darker (0.1f) : (down ? Theme::gridCursor : juce::Colour (0xff2a3446)));
        g.fillRoundedRectangle (k.bounds, 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (k.bounds, 4.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (Theme::uiFont (13.0f, 600));
        g.drawText (kNoteNames[k.semitone], k.bounds.withTrimmedBottom (10.0f), juce::Justification::centredBottom);
    }
}

void NotePicker::setOctave (int o)
{
    octave = juce::jlimit (minOctave, maxOctave, o);
    for (int i = 0; i < octaveButtons.size(); ++i)
        octaveButtons[i]->setToggleState (minOctave + i == octave, juce::dontSendNotification);
    repaint();
}

void NotePicker::mouseDown (const juce::MouseEvent& e)
{
    const int s = keyAt (e.position);
    if (s < 0) return;
    semitone = s;
    stopPreview();
    previewing = s;
    if (onPreview) onPreview (getSelectedNote(), true);
    repaint();
}

void NotePicker::mouseUp (const juce::MouseEvent&)        { stopPreview(); repaint(); }
void NotePicker::mouseDoubleClick (const juce::MouseEvent& e) { if (keyAt (e.position) >= 0) confirm(); }

void NotePicker::stopPreview()
{
    if (previewing >= 0 && onPreview)
        onPreview (octave * 12 + previewing, false);
    previewing = -1;
}

bool NotePicker::bodyKeyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::leftKey)  { semitone = (semitone + 11) % 12; repaint(); return true; }
    if (code == juce::KeyPress::rightKey) { semitone = (semitone + 1) % 12; repaint(); return true; }
    if (code == juce::KeyPress::upKey)    { setOctave (octave + 1); return true; }
    if (code == juce::KeyPress::downKey)  { setOctave (octave - 1); return true; }
    return false;
}

void NotePicker::confirm()    { stopPreview(); if (onInsert) onInsert (getSelectedNote()); close(); }
void NotePicker::leftAction() { stopPreview(); if (onInsert) onInsert (kNoteOff); close(); }

//==============================================================================
// InstrumentPicker
//==============================================================================
class InstrumentPicker::List final : public juce::Component
{
public:
    explicit List (InstrumentPicker& o) : owner (o) {}
    static constexpr int rowH = 40;

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < (int) owner.entries.size(); ++i)
        {
            auto& e = owner.entries[(size_t) i];
            auto r = juce::Rectangle<int> (0, i * rowH, getWidth(), rowH).reduced (0, 2).toFloat();
            const bool sel = i == owner.selected;
            g.setColour (sel ? Theme::selection : (i == hover ? Theme::panelAlt : Theme::panel));
            g.fillRoundedRectangle (r, 7.0f);
            if (sel)
            {
                g.setColour (Theme::accentBlue.withAlpha (0.6f));
                g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.2f);
            }

            auto badge = r.removeFromLeft (44).withSizeKeepingCentre (32, 24);
            g.setColour (e.colour);
            g.fillRoundedRectangle (badge, 6.0f);
            g.setColour (juce::Colours::white);
            g.setFont (Theme::monoFont (13.0f, true));
            g.drawText (hex2 (i + 1), badge, juce::Justification::centred);

            r.removeFromLeft (4);
            g.setColour (Theme::text);
            g.setFont (Theme::uiFont (13.5f, 600));
            g.drawFittedText (e.trackName + (i + 1 == owner.own ? "  (this track)" : ""),
                              r.removeFromTop (r.getHeight() * 0.55f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.9f);
            g.setColour (Theme::textDim);
            g.setFont (Theme::uiFont (12.0f));
            g.drawFittedText (e.instrumentName, r.toNearestInt(), juce::Justification::topLeft, 1, 0.9f);
        }
    }

    int rowAt (juce::Point<int> p) const { const int r = p.y / rowH; return juce::isPositiveAndBelow (r, (int) owner.entries.size()) ? r : -1; }
    void mouseMove (const juce::MouseEvent& e) override { const int h = rowAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { const int r = rowAt (e.getPosition()); if (r >= 0) { owner.selected = r; owner.repaint(); repaint(); } }
    void mouseDoubleClick (const juce::MouseEvent& e) override { if (rowAt (e.getPosition()) >= 0) owner.confirm(); }

    InstrumentPicker& owner;
    int hover = -1;
};

InstrumentPicker::InstrumentPicker (std::vector<Entry> tracks, int current, int ownTrack)
    : CellPickerBase ("Choose instrument", "Plays on track", "Clear"), entries (std::move (tracks)), own (ownTrack)
{
    selected = juce::jlimit (0, juce::jmax (0, (int) entries.size() - 1), (current >= 1 ? current : ownTrack) - 1);
    list = std::make_unique<List> (*this);
    viewport.setViewedComponent (list.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    getInsertButton().setButtonText ("Use instrument");
    setSize (380, 140 + juce::jlimit (2, 7, (int) entries.size()) * List::rowH + 8);
}

InstrumentPicker::~InstrumentPicker()
{
    viewport.setViewedComponent (nullptr, false);
}

void InstrumentPicker::layoutBody (juce::Rectangle<int> r)
{
    viewport.setBounds (r);
    list->setSize (r.getWidth() - (entries.size() * List::rowH > (size_t) r.getHeight() ? 10 : 0), (int) entries.size() * List::rowH);
    viewport.setViewPosition (0, juce::jmax (0, selected * List::rowH - r.getHeight() / 2));
}

juce::String InstrumentPicker::getReadout() const { return entries.empty() ? juce::String ("--") : hex2 (selected + 1); }

void InstrumentPicker::confirm()    { if (onInsert && ! entries.empty()) onInsert (selected + 1); close(); }
void InstrumentPicker::leftAction() { if (onInsert) onInsert (0); close(); }

bool InstrumentPicker::bodyKeyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    if (code != juce::KeyPress::upKey && code != juce::KeyPress::downKey) return false;
    selected = juce::jlimit (0, juce::jmax (0, (int) entries.size() - 1), selected + (code == juce::KeyPress::upKey ? -1 : 1));
    const int y = selected * List::rowH;
    if (y < viewport.getViewPositionY()) viewport.setViewPosition (0, y);
    else if (y + List::rowH > viewport.getViewPositionY() + viewport.getHeight()) viewport.setViewPosition (0, y + List::rowH - viewport.getHeight());
    list->repaint();
    repaint();
    return true;
}

//==============================================================================
// VolumePicker
//==============================================================================
VolumePicker::VolumePicker (int current)
    : CellPickerBase ("Volume", "Velocity", "Clear")
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (0, 127, 1);
    slider.setValue (current >= 0 ? current : 0x66, juce::dontSendNotification);
    slider.setDoubleClickReturnValue (true, 0x66);
    slider.setColour (juce::Slider::trackColourId, Theme::accent);
    slider.setColour (juce::Slider::thumbColourId, Theme::accent);
    slider.setWantsKeyboardFocus (false);
    slider.onValueChange = [this] { repaint(); };
    slider.setTooltip ("Drag to set the volume (Shift-drag for fine steps, double-click = default 66)");
    slider.setVelocityModeParameters (0.4, 1, 0.02, true, juce::ModifierKeys::shiftModifier);
    addAndMakeVisible (slider);

    for (auto* b : { &minus, &plus })
    {
        styleTextButton (*b);
        addAndMakeVisible (b);
    }
    minus.onClick = [this] { slider.setValue (slider.getValue() - 1); };
    plus.onClick  = [this] { slider.setValue (slider.getValue() + 1); };

    struct P { const char* mark; int v; };
    for (auto p : { P { "pp", 0x20 }, P { "p", 0x38 }, P { "mf", 0x50 }, P { "f", 0x66 }, P { "ff", 0x7F } })
    {
        auto* b = presets.add (new juce::TextButton (juce::String (p.mark) + "  " + hex2 (p.v)));
        const int v = p.v;
        b->onClick = [this, v] { slider.setValue (v); };
        b->setTooltip ("Set volume to " + hex2 (v));
        styleTextButton (*b);
        addAndMakeVisible (b);
    }
    getInsertButton().setButtonText ("Set volume");
    setSize (400, 300);
}

void VolumePicker::layoutBody (juce::Rectangle<int> r)
{
    meterArea = r.removeFromTop (54);
    r.removeFromTop (8);
    auto row = r.removeFromTop (34);
    minus.setBounds (row.removeFromLeft (34));
    plus.setBounds (row.removeFromRight (34));
    slider.setBounds (row.reduced (8, 0));
    r.removeFromTop (12);
    auto pr = r.removeFromTop (32);
    const int gap = 6;
    const int w = (pr.getWidth() - gap * (presets.size() - 1)) / presets.size();
    for (auto* b : presets) { b->setBounds (pr.removeFromLeft (w)); pr.removeFromLeft (gap); }
}

void VolumePicker::paintBody (juce::Graphics& g)
{
    // big level bar with the value
    auto m = meterArea.toFloat();
    g.setColour (Theme::gridBg);
    g.fillRoundedRectangle (m, 8.0f);
    const float t = (float) slider.getValue() / 127.0f;
    auto fill = m.reduced (5.0f);
    juce::ColourGradient grad (Theme::accent, fill.getX(), 0, Theme::accentBlue, fill.getRight(), 0, false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (fill.withWidth (juce::jmax (8.0f, fill.getWidth() * t)), 5.0f);

    g.setColour (juce::Colours::white);
    g.setFont (Theme::monoFont (22.0f, true));
    g.drawText (hex2 ((int) slider.getValue()), m.reduced (14.0f, 0), juce::Justification::centredLeft);
    g.setFont (Theme::uiFont (14.0f, 600));
    g.drawText (juce::String (juce::roundToInt (t * 100.0f)) + " %   " + (t > 0.001f ? juce::String (20.0f * std::log10 (t), 1) + " dB" : juce::String ("-inf dB")),
                m.reduced (14.0f, 0), juce::Justification::centredRight);
}

juce::String VolumePicker::getReadout() const { return hex2 ((int) slider.getValue()); }

bool VolumePicker::bodyKeyPressed (const juce::KeyPress& key)
{
    const int step = key.getModifiers().isShiftDown() ? 16 : 1;
    if (key.getKeyCode() == juce::KeyPress::leftKey  || key.getKeyCode() == juce::KeyPress::downKey) { slider.setValue (slider.getValue() - step); return true; }
    if (key.getKeyCode() == juce::KeyPress::rightKey || key.getKeyCode() == juce::KeyPress::upKey)   { slider.setValue (slider.getValue() + step); return true; }
    return false;
}

void VolumePicker::confirm()    { if (onInsert) onInsert ((int) slider.getValue()); close(); }
void VolumePicker::leftAction() { if (onInsert) onInsert (-1); close(); }

//==============================================================================
// EffectPicker
//==============================================================================
const std::vector<EffectPicker::Cmd>& EffectPicker::commands()
{
    static const std::vector<Cmd> list {
        { 'T', "Tempo",    0x20, 0xFF, 0x78 },
        { 'L', "Lines/beat", 0x01, 0x20, 0x04 },
        { 'D', "Delay",    0x00, 0xFF, 0x80 },
        { 'C', "Cut",      0x00, 0xFF, 0x80 },
        { 'R', "Retrigger", 0x00, 0xFF, 0x03 },
        { 'P', "Pitch bend", 0x00, 0xFF, 0x80 },
        { 'M', "Modulation", 0x00, 0x7F, 0x40 },
        { 'B', "Jump",     0x00, 0xFF, 0x00 },
    };
    return list;
}

EffectPicker::EffectPicker (char cmd, int value)
    : CellPickerBase ("Effect", "Command", "Clear")
{
    auto& cmds = commands();
    for (int i = 0; i < (int) cmds.size(); ++i)
    {
        auto* b = cmdButtons.add (new juce::TextButton (juce::String::charToString ((juce::juce_wchar) cmds[(size_t) i].letter) + "  " + cmds[(size_t) i].name));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4712);
        b->setColour (juce::TextButton::buttonOnColourId, Theme::accent);
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        b->onClick = [this, i] { selectCommand (i); };
        styleTextButton (*b);
        addAndMakeVisible (b);
    }

    auto setupSlider = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setColour (juce::Slider::trackColourId, Theme::accent);
        s.setColour (juce::Slider::thumbColourId, Theme::accent);
        s.setWantsKeyboardFocus (false);
        s.setVelocityModeParameters (0.4, 1, 0.02, true, juce::ModifierKeys::shiftModifier);
        s.onValueChange = [this] { repaint(); };
        addChildComponent (s);
    };
    for (auto* b : { &minus, &plus })
    {
        styleTextButton (*b);
        addChildComponent (b);
    }
    minus.onClick = [this] { valueSlider.setValue (valueSlider.getValue() - 1); };
    plus.onClick  = [this] { valueSlider.setValue (valueSlider.getValue() + 1); };
    setupSlider (valueSlider);
    setupSlider (retrigSpeed);
    setupSlider (retrigFall);
    retrigSpeed.setRange (1, 15, 1);
    retrigFall.setRange (0, 15, 1);

    int idx = 0;
    for (int i = 0; i < (int) cmds.size(); ++i)
        if (cmds[(size_t) i].letter == cmd) idx = i;
    const bool known = cmd != 0 && cmds[(size_t) idx].letter == cmd;

    cmdIndex = idx;
    cmdButtons[idx]->setToggleState (true, juce::dontSendNotification);
    const int v = known ? value : cmds[(size_t) idx].defaultV;
    valueSlider.setRange (cmds[(size_t) idx].minV, cmds[(size_t) idx].maxV, 1);
    valueSlider.setValue (v, juce::dontSendNotification);
    retrigSpeed.setValue (juce::jmax (1, v & 0x0F), juce::dontSendNotification);
    retrigFall.setValue ((v >> 4) & 0x0F, juce::dontSendNotification);
    updateValueControls();

    getInsertButton().setButtonText ("Set effect");
    setSize (440, 380);
}

void EffectPicker::selectCommand (int index)
{
    cmdIndex = index;
    auto& c = commands()[(size_t) index];
    valueSlider.setRange (c.minV, c.maxV, 1);
    valueSlider.setValue (c.defaultV, juce::dontSendNotification);
    retrigSpeed.setValue (juce::jmax (1, c.defaultV & 0x0F), juce::dontSendNotification);
    retrigFall.setValue ((c.defaultV >> 4) & 0x0F, juce::dontSendNotification);
    updateValueControls();
    resized();
    repaint();
}

void EffectPicker::updateValueControls()
{
    const bool retrig = commands()[(size_t) cmdIndex].letter == 'R';
    valueSlider.setVisible (! retrig);
    minus.setVisible (! retrig);
    plus.setVisible (! retrig);
    retrigSpeed.setVisible (retrig);
    retrigFall.setVisible (retrig);
}

void EffectPicker::layoutBody (juce::Rectangle<int> r)
{
    const int cols = 4, gap = 6;
    for (int row = 0; row < 2; ++row)
    {
        auto line = r.removeFromTop (32);
        const int w = (line.getWidth() - gap * (cols - 1)) / cols;
        for (int c = 0; c < cols; ++c)
        {
            const int i = row * cols + c;
            if (i < cmdButtons.size()) cmdButtons[i]->setBounds (line.removeFromLeft (w));
            line.removeFromLeft (gap);
        }
        r.removeFromTop (gap);
    }
    r.removeFromTop (6);
    descArea = r.removeFromTop (40);
    r.removeFromTop (6);
    valueLabelArea = r.removeFromTop (20);

    if (commands()[(size_t) cmdIndex].letter == 'R')
    {
        auto a = r.removeFromTop (30);
        retrigSpeed.setBounds (a.withTrimmedLeft (110));
        r.removeFromTop (4);
        retrigFall.setBounds (r.removeFromTop (30).withTrimmedLeft (110));
    }
    else
    {
        auto row = r.removeFromTop (34);
        minus.setBounds (row.removeFromLeft (34));
        plus.setBounds (row.removeFromRight (34));
        valueSlider.setBounds (row.reduced (8, 0));
    }
}

int EffectPicker::currentValue() const
{
    if (commands()[(size_t) cmdIndex].letter == 'R')
        return (((int) retrigFall.getValue() & 0x0F) << 4) | ((int) retrigSpeed.getValue() & 0x0F);
    return (int) valueSlider.getValue();
}

juce::String EffectPicker::describeValue() const
{
    const int v = currentValue();
    switch (commands()[(size_t) cmdIndex].letter)
    {
        case 'T': return juce::String (v) + " BPM";
        case 'L': return juce::String (v) + " lines per beat";
        case 'D': return "Delay by " + juce::String (juce::roundToInt (v / 2.56)) + " % of a line";
        case 'C': return "Cut after " + juce::String (juce::roundToInt (v / 2.56)) + " % of a line";
        case 'R':
        {
            const int y = juce::jmax (1, v & 0x0F), x = (v >> 4) & 0x0F;
            const int hits = (12 - 1) / y + 1;
            return juce::String (hits) + " hits per line" + (x > 0 ? ", -" + juce::String (x) + "/16 volume per hit" : juce::String());
        }
        case 'P':
        {
            const int pct = juce::roundToInt ((v - 0x80) / 128.0 * 100.0);
            return v == 0x80 ? juce::String ("Centre (no bend)") : (pct > 0 ? "Up " : "Down ") + juce::String (std::abs (pct)) + " %";
        }
        case 'M': return "Modulation " + juce::String (juce::roundToInt (v / 1.27)) + " %";
        case 'B': return "Jump to sequence slot " + hex2 (v);
        default:  return {};
    }
}

void EffectPicker::paintBody (juce::Graphics& g)
{
    const juce::String descs[] = {
        "Changes the song tempo from this line.",
        "Changes how many lines make up one beat.",
        "Starts the note later within the line (swing, flams).",
        "Stops the note early within the line (staccato).",
        "Repeats the note within the line (rolls, stutters).",
        "Bends the pitch of the playing note.",
        "Sends the modulation wheel (vibrato on most instruments).",
        "Jumps to another place in the song after this line.",
    };
    g.setColour (Theme::textDim);
    g.setFont (Theme::uiFont (13.0f));
    g.drawFittedText (descs[cmdIndex], descArea, juce::Justification::centredLeft, 2);

    g.setColour (Theme::text);
    g.setFont (Theme::uiFont (13.5f, 700));
    g.drawText (describeValue(), valueLabelArea, juce::Justification::centredLeft);

    if (commands()[(size_t) cmdIndex].letter == 'R')
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::uiFont (12.5f, 600));
        g.drawText ("Every " + juce::String ((int) retrigSpeed.getValue()) + " ticks", retrigSpeed.getBounds().withX (bodyArea.getX()).withWidth (106), juce::Justification::centredLeft);
        g.drawText ("Fade " + juce::String ((int) retrigFall.getValue()) + "/16", retrigFall.getBounds().withX (bodyArea.getX()).withWidth (106), juce::Justification::centredLeft);
    }
}

juce::String EffectPicker::getReadout() const
{
    return juce::String::charToString ((juce::juce_wchar) commands()[(size_t) cmdIndex].letter) + hex2 (currentValue());
}

bool EffectPicker::bodyKeyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    const int dir = (code == juce::KeyPress::rightKey || code == juce::KeyPress::upKey) ? 1
                  : (code == juce::KeyPress::leftKey || code == juce::KeyPress::downKey) ? -1 : 0;
    if (dir == 0) return false;
    const int step = key.getModifiers().isShiftDown() ? 16 : 1;
    if (commands()[(size_t) cmdIndex].letter == 'R')
        retrigSpeed.setValue (retrigSpeed.getValue() + dir);
    else
        valueSlider.setValue (valueSlider.getValue() + dir * step);
    return true;
}

void EffectPicker::confirm()    { if (onInsert) onInsert (commands()[(size_t) cmdIndex].letter, currentValue()); close(); }
void EffectPicker::leftAction() { if (onInsert) onInsert (0, 0); close(); }

} // namespace artilody
