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
    recentFiles.setMaxNumberOfItems (10);
    recentFiles.restoreFromString (settings->getValue ("recentFiles"));
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
    structureUpdater.callback = [this] { structureChanged(); };
    ctx.onStructureChanged = [this] { structureUpdater.triggerAsyncUpdate(); };
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
    topBar.onRewind = [this] { rewindToStart(); };
    topBar.onPlayPattern = [this] { playPattern(); };
    topBar.onStop = [this] { stopPlayback(); };
    topBar.onUndo = [this] { ctx.undo.undo(); trackArea.grid.repaint(); };
    topBar.onRedo = [this] { ctx.undo.redo(); trackArea.grid.repaint(); };
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
    structureUpdater.cancelPendingUpdate();
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
void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* b)
{
    if (b == &engine)
    {
        structureUpdater.triggerAsyncUpdate();
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
    menuItemsChanged();
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
        if (lower == 'e') { exportAudio(); return true; }
        if (lower == 'i') { importAudioFile(); return true; }
        if (lower == 'q') { juce::JUCEApplication::getInstance()->systemRequestedQuit(); return true; }
        if (lower == 'p') { PluginHost::get().showPluginManager(); return true; }
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

void MainComponent::rewindToStart()
{
    // move to the start of the song; only keeps playing if it was already playing
    const bool wasPlaying = engine.isPlaying();
    const bool looping = engine.isLoopingPattern();
    sequenceList.selectSlot (0);
    trackArea.grid.setCursor (ctx.state.cursorTrack, ctx.state.cursorColumn, ctx.state.cursorField, 0);
    if (wasPlaying)
        engine.play (0, 0, looping);
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

//==============================================================================
namespace MenuIds
{
    enum
    {
        mi_newProject = 1, mi_openProject, mi_saveProject, mi_saveProjectAs, mi_importAudio, mi_exportAudio, mi_quit,
        mi_demoSong, mi_jazzSong, mi_clearRecent,
        mi_undo = 100, mi_redo, mi_cut, mi_copy, mi_paste, mi_del, mi_selectTrack, mi_transposeUp, mi_transposeDown, mi_octaveUp, mi_octaveDown, mi_interpolate, mi_editMode,
        mi_playSong = 200, mi_playFromStart, mi_rewind, mi_playPattern, mi_stop, mi_addTrack, mi_renameSong, mi_followPlayback,
        mi_toggleMixer = 300, mi_collapseFx,
        mi_pluginManager = 400, mi_scanAllPlugins,
        mi_audioSettings = 500, mi_defaultSoundFont,
        mi_keyboardHelp = 600, mi_about,
        mi_recentBase = 1000
    };
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Song", "View", "Plug-ins", "Options", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    using namespace MenuIds;
    juce::PopupMenu m;
    auto item = [&m] (int id, const juce::String& text, const juce::String& shortcut = {}, bool enabled = true, bool ticked = false)
    {
        juce::PopupMenu::Item i (text);
        i.itemID = id;
        i.isEnabled = enabled;
        i.isTicked = ticked;
        i.shortcutKeyDescription = shortcut;
        m.addItem (i);
    };

    const juce::String cmd =
       #if JUCE_MAC
        juce::String (juce::CharPointer_UTF8 ("\xe2\x8c\x98"));
       #else
        "Ctrl+";
       #endif

    switch (index)
    {
        case 0:
        {
            item (mi_newProject, "New Project", cmd + "N");
            item (mi_openProject, "Open Project...", cmd + "O");
            juce::PopupMenu recent;
            recentFiles.createPopupMenuItems (recent, mi_recentBase, true, true);
            if (recent.getNumItems() > 0)
            {
                recent.addSeparator();
                recent.addItem (mi_clearRecent, "Clear list");
            }
            m.addSubMenu ("Open Recent Project", recent, recent.getNumItems() > 0);
            m.addSeparator();
            item (mi_saveProject, "Save Project", cmd + "S");
            item (mi_saveProjectAs, "Save Project As...", cmd + "Shift+S");
            m.addSeparator();
            item (mi_importAudio, "Open Audio File as Instrument...", cmd + "I", engine.song.numTracks() > 0);
            item (mi_exportAudio, "Export Song as Audio (WAV / AIFF / FLAC)...", cmd + "E");
            m.addSeparator();
            juce::PopupMenu demos;
            demos.addItem (mi_demoSong, "Sunrise Circuits (pop / electro)");
            demos.addItem (mi_jazzSong, "Blue Corner Swing (jazz)");
            m.addSubMenu ("Demo Songs", demos);
           #if ! JUCE_MAC
            m.addSeparator();
            item (mi_quit, "Quit", cmd + "Q");
           #endif
            break;
        }
        case 1:
            item (mi_undo, ctx.undo.canUndo() ? "Undo " + ctx.undo.getUndoDescription() : juce::String ("Undo"), cmd + "Z", ctx.undo.canUndo());
            item (mi_redo, ctx.undo.canRedo() ? "Redo " + ctx.undo.getRedoDescription() : juce::String ("Redo"), cmd + "Y", ctx.undo.canRedo());
            m.addSeparator();
            item (mi_cut, "Cut", cmd + "X");
            item (mi_copy, "Copy", cmd + "C");
            item (mi_paste, "Paste", cmd + "V");
            item (mi_del, "Delete", "Del");
            item (mi_selectTrack, "Select Track Column", cmd + "A");
            m.addSeparator();
            item (mi_transposeUp, "Transpose +1", cmd + "Up");
            item (mi_transposeDown, "Transpose -1", cmd + "Down");
            item (mi_octaveUp, "Transpose +12", cmd + "Shift+Up");
            item (mi_octaveDown, "Transpose -12", cmd + "Shift+Down");
            item (mi_interpolate, "Interpolate Volume");
            m.addSeparator();
            item (mi_editMode, "Edit Mode", "Esc", true, ctx.state.editMode);
            break;
        case 2:
            item (mi_playSong, "Play Song", "Space");
            item (mi_playFromStart, "Play From Start");
            item (mi_rewind, "Go to Start");
            item (mi_playPattern, "Loop Pattern", "Shift+Space");
            item (mi_stop, "Stop", "Space", engine.isPlaying());
            m.addSeparator();
            item (mi_followPlayback, "Follow Playback", {}, true, ctx.state.followPlayback);
            m.addSeparator();
            item (mi_addTrack, "Add Track", cmd + "T");
            item (mi_renameSong, "Rename Song...");
            break;
        case 3:
            item (mi_toggleMixer, "Show Mixer", cmd + "M", true, ! ctx.state.mixerCollapsed);
            item (mi_collapseFx, "Collapse / Expand All Plug-in Lists");
            break;
        case 4:
            item (mi_pluginManager, "Plug-in Manager (scan VST3 / AU / CLAP)...", cmd + "P");
            item (mi_scanAllPlugins, "Scan for New Plug-ins (all formats)");
            break;
        case 5:
            item (mi_audioSettings, "Audio & MIDI Settings...");
            item (mi_defaultSoundFont, "Default SoundFont...");
            break;
        case 6:
            item (mi_keyboardHelp, "Keyboard Shortcuts");
            item (mi_about, "About Artilody Tracker");
            break;
        default: break;
    }
    return m;
}

void MainComponent::menuItemSelected (int id, int)
{
    using namespace MenuIds;
    auto& grid = trackArea.grid;

    if (id >= mi_recentBase && id < mi_recentBase + 100)
    {
        auto f = recentFiles.getFile (id - mi_recentBase);
        auto doOpen = [this, f] { if (f.existsAsFile()) loadFile (f); };
        if (! ctx.state.dirty) doOpen();
        else juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Open project", "Discard unsaved changes?", "Discard", "Cancel", nullptr,
                                                 juce::ModalCallbackFunction::create ([doOpen] (int r) { if (r != 0) doOpen(); }));
        return;
    }

    switch (id)
    {
        case mi_newProject:     newProject(); break;
        case mi_openProject:    openProject(); break;
        case mi_saveProject:    saveProject (false); break;
        case mi_saveProjectAs:  saveProject (true); break;
        case mi_importAudio:    importAudioFile(); break;
        case mi_exportAudio:    exportAudio(); break;
        case mi_quit:           juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
        case mi_demoSong:       loadBuiltInSong (false); break;
        case mi_jazzSong:       loadBuiltInSong (true); break;
        case mi_clearRecent:    recentFiles.clear(); properties.getUserSettings()->setValue ("recentFiles", juce::String()); break;

        case mi_undo:           ctx.undo.undo(); grid.repaint(); break;
        case mi_redo:           ctx.undo.redo(); grid.repaint(); break;
        case mi_cut:            grid.copySelection (true); break;
        case mi_copy:           grid.copySelection (false); break;
        case mi_paste:          grid.paste(); break;
        case mi_del:            grid.deleteSelectionOrCell(); break;
        case mi_selectTrack:    grid.selectAllInTrack(); break;
        case mi_transposeUp:    grid.transpose (1); break;
        case mi_transposeDown:  grid.transpose (-1); break;
        case mi_octaveUp:       grid.transpose (12); break;
        case mi_octaveDown:     grid.transpose (-12); break;
        case mi_interpolate:    grid.interpolateVolumes(); break;
        case mi_editMode:       ctx.state.editMode = ! ctx.state.editMode; ctx.state.changed(); break;

        case mi_playSong:       playSong (false); break;
        case mi_playFromStart:  playSong (true); break;
        case mi_rewind:      rewindToStart(); break;
        case mi_playPattern:    playPattern(); break;
        case mi_stop:           stopPlayback(); break;
        case mi_followPlayback: ctx.state.followPlayback = ! ctx.state.followPlayback; ctx.state.changed(); break;
        case mi_addTrack:       addTrack(); break;
        case mi_renameSong:     renameSong(); break;

        case mi_toggleMixer:    mixerButton.triggerClick(); break;
        case mi_collapseFx:     fxListsButton.triggerClick(); break;
        case mi_pluginManager:  PluginHost::get().showPluginManager(); break;
        case mi_scanAllPlugins: PluginHost::get().showPluginManager (true); break;
        case mi_audioSettings:  showAudioSettings(); break;
        case mi_defaultSoundFont: chooseDefaultSoundFont(); break;
        case mi_keyboardHelp:   showKeyboardHelp(); break;
        case mi_about:          showAbout(); break;
        default: break;
    }
}

void MainComponent::renameSong()
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
}

void MainComponent::loadBuiltInSong (bool jazz)
{
    auto doLoad = [this, jazz]
    {
        stopPlayback();
        PluginHost::get().closeAllEditors();
        ctx.undo.clearUndoHistory();
        if (jazz) ProjectIO::createJazzDemo (engine);
        else      ProjectIO::createDemoSong (engine);
        resetEditorState();
        structureChanged();
    };
    if (! ctx.state.dirty) { doLoad(); return; }
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Demo song", "Discard unsaved changes?", "Discard", "Cancel", nullptr,
                                        juce::ModalCallbackFunction::create ([doLoad] (int r) { if (r != 0) doLoad(); }));
}

void MainComponent::showKeyboardHelp()
{
    juce::String t;
    t << "Esc\tEdit mode on / off\n"
      << "Space\tPlay / stop song\n"
      << "Shift+Space\tLoop current pattern\n"
      << "Enter\tPlay from cursor row\n"
      << "Z..M, Q..P\tNotes (two octaves)\n"
      << "A or 1\tNote off\n"
      << "0-9, A-F\tHex values in Ins / Vol / FX\n"
      << "Arrows, Tab, PgUp/PgDn, Home/End\tNavigation\n"
      << "Shift+Arrows / mouse drag\tSelect\n"
      << "Del / Ins / Backspace\tClear, insert row, delete row\n"
      << "Ctrl+C / X / V\tCopy, cut, paste\n"
      << "Ctrl+Z / Y\tUndo / redo\n"
      << "Ctrl+Up / Down (+Shift)\tTranspose semitone (octave)\n"
      << "Num * / F2\tOctave up\n"
      << "Num / / F1\tOctave down\n"
      << "Ctrl+N / O / S / E / I\tNew, open, save, export audio, open audio file\n"
      << "Ctrl+T / M / P\tAdd track, mixer, plug-in manager\n\n"
      << "Effects: Txx tempo, Lxx lines per beat, Dxx delay, Cxx cut,\n"
      << "Rxy retrigger, Pxx pitch bend, Mxx modulation, Bxx jump.";
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Keyboard Shortcuts", t.replace ("\t", "  -  "));
}

void MainComponent::addRecentFile (const juce::File& f)
{
    recentFiles.addFile (f);
    properties.getUserSettings()->setValue ("recentFiles", recentFiles.toString());
    menuItemsChanged();
}

void MainComponent::importAudioFile()
{
    if (engine.song.numTracks() == 0) return;
    if (auto* strip = trackArea.getStripComponent (ctx.state.cursorTrack))
        strip->chooseAudioSample();
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
    addRecentFile (f);
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

void MainComponent::openFileFromSystem (const juce::File& f)
{
    if (! f.existsAsFile() || ! ProjectIO::isProjectFile (f))
        return;
    if (! ctx.state.dirty) { loadFile (f); return; }
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Open project", "Discard unsaved changes?", "Discard", "Cancel", nullptr,
                                        juce::ModalCallbackFunction::create ([this, f] (int r) { if (r != 0) loadFile (f); }));
}

void MainComponent::openProject()
{
    auto doOpen = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Open project", lastDirectory, ProjectIO::openWildcard);
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
        // Always write .artixml - a project opened from an old .artilody file is saved
        // as a new .artixml file next to it (the old file is left untouched).
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
        addRecentFile (f);
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

void MainComponent::exportAudio()
{
    auto initial = lastDirectory.getChildFile (juce::File::createLegalFileName (engine.song.title) + ".wav");
    chooser = std::make_unique<juce::FileChooser> ("Export song as audio (WAV / AIFF / FLAC)", initial, "*.wav;*.aiff;*.aif;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (! f.hasFileExtension ("wav;aif;aiff;flac"))
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
    auto second = f.getSiblingFile (juce::String ("roundtrip2") + ProjectIO::fileExtension);
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
