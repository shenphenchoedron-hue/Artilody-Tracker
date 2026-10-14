#include "MainComponent.h"
#include "../PluginHost.h"
#include "../ProjectIO.h"

namespace artilody
{

MainComponent::MainComponent (juce::ApplicationProperties& props) : properties (props)
{
    setWantsKeyboardFocus (true);

    auto* settings = properties.getUserSettings();
    lastDirectory = juce::File (settings->getValue ("lastDirectory", juce::File::getSpecialLocation (juce::File::userMusicDirectory).getFullPathName()));
    const auto sfOverride = settings->getValue ("defaultSoundFont");
    if (sfOverride.isNotEmpty())
        SoundFontFile::setDefaultOverride (juce::File (sfOverride));

    // audio
    {
        auto saved = settings->getXmlValue ("audioDevice");
        engine.initialiseAudio (saved.get());
    }
    SoundFontFile::getDefault();

    // context callbacks
    ctx.onStructureChanged = [this] { triggerAsyncUpdate(); };
    ctx.onPatternChanged = [this] { patternChanged(); };
    ctx.onShowAudioSettings = [this] { showAudioSettings(); };
    ctx.onSelectTrack = [this] (int t) { selectTrack (t); };
    ctx.state.mixerCollapsed = settings->getBoolValue ("mixerCollapsed", false);
    ctx.state.addChangeListener (this);
    engine.addChangeListener (this);
    engine.onProcessorDeleted = [] (juce::AudioProcessor* p) { PluginHost::get().closeEditor (p); };
    engine.onMidiNote = [this] (int note, int vel, bool on)
    {
        if (on) trackArea.grid.enterMidiNote (note, vel);
    };

    // top bar
    topBar.onPlaySong = [this] { playSong (false); };
    topBar.onRewind = [this] { playSong (true); };
    topBar.onPlayPattern = [this] { playPattern(); };
    topBar.onStop = [this] { stopPlayback(); };
    topBar.onUndo = [this] { ctx.undo.undo(); trackArea.grid.repaint(); };
    topBar.onRedo = [this] { ctx.undo.redo(); trackArea.grid.repaint(); };
    topBar.onPluginManager = [] { PluginHost::get().showPluginManager(); };
    topBar.onAudioSettings = [this] { showAudioSettings(); };
    topBar.onExport = [this] { exportWav(); };
    topBar.onSave = [this] { saveProject (false); };
    topBar.onMainMenu = [this] { showMainMenu(); };
    addAndMakeVisible (topBar);

    addAndMakeVisible (sequenceList);
    addAndMakeVisible (rowNumbers);

    trackViewport.setViewedComponent (&trackArea, false);
    trackViewport.setScrollBarsShown (false, true);
    trackViewport.setScrollBarThickness (10);
    addAndMakeVisible (trackViewport);
    trackArea.onAddTrack = [this] { addTrack(); };
    trackArea.grid.onViewChanged = [this] { rowNumbers.repaint(); trackArea.repaint (0, 0, trackArea.getWidth(), trackArea.getGridTop()); };
    trackArea.grid.onCursorTrackChanged = [this] { trackArea.repaint (0, 0, trackArea.getWidth(), trackArea.getGridTop()); repaint (toolbarArea); };
    trackArea.grid.onRequestTrackVisible = [this] (int t) { scrollToTrack (t); };

    masterStrip = std::make_unique<MixerStrip> (ctx, engine.getMaster(), -1);
    addAndMakeVisible (*masterStrip);

    addAndMakeVisible (rightPanel);

    addTrackButton.onClick = [this] { addTrack(); };
    addTrackButton.setTooltip ("Add a track - a matching channel strip is created under it");
    mixerButton.onClick = [this]
    {
        ctx.state.mixerCollapsed = ! ctx.state.mixerCollapsed;
        properties.getUserSettings()->setValue ("mixerCollapsed", ctx.state.mixerCollapsed);
        trackArea.setMixerCollapsed (ctx.state.mixerCollapsed);
        masterStrip->setCompact (ctx.state.mixerCollapsed);
        mixerButton.setButtonText (ctx.state.mixerCollapsed ? "Show Mixer" : "Hide Mixer");
        resized();
        repaint();
    };
    mixerCollapseButton.drawBackground = false;
    mixerCollapseButton.setTooltip ("Collapse / expand the mixer");
    mixerCollapseButton.onClick = [this] { mixerButton.triggerClick(); };
    fxListsButton.setTooltip ("Collapse or expand the plug-in lists of all channel strips");
    fxListsButton.onClick = [this]
    {
        bool anyOpen = engine.getMaster().fxCollapsed == false;
        for (int t = 0; t < engine.getNumStrips(); ++t)
            anyOpen = anyOpen || ! engine.getStrip (t).fxCollapsed;
        for (int t = 0; t < engine.getNumStrips(); ++t)
            engine.getStrip (t).fxCollapsed = anyOpen;
        engine.getMaster().fxCollapsed = anyOpen;
        structureChanged();
    };
    colsMinus.setTooltip ("Remove a note column from the selected track");
    colsPlus.setTooltip ("Add a note column to the selected track");
    colsMinus.onClick = [this]
    {
        const int t = ctx.state.cursorTrack;
        if (! juce::isPositiveAndBelow (t, engine.song.numTracks())) return;
        engine.setTrackColumns (t, engine.song.tracks[(size_t) t].noteColumns - 1);
        ctx.state.dirty = true;
        structureChanged();
    };
    colsPlus.onClick = [this]
    {
        const int t = ctx.state.cursorTrack;
        if (! juce::isPositiveAndBelow (t, engine.song.numTracks())) return;
        engine.setTrackColumns (t, engine.song.tracks[(size_t) t].noteColumns + 1);
        ctx.state.dirty = true;
        structureChanged();
    };
    for (auto* b : { &addTrackButton, &mixerButton, &fxListsButton, &colsMinus, &colsPlus })
        addAndMakeVisible (b);
    addAndMakeVisible (mixerCollapseButton);
    addTrackButton.setColour (juce::TextButton::buttonColourId, Theme::selection);
    addTrackButton.setColour (juce::TextButton::textColourOffId, Theme::accentBlue);

    // initial song
    ProjectIO::createDemoSong (engine);
    ctx.state.dirty = false;

    trackArea.setMixerCollapsed (ctx.state.mixerCollapsed);
    masterStrip->setCompact (ctx.state.mixerCollapsed);
    structureChanged();

    setSize (1600, 960);
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)]
    {
        if (safe != nullptr) safe->trackArea.grid.grabKeyboardFocus();
    });
}

MainComponent::~MainComponent()
{
    cancelPendingUpdate();
    engine.stop();
    ctx.state.removeChangeListener (this);
    engine.removeChangeListener (this);

    if (auto* settings = properties.getUserSettings())
    {
        if (auto xml = engine.deviceManager.createStateXml())
            settings->setValue ("audioDevice", xml.get());
        settings->setValue ("lastDirectory", lastDirectory.getFullPathName());
        settings->saveIfNeeded();
    }

    PluginHost::get().closeAllEditors();
    engine.onMidiNote = nullptr;
    engine.shutdownAudio();
    trackViewport.setViewedComponent (nullptr, false);
    masterStrip.reset();
}

//==============================================================================
void MainComponent::handleAsyncUpdate()
{
    structureChanged();
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* b)
{
    if (b == &engine)
    {
        triggerAsyncUpdate();
        return;
    }
    // editor state change
    mixerButton.setButtonText (ctx.state.mixerCollapsed ? "Show Mixer" : "Hide Mixer");
    trackArea.grid.repaint();
    updateTitle();
}

void MainComponent::structureChanged()
{
    auto& song = engine.song;
    ctx.state.seqSlot = juce::jlimit (0, juce::jmax (0, (int) song.sequence.size() - 1), ctx.state.seqSlot);
    ctx.state.pattern = juce::jlimit (0, juce::jmax (0, (int) song.patterns.size() - 1), ctx.state.pattern);

    mixerButton.setButtonText (ctx.state.mixerCollapsed ? "Show Mixer" : "Hide Mixer");
    trackArea.rebuild();
    masterStrip->refresh();
    rightPanel.refresh();
    sequenceList.repaint();
    resized();
    repaint();
    updateTitle();
}

void MainComponent::patternChanged()
{
    trackArea.grid.updateLayout();
    rowNumbers.repaint();
    sequenceList.repaint();
    rightPanel.refresh();
    repaint (toolbarArea);
}

void MainComponent::selectTrack (int t)
{
    if (! juce::isPositiveAndBelow (t, engine.song.numTracks())) return;
    auto& g = trackArea.grid;
    g.setCursor (t, 0, 0, ctx.state.cursorRow);
    trackArea.repaint();
    g.grabKeyboardFocus();
}

void MainComponent::scrollToTrack (int t)
{
    const int x = trackArea.grid.getTrackX (t);
    const int w = trackArea.grid.getTrackWidth (t);
    auto pos = trackViewport.getViewPosition();
    const int vw = trackViewport.getViewWidth();
    if (x < pos.x) pos.x = x;
    else if (x + w > pos.x + vw) pos.x = x + w - vw + 8;
    trackViewport.setViewPosition (pos);
}

void MainComponent::updateTitle()
{
    juce::String t = "Artilody Tracker - " + engine.song.title;
    if (ctx.state.currentFile.existsAsFile()) t << "  [" << ctx.state.currentFile.getFileName() << "]";
    if (ctx.state.dirty) t << " *";
    if (onTitleChanged) onTitleChanged (t);
    topBar.repaint();
}

//==============================================================================
void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    topBar.setBounds (r.removeFromTop (64));
    r.removeFromTop (6);

    toolbarArea = r.removeFromTop (38);
    {
        auto tb = toolbarArea;
        addTrackButton.setBounds (tb.removeFromLeft (124).reduced (0, 3));
        tb.removeFromLeft (16);
        colsLabelArea = tb.removeFromLeft (70);
        colsMinus.setBounds (tb.removeFromLeft (30).reduced (0, 4));
        tb.removeFromLeft (4);
        colsPlus.setBounds (tb.removeFromLeft (30).reduced (0, 4));
        tb.removeFromLeft (16);
        auto rightSide = tb.removeFromRight (290);
        mixerButton.setBounds (rightSide.removeFromRight (120).reduced (0, 3));
        rightSide.removeFromRight (8);
        fxListsButton.setBounds (rightSide.removeFromRight (150).reduced (0, 3));
        patternInfoArea = tb;
    }
    r.removeFromTop (8);

    rightPanel.setBounds (r.removeFromRight (262));
    r.removeFromRight (12);

    auto left = r.removeFromLeft (Layout::leftColumnWidth);
    r.removeFromLeft (8);

    trackViewport.setBounds (r);
    const int scrollH = 10;
    trackArea.setSize (juce::jmax (trackArea.getIdealWidth(), r.getWidth()), r.getHeight() - scrollH);

    const int mixerY = r.getY() + trackArea.getGridBottom() + 10;
    const int mixerH = trackArea.getMixerHeight();
    masterStrip->setBounds (left.getX(), mixerY, left.getWidth(), mixerH);
    mixerCollapseButton.setBounds (left.getX() + left.getWidth() / 2 - 12, mixerY - 22, 24, 18);
    mixerCollapseButton.icon = ctx.state.mixerCollapsed ? Theme::Icon::ChevronUp : Theme::Icon::ChevronDown;

    sequenceList.setBounds (left.getX(), r.getY(), Layout::seqWidth, mixerY - 26 - r.getY());
    rowNumbers.setBounds (left.getX() + Layout::seqWidth + 6, r.getY() + trackArea.getGridTop(),
                          Layout::rowNumberWidth, trackArea.getGridBottom() - trackArea.getGridTop());
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);

    // toolbar info
    g.setColour (Theme::textDim);
    g.setFont (Theme::uiFont (12.5f, 600));
    g.drawText ("Columns", colsLabelArea, juce::Justification::centredLeft);

    if (auto* p = ctx.currentPattern())
    {
        auto pr = patternInfoArea.reduced (0, 4);
        auto chip = pr.removeFromLeft (juce::jmin (pr.getWidth(), 420)).toFloat();
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (chip, 8.0f);
        g.setColour (Theme::border);
        g.drawRoundedRectangle (chip.reduced (0.5f), 8.0f, 1.0f);
        auto ir = chip.reduced (10.0f, 0.0f);
        g.setColour (p->colour);
        g.fillRoundedRectangle (ir.removeFromLeft (14.0f).withSizeKeepingCentre (14.0f, 14.0f), 4.0f);
        ir.removeFromLeft (8.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::uiFont (13.5f, 700));
        juce::String info = "Pattern " + juce::String::toHexString (ctx.state.pattern).paddedLeft ('0', 2).toUpperCase() + "  " + p->name;
        g.drawFittedText (info, ir.removeFromLeft (ir.getWidth() * 0.6f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        g.setColour (Theme::textDim);
        g.setFont (Theme::uiFont (12.5f, 400));
        juce::String trackInfo;
        if (juce::isPositiveAndBelow (ctx.state.cursorTrack, engine.song.numTracks()))
            trackInfo = engine.song.tracks[(size_t) ctx.state.cursorTrack].name + ", row " + juce::String::toHexString (ctx.state.cursorRow).paddedLeft ('0', 2).toUpperCase();
        g.drawFittedText (trackInfo, ir.toNearestInt(), juce::Justification::centredRight, 1, 0.8f);
    }

    if (ctx.state.editMode)
    {
        auto vb = trackViewport.getBounds().toFloat().expanded (2.0f);
        g.setColour (Theme::record.withAlpha (0.55f));
        g.drawRoundedRectangle (vb.withHeight ((float) trackArea.getGridBottom() + 4.0f), 9.0f, 2.0f);
    }
}

//==============================================================================
bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();
    const auto lower = juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) code);

    if (code == juce::KeyPress::spaceKey)
    {
        if (engine.isPlaying()) stopPlayback();
        else if (mods.isShiftDown()) playPattern();
        else playSong (false);
        return true;
    }
    if (code == juce::KeyPress::returnKey && ! mods.isAnyModifierKeyDown())
    {
        engine.play (ctx.state.seqSlot, ctx.state.cursorRow, false);
        return true;
    }
    if (mods.isCommandDown())
    {
        if (lower == 's') { saveProject (mods.isShiftDown()); return true; }
        if (lower == 'o') { openProject(); return true; }
        if (lower == 'n') { newProject(); return true; }
        if (lower == 'e') { exportWav(); return true; }
        if (lower == 'z' && ! mods.isShiftDown()) { ctx.undo.undo(); trackArea.grid.repaint(); return true; }
        if (lower == 'y' || (lower == 'z' && mods.isShiftDown())) { ctx.undo.redo(); trackArea.grid.repaint(); return true; }
        if (lower == 't') { addTrack(); return true; }
        if (lower == 'm') { mixerButton.triggerClick(); return true; }
    }
    if (code == juce::KeyPress::F1Key) { ctx.state.octave = juce::jmax (0, ctx.state.octave - 1); ctx.state.changed(); return true; }
    if (code == juce::KeyPress::F2Key) { ctx.state.octave = juce::jmin (9, ctx.state.octave + 1); ctx.state.changed(); return true; }
    return false;
}

//==============================================================================
void MainComponent::addTrack()
{
    ctx.undo.clearUndoHistory();
    const int t = engine.addTrack ("Track " + juce::String (engine.song.numTracks() + 1).paddedLeft ('0', 2));
    if (t < 0) return;
    ProjectIO::setSoundFontInstrument (engine.getStrip (t), "Pop", "Grand Piano");
    ctx.state.dirty = true;
    structureChanged();
    selectTrack (t);
    scrollToTrack (t);
}

void MainComponent::playSong (bool fromStart)
{
    if (fromStart)
    {
        ctx.state.seqSlot = 0;
        sequenceList.selectSlot (0);
    }
    engine.play (ctx.state.seqSlot, 0, false);
    topBar.updateState();
}

void MainComponent::playPattern()
{
    engine.play (ctx.state.seqSlot, 0, true);
    topBar.updateState();
}

void MainComponent::stopPlayback()
{
    engine.stop();
    topBar.updateState();
}

//==============================================================================
void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (engine.deviceManager, 0, 0, 2, 2, true, false, true, false);
    selector->setSize (560, 460);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (selector);
    o.dialogTitle = "Audio & MIDI Settings";
    o.dialogBackgroundColour = Theme::panel;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

void MainComponent::showMainMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "New project");
    m.addItem (2, "Open project...");
    m.addItem (3, "Save project");
    m.addItem (4, "Save project as...");
    m.addSeparator();
    m.addItem (5, "Export song to WAV...");
    m.addSeparator();
    m.addItem (6, "Rename song...");
    m.addItem (7, "Load demo song");
    m.addItem (12, "Load jazz demo song");
    m.addSeparator();
    m.addItem (8, "Plug-in Manager (VST3 / AU / CLAP)...");
    m.addItem (9, "Audio & MIDI settings...");
    m.addItem (10, "Default SoundFont...");
    m.addSeparator();
    m.addItem (11, "About Artilody Tracker");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&topBar), [this] (int r)
    {
        switch (r)
        {
            case 1: newProject(); break;
            case 2: openProject(); break;
            case 3: saveProject (false); break;
            case 4: saveProject (true); break;
            case 5: exportWav(); break;
            case 6:
            {
                auto* aw = new juce::AlertWindow ("Rename song", "Song title:", juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("t", engine.song.title);
                aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int res)
                {
                    if (res == 1 && aw->getTextEditorContents ("t").trim().isNotEmpty())
                    {
                        engine.song.title = aw->getTextEditorContents ("t").trim();
                        ctx.state.dirty = true;
                        updateTitle();
                    }
                }), true);
                break;
            }
            case 7:
                stopPlayback();
                PluginHost::get().closeAllEditors();
                ctx.undo.clearUndoHistory();
                ProjectIO::createDemoSong (engine);
                resetEditorState();
                structureChanged();
                break;
            case 12:
                stopPlayback();
                PluginHost::get().closeAllEditors();
                ctx.undo.clearUndoHistory();
                ProjectIO::createJazzDemo (engine);
                resetEditorState();
                structureChanged();
                break;
            case 8: PluginHost::get().showPluginManager(); break;
            case 9: showAudioSettings(); break;
            case 10: chooseDefaultSoundFont(); break;
            case 11: showAbout(); break;
            default: break;
        }
    });
}

void MainComponent::showAbout()
{
    auto sf = SoundFontFile::getDefault();
    juce::String msg;
    msg << "Artilody Tracker " << ARTILODY_VERSION << "\n\n"
        << "A pattern based music tracker with a full mixer: every track has its own channel strip "
        << "with instrument, VST3 / AU / CLAP effects, EQ, compressor, reverb, panning and volume.\n\n"
        << "Built with JUCE. SoundFont playback by TinySoundFont.\n"
        << "Default SoundFont: " << (sf != nullptr ? sf->getFile().getFileName() : juce::String ("none found")) << "\n"
        << "(GeneralUser GS by S. Christian Collins)";
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "About", msg);
}

void MainComponent::chooseDefaultSoundFont()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose the default General MIDI SoundFont", lastDirectory, "*.sf2");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        properties.getUserSettings()->setValue ("defaultSoundFont", f.getFullPathName());
        SoundFontFile::setDefaultOverride (f);
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Default SoundFont",
            "New built-in instruments will use " + f.getFileName() + ".");
    });
}

void MainComponent::resetEditorState()
{
    auto& st = ctx.state;
    st.currentFile = juce::File();
    st.seqSlot = 0;
    st.pattern = engine.song.sequence.empty() ? 0 : engine.song.sequence[0];
    st.cursorTrack = st.cursorRow = st.cursorColumn = st.cursorField = 0;
    st.hasSelection = false;
    st.dirty = false;
}

void MainComponent::newProject()
{
    auto doNew = [this]
    {
        stopPlayback();
        PluginHost::get().closeAllEditors();
        ctx.undo.clearUndoHistory();
        ProjectIO::createEmptySong (engine);
        resetEditorState();
        structureChanged();
    };

    if (! ctx.state.dirty) { doNew(); return; }
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "New project", "Discard unsaved changes?", "Discard", "Cancel", nullptr,
                                        juce::ModalCallbackFunction::create ([doNew] (int r) { if (r != 0) doNew(); }));
}

void MainComponent::loadFile (const juce::File& f)
{
    stopPlayback();
    PluginHost::get().closeAllEditors();
    ctx.undo.clearUndoHistory();
    juce::String err;
    if (! ProjectIO::load (engine, f, err))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Open project", err);
        return;
    }
    if (err.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Open project", err);

    lastDirectory = f.getParentDirectory();
    const bool collapsed = ctx.state.mixerCollapsed;
    ctx.state.currentFile = f;
    ctx.state.seqSlot = 0;
    ctx.state.pattern = engine.song.sequence.empty() ? 0 : engine.song.sequence[0];
    ctx.state.cursorTrack = ctx.state.cursorRow = ctx.state.cursorColumn = ctx.state.cursorField = 0;
    ctx.state.hasSelection = false;
    ctx.state.mixerCollapsed = collapsed;
    ctx.state.dirty = false;
    structureChanged();
}

void MainComponent::openProject()
{
    auto doOpen = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Open project", lastDirectory, juce::String ("*") + ProjectIO::fileExtension);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile()) loadFile (f);
        });
    };

    if (! ctx.state.dirty) { doOpen(); return; }
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Open project", "Discard unsaved changes?", "Discard", "Cancel", nullptr,
                                        juce::ModalCallbackFunction::create ([doOpen] (int r) { if (r != 0) doOpen(); }));
}

void MainComponent::saveProject (bool saveAs, std::function<void (bool)> done)
{
    auto writeTo = [this, done] (juce::File f)
    {
        if (f == juce::File()) { if (done) done (false); return; }
        f = f.withFileExtension (ProjectIO::fileExtension);
        juce::String err;
        if (! ProjectIO::save (engine, f, err))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save project", err);
            if (done) done (false);
            return;
        }
        ctx.state.currentFile = f;
        ctx.state.dirty = false;
        lastDirectory = f.getParentDirectory();
        updateTitle();
        if (done) done (true);
    };

    if (! saveAs && ctx.state.currentFile != juce::File())
    {
        writeTo (ctx.state.currentFile);
        return;
    }

    auto initial = lastDirectory.getChildFile (juce::File::createLegalFileName (engine.song.title) + ProjectIO::fileExtension);
    chooser = std::make_unique<juce::FileChooser> ("Save project", initial, juce::String ("*") + ProjectIO::fileExtension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [writeTo] (const juce::FileChooser& fc) { writeTo (fc.getResult()); });
}

void MainComponent::exportWav()
{
    auto initial = lastDirectory.getChildFile (juce::File::createLegalFileName (engine.song.title) + ".wav");
    chooser = std::make_unique<juce::FileChooser> ("Export song to WAV", initial, "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension (".wav");
        stopPlayback();

        struct ExportJob final : public juce::ThreadWithProgressWindow
        {
            ExportJob (AudioEngine& e, juce::File file)
                : ThreadWithProgressWindow ("Exporting " + file.getFileName(), true, true), engine (e), target (std::move (file)) {}

            void run() override
            {
                ok = engine.renderToFile (target, 0.0, 24, [this] (float p)
                {
                    setProgress (p);
                    return ! threadShouldExit();
                }, error);
            }

            void threadComplete (bool cancelled) override
            {
                if (ok)
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Export finished", target.getFullPathName());
                else if (! cancelled || error.isNotEmpty())
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export", error.isNotEmpty() ? error : juce::String ("Export cancelled"));
                delete this;
            }

            AudioEngine& engine;
            juce::File target;
            juce::String error;
            bool ok = false;
        };

        auto* job = new ExportJob (engine, f);
        job->launchThread();
    });
}

bool MainComponent::roundTripTest (const juce::File& f)
{
    juce::String err;
    if (! ProjectIO::save (engine, f, err)) return false;
    const auto first = f.loadFileAsString();
    if (! ProjectIO::load (engine, f, err)) return false;
    auto second = f.getSiblingFile ("roundtrip2.artilody");
    if (! ProjectIO::save (engine, second, err)) return false;
    return first == second.loadFileAsString();
}

bool MainComponent::saveJazzDemo (const juce::File& projectFile, const juce::File& wavFile)
{
    ProjectIO::createJazzDemo (engine);
    juce::String err;
    projectFile.getParentDirectory().createDirectory();
    if (! ProjectIO::save (engine, projectFile, err)) return false;
    return wavFile == juce::File() || engine.renderToFile (wavFile, 44100.0, 16, {}, err);
}

bool MainComponent::renderTo (const juce::File& f)
{
    juce::String err;
    const bool ok = engine.renderToFile (f, 44100.0, 16, {}, err);
    if (! ok) DBG (err);
    return ok;
}

void MainComponent::requestQuit (std::function<void (bool)> callback)
{
    if (! ctx.state.dirty)
    {
        callback (true);
        return;
    }
    juce::AlertWindow::showYesNoCancelBox (juce::MessageBoxIconType::QuestionIcon, "Quit Artilody Tracker",
        "Save changes to \"" + engine.song.title + "\" before quitting?", "Save", "Don't save", "Cancel", nullptr,
        juce::ModalCallbackFunction::create ([this, callback] (int r)
        {
            if (r == 1) saveProject (false, [callback] (bool ok) { callback (ok); });
            else if (r == 2) callback (true);
            else callback (false);
        }));
}

} // namespace artilody
