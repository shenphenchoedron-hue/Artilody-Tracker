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
    recordButton.fill = juce::Colour (0xffffe3e6);
    recordButton.iconColour = Theme::record;
    recordButton.onFill = Theme::record;
    loopButton.onFill = Theme::accentBlue;
    followButton.onFill = Theme::accentBlue;

    rewindButton.setTooltip ("Play song from the start");
    playButton.setTooltip ("Play song from the selected sequence slot (Space)");
    loopButton.setTooltip ("Loop the current pattern (Shift+Space)");
    stopButton.setTooltip ("Stop");
    recordButton.setTooltip ("Edit mode - type notes into the pattern (Esc)");
    followButton.setTooltip ("Follow playback");
    undoButton.setTooltip ("Undo (Ctrl+Z)");
    redoButton.setTooltip ("Redo (Ctrl+Y)");
    pluginButton.setTooltip ("Plug-in Manager (scan VST3 / AU / CLAP)");
    exportButton.setTooltip ("Export song to WAV");
    saveButton.setTooltip ("Save project (Ctrl+S)");
    settingsButton.setTooltip ("Audio & MIDI settings");
    menuButton.setTooltip ("File menu");

    for (auto* b : { &rewindButton, &playButton, &loopButton, &stopButton, &recordButton, &followButton,
                     &undoButton, &redoButton, &pluginButton, &exportButton, &menuButton, &saveButton, &settingsButton })
    {
        b->radius = 10.0f;
        addAndMakeVisible (b);
    }
    for (auto* b : { &undoButton, &redoButton, &pluginButton, &exportButton, &menuButton, &saveButton, &settingsButton })
        b->fill = Theme::panel;

    rewindButton.onClick = [this] { if (onRewind) onRewind(); };
    playButton.onClick = [this] { if (onPlaySong) onPlaySong(); };
    loopButton.onClick = [this] { if (onPlayPattern) onPlayPattern(); };
    stopButton.onClick = [this] { if (onStop) onStop(); };
    recordButton.onClick = [this] { ctx.state.editMode = ! ctx.state.editMode; ctx.state.changed(); };
    followButton.onClick = [this] { ctx.state.followPlayback = ! ctx.state.followPlayback; ctx.state.changed(); };
    undoButton.onClick = [this] { if (onUndo) onUndo(); };
    redoButton.onClick = [this] { if (onRedo) onRedo(); };
    pluginButton.onClick = [this] { if (onPluginManager) onPluginManager(); };
    exportButton.onClick = [this] { if (onExport) onExport(); };
    saveButton.onClick = [this] { if (onSave) onSave(); };
    settingsButton.onClick = [this] { if (onAudioSettings) onAudioSettings(); };
    menuButton.onClick = [this] { if (onMainMenu) onMainMenu(); };

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
    playButton.setToggleState (ctx.engine.isPlaying() && ! ctx.engine.isLoopingPattern(), juce::dontSendNotification);
    loopButton.setToggleState (ctx.engine.isPlaying() && ctx.engine.isLoopingPattern(), juce::dontSendNotification);
    recordButton.setToggleState (ctx.state.editMode, juce::dontSendNotification);
    followButton.setToggleState (ctx.state.followPlayback, juce::dontSendNotification);
    undoButton.setEnabled (ctx.undo.canUndo());
    redoButton.setEnabled (ctx.undo.canRedo());
    for (auto* b : { &bpmBox, &lpbBox, &octBox, &stepBox })
        b->repaint();
    repaint();
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
    place (loopButton, 44, 6);
    place (stopButton, 44, 6);
    place (recordButton, 44, 18);
    place (bpmBox, 78);
    place (lpbBox, 62);
    place (octBox, 62);
    place (stepBox, 62, 8);
    place (followButton, 44, 18);

    auto right = r;
    settingsButton.setBounds (right.removeFromRight (44)); right.removeFromRight (8);
    projectArea = right.removeFromRight (juce::jmin (190, juce::jmax (0, right.getWidth() - 360)));
    right.removeFromRight (8);
    saveButton.setBounds (right.removeFromRight (44)); right.removeFromRight (6);
    menuButton.setBounds (right.removeFromRight (44)); right.removeFromRight (18);
    exportButton.setBounds (right.removeFromRight (44)); right.removeFromRight (6);
    pluginButton.setBounds (right.removeFromRight (44)); right.removeFromRight (6);
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
        Theme::drawIcon (g, Theme::Icon::Chevron, inner.removeFromRight (14).toFloat().withTrimmedTop (14).withSizeKeepingCentre (13, 13), Theme::text);
        g.setColour (Theme::textDim);
        g.setFont (Theme::uiFont (11.0f, 600));
        g.drawText (ctx.state.dirty ? "Project  (modified)" : "Project", inner.removeFromTop (14), juce::Justification::centredLeft);
        g.setColour (Theme::text);
        g.setFont (Theme::uiFont (15.5f, 700));
        g.drawFittedText (ctx.engine.song.title, inner, juce::Justification::centredLeft, 1, 0.8f);
    }
}

void TopBar::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked()) return;
    if (projectArea.contains (e.getPosition()) && onMainMenu)
        onMainMenu();
}

} // namespace artilody
