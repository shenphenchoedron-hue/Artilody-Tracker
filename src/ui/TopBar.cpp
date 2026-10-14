#include "TopBar.h"

namespace artilody
{

NumberBox::NumberBox (const juce::String& l, double mn, double mx, double s) : label (l), minValue (mn), maxValue (mx), step (s)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void NumberBox::apply (double v)
{
    v = juce::jlimit (minValue, maxValue, std::round (v / step) * step);
    if (setValue) setValue (v);
    repaint();
}

void NumberBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (Theme::border);
    g.drawRoundedRectangle (r, 9.0f, 1.0f);

    auto inner = getLocalBounds().reduced (10, 5);
    auto chevron = inner.removeFromRight (presets.isEmpty() ? 0 : 14);
    if (! presets.isEmpty())
        Theme::drawIcon (g, Theme::Icon::Chevron, chevron.toFloat().withTrimmedTop (14).withSizeKeepingCentre (13, 13), Theme::text);

    g.setColour (Theme::textDim);
    g.setFont (Theme::uiFont (11.0f, 600));
    g.drawText (label, inner.removeFromTop (14), juce::Justification::centredLeft);
    g.setColour (Theme::text);
    g.setFont (Theme::uiFont (17.0f, 700));
    const double v = getValue ? getValue() : 0.0;
    g.drawText (juce::String (v, decimals), inner, juce::Justification::centredLeft);
}

void NumberBox::mouseDown (const juce::MouseEvent&)
{
    dragStartValue = getValue ? getValue() : 0.0;
    dragging = false;
}

void NumberBox::mouseDrag (const juce::MouseEvent& e)
{
    if (std::abs (e.getDistanceFromDragStartY()) > 2) dragging = true;
    if (! dragging) return;
    const double sens = (maxValue - minValue) > 100 ? 0.5 : 0.08;
    apply (dragStartValue - e.getDistanceFromDragStartY() * sens * step / (step < 1 ? step : 1.0));
}

void NumberBox::mouseUp (const juce::MouseEvent& e)
{
    if (dragging || ! e.mouseWasClicked() || e.getNumberOfClicks() > 1) return;
    if (presets.isEmpty()) return;
    juce::PopupMenu m;
    for (int i = 0; i < presets.size(); ++i)
        m.addItem (i + 1, juce::String (presets[i], decimals), true, getValue && juce::approximatelyEqual (getValue(), presets[i]));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r) { if (r > 0) apply (presets[r - 1]); });
}

void NumberBox::mouseDoubleClick (const juce::MouseEvent&)
{
    auto* aw = new juce::AlertWindow (label, "Enter a value (" + juce::String (minValue, decimals) + " - " + juce::String (maxValue, decimals) + ")", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("v", juce::String (getValue ? getValue() : 0.0, decimals));
    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
    {
        if (r == 1) apply (aw->getTextEditorContents ("v").getDoubleValue());
    }), true);
}

void NumberBox::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    apply ((getValue ? getValue() : 0.0) + (w.deltaY > 0 ? step : -step));
}

//==============================================================================
TopBar::TopBar (AppContext& c) : ctx (c)
{
    playButton.fill = Theme::accent;
    playButton.iconColour = juce::Colours::white;
    playButton.onFill = Theme::accent.darker (0.15f);
    stopButton.fill = Theme::record;
    stopButton.iconColour = juce::Colours::white;
    stopButton.onFill = Theme::record.darker (0.15f);
    loopButton.onFill = Theme::accentBlue;
    followButton.onFill = Theme::accentBlue;

    rewindButton.setTooltip ("Go to the start of the song (does not start playback)");
    playButton.setTooltip ("Play the song (Space). After Stop it continues from where it stopped.");
    loopButton.setTooltip ("Loop on / off. When on, playback repeats the current pattern - or only the rows you mark by dragging over the row numbers.");
    stopButton.setTooltip ("Stop - Play continues from here, Rewind goes back to the start");
    recordButton.setTooltip ("Edit mode on / off (Esc) - click a field or play a MIDI keyboard to write into the pattern");
    followButton.setTooltip ("Follow playback");
    undoButton.setTooltip ("Undo (Ctrl+Z)");
    redoButton.setTooltip ("Redo (Ctrl+Y)");

    for (auto* b : { &rewindButton, &playButton, &loopButton, &stopButton, &followButton,
                     &undoButton, &redoButton })
    {
        b->radius = 10.0f;
        addAndMakeVisible (b);
    }
    addAndMakeVisible (recordButton);
    for (auto* b : { &undoButton, &redoButton })
        b->fill = Theme::panel;

    rewindButton.onClick = [this] { if (onRewind) onRewind(); };
    playButton.onClick = [this] { if (onPlaySong) onPlaySong(); };
    loopButton.onClick = [this] { if (onToggleLoop) onToggleLoop(); };
    stopButton.onClick = [this] { if (onStop) onStop(); };
    recordButton.onClick = [this] { ctx.state.editMode = ! ctx.state.editMode; ctx.state.changed(); };
    followButton.onClick = [this] { ctx.state.followPlayback = ! ctx.state.followPlayback; ctx.state.changed(); };
    undoButton.onClick = [this] { if (onUndo) onUndo(); };
    redoButton.onClick = [this] { if (onRedo) onRedo(); };

    bpmBox.getValue = [this] { return ctx.engine.isPlaying() ? ctx.engine.runtimeBpm.load() : ctx.engine.song.bpm; };
    bpmBox.setValue = [this] (double v) { ctx.engine.setBpm (v); ctx.state.dirty = true; };
    bpmBox.presets = { 70, 80, 90, 100, 110, 120, 125, 128, 130, 140, 150, 160, 174 };
    lpbBox.getValue = [this] { return (double) ctx.engine.song.lpb; };
    lpbBox.setValue = [this] (double v) { ctx.engine.setLpb ((int) v); ctx.state.dirty = true; ctx.state.changed(); };
    lpbBox.presets = { 2, 3, 4, 6, 8, 12, 16 };
    octBox.getValue = [this] { return (double) ctx.state.octave; };
    octBox.setValue = [this] (double v) { ctx.state.octave = (int) v; };
    octBox.presets = { 1, 2, 3, 4, 5, 6, 7, 8 };
    stepBox.getValue = [this] { return (double) ctx.state.editStep; };
    stepBox.setValue = [this] (double v) { ctx.state.editStep = (int) v; };
    stepBox.presets = { 0, 1, 2, 3, 4, 8, 16 };

    for (auto* b : { &bpmBox, &lpbBox, &octBox, &stepBox })
        addAndMakeVisible (b);

    masterMeter.horizontal = true;
    addAndMakeVisible (masterMeter);

    ctx.state.addChangeListener (this);
    updateState();
    startTimerHz (20);
}

TopBar::~TopBar()
{
    ctx.state.removeChangeListener (this);
}

void TopBar::changeListenerCallback (juce::ChangeBroadcaster*) { updateState(); }

void TopBar::updateState()
{
    playButton.setToggleState (ctx.engine.isPlaying(), juce::dontSendNotification);
    loopButton.setToggleState (ctx.state.loopEnabled, juce::dontSendNotification);
    recordButton.setRecording (ctx.state.editMode);
    followButton.setToggleState (ctx.state.followPlayback, juce::dontSendNotification);
    undoButton.setEnabled (ctx.undo.canUndo());
    redoButton.setEnabled (ctx.undo.canRedo());
    for (auto* b : { &bpmBox, &lpbBox, &octBox, &stepBox })
        b->repaint();
    repaint();
}

//==============================================================================
void TopBar::RecordButton::setRecording (bool on)
{
    if (on == getToggleState()) return;
    setToggleState (on, juce::dontSendNotification);
    if (on) startTimerHz (30);
    else  { stopTimer(); phase = 0.0f; }
    repaint();
}

void TopBar::RecordButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const bool on = getToggleState();
    auto b = getLocalBounds().toFloat();
    const float d = juce::jmin (b.getWidth(), b.getHeight()) - 10.0f;   // room for the glow
    auto circle = juce::Rectangle<float> (d, d).withCentre (b.getCentre());
    const auto red = Theme::record;

    if (on)
    {
        // pulsing glow around the button
        const float pulse = 0.5f + 0.5f * std::sin (phase);
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (red.withAlpha ((0.22f + 0.22f * pulse) / (float) i));
            g.fillEllipse (circle.expanded (1.2f * (float) i + 1.0f * pulse));
        }
        // bright red body with a highlight
        juce::ColourGradient grad (juce::Colour (0xffff8a96).interpolatedWith (red, 0.3f * (1.0f - pulse)), circle.getCentreX(), circle.getY() + d * 0.3f,
                                   red, circle.getCentreX(), circle.getBottom(), true);
        g.setGradientFill (grad);
        g.fillEllipse (circle);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawEllipse (circle.reduced (1.0f), 2.0f);
        // white "recording" dot
        g.setColour (juce::Colours::white);
        g.fillEllipse (circle.withSizeKeepingCentre (d * 0.34f, d * 0.34f));
    }
    else
    {
        g.setColour (down ? juce::Colour (0xffffd4d9) : (highlighted ? juce::Colour (0xffffe9ec) : Theme::panel));
        g.fillEllipse (circle);
        g.setColour (red.withAlpha (highlighted ? 0.9f : 0.55f));
        g.drawEllipse (circle.reduced (1.0f), 2.0f);
        g.setColour (red);
        g.fillEllipse (circle.withSizeKeepingCentre (d * 0.42f, d * 0.42f));
    }
}

void TopBar::timerCallback()
{
    auto& m = ctx.engine.getMaster();
    masterMeter.setLevels (m.peakL.exchange (0.0f), m.peakR.exchange (0.0f));
    cpu = cpu * 0.8f + 0.2f * ctx.engine.getCpuLoad();
    updateState();
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (4, 8);
    logoArea = r.removeFromLeft (300);

    auto place = [&r] (juce::Component& c, int w, int gap = 8) { c.setBounds (r.removeFromLeft (w)); r.removeFromLeft (gap); };
    r.removeFromLeft (10);
    place (rewindButton, 44, 6);
    place (playButton, 52, 6);
    place (stopButton, 44, 6);
    place (loopButton, 44, 6);
    // the record button is bigger than the others: it uses the full height of the bar
    {
        auto rb = r.removeFromLeft (60);
        recordButton.setBounds (rb.getX(), 0, rb.getWidth(), getHeight());
        r.removeFromLeft (12);
    }
    place (bpmBox, 78);
    place (lpbBox, 62);
    place (octBox, 62);
    place (stepBox, 62, 8);
    place (followButton, 44, 18);

    auto right = r;
    projectArea = right.removeFromRight (juce::jmin (230, juce::jmax (0, right.getWidth() - 200)));
    right.removeFromRight (16);
    redoButton.setBounds (right.removeFromRight (44)); right.removeFromRight (6);
    undoButton.setBounds (right.removeFromRight (44)); right.removeFromRight (16);
    meterArea = right;
    masterMeter.setBounds (meterArea.withSizeKeepingCentre (juce::jmax (0, meterArea.getWidth() - 10), 12).translated (0, 6));
    masterMeter.setVisible (meterArea.getWidth() > 60);
}

void TopBar::paint (juce::Graphics& g)
{
    // logo: mark + wordmark
    auto mark = Theme::logoMark();
    auto la = logoArea.toFloat();
    if (mark.isValid())
    {
        auto mr = la.removeFromLeft (la.getHeight() * 1.3f).reduced (2.0f);
        g.drawImage (mark, mr, juce::RectanglePlacement::centred);
    }
    la.removeFromLeft (6.0f);
    g.setColour (Theme::text);
    g.setFont (Theme::uiFont (30.0f, 600));
    const juce::String a ("Artilody");
    g.drawText (a, la, juce::Justification::centredLeft);
    const float aw = Theme::uiFont (30.0f, 600).getStringWidthFloat (a);
    g.setFont (Theme::uiFont (30.0f, 400));
    g.drawText ("Tracker", la.withTrimmedLeft (aw + 8.0f), juce::Justification::centredLeft);

    // master meter + cpu label
    if (meterArea.getWidth() > 60)
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::uiFont (10.5f, 600));
        auto mt = meterArea.reduced (5, 0);
        g.drawText ("MASTER", mt.withHeight (meterArea.getHeight() / 2 - 2), juce::Justification::bottomLeft);
        g.drawText ("CPU " + juce::String (juce::roundToInt (cpu * 100.0f)) + "%", mt.withHeight (meterArea.getHeight() / 2 - 2), juce::Justification::bottomRight);
    }

    // project box
    if (projectArea.getWidth() > 40)
    {
        auto pr = projectArea.toFloat().reduced (0.5f);
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (pr, 9.0f);
        g.setColour (Theme::border);
        g.drawRoundedRectangle (pr, 9.0f, 1.0f);
        auto inner = projectArea.reduced (12, 5);
        g.setColour (Theme::textDim);
        g.setFont (Theme::uiFont (11.0f, 600));
        g.drawText (ctx.state.dirty ? "Project  (modified)" : "Project", inner.removeFromTop (14), juce::Justification::centredLeft);
        g.setColour (Theme::text);
        g.setFont (Theme::uiFont (15.5f, 700));
        g.drawFittedText (ctx.engine.song.title, inner, juce::Justification::centredLeft, 1, 0.8f);
    }
}

} // namespace artilody
