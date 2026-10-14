#include "PatternGrid.h"
#include <array>

namespace artilody
{

static constexpr int kPadLeft = 9;
static constexpr int kNumFields = 8;

// Pre-built strings so painting the grid does not allocate thousands of Strings per frame.
static const juce::String& hexString (int v)
{
    static const auto table = []
    {
        std::array<juce::String, 256> t;
        for (int i = 0; i < 256; ++i)
            t[(size_t) i] = juce::String::toHexString (i).paddedLeft ('0', 2).toUpperCase();
        return t;
    }();
    return table[(size_t) (v & 0xFF)];
}

static const juce::String& noteString (int note)
{
    static const auto table = []
    {
        std::array<juce::String, 128> t;
        for (int i = 0; i < 128; ++i)
            t[(size_t) i] = noteToString (i);
        return t;
    }();
    return table[(size_t) juce::jlimit (0, 127, note)];
}

PatternGrid::PatternGrid (AppContext& c) : ctx (c)
{
    setWantsKeyboardFocus (true);
    setOpaque (true);
    startTimerHz (30);
}

PatternGrid::~PatternGrid()
{
    stopTimer();
}

void PatternGrid::updateLayout()
{
    auto& song = ctx.engine.song;
    trackX.clear();
    trackW.clear();
    int x = 0;
    for (auto& t : song.tracks)
    {
        const int w = Layout::trackWidth (t.noteColumns);
        trackX.push_back (x);
        trackW.push_back (w);
        x += w + Layout::trackGap;
    }
    totalWidth = x;

    // keep cursor valid
    auto& st = ctx.state;
    if (song.tracks.empty())
    {
        st.cursorTrack = 0;
        st.cursorColumn = 0;
    }
    else
    {
        st.cursorTrack = juce::jlimit (0, (int) song.tracks.size() - 1, st.cursorTrack);
        st.cursorColumn = juce::jlimit (0, song.tracks[(size_t) st.cursorTrack].noteColumns - 1, st.cursorColumn);
    }
    if (auto* p = ctx.currentPattern())
        st.cursorRow = juce::jlimit (0, p->numRows - 1, st.cursorRow);
    repaint();
}

int PatternGrid::getTrackX (int t) const      { return juce::isPositiveAndBelow (t, (int) trackX.size()) ? trackX[(size_t) t] : totalWidth; }
int PatternGrid::getTrackWidth (int t) const  { return juce::isPositiveAndBelow (t, (int) trackW.size()) ? trackW[(size_t) t] : 0; }
int PatternGrid::getTotalWidth() const        { return totalWidth; }

int PatternGrid::getFirstVisibleRow() const
{
    const int visible = getNumVisibleRows();
    const int rows = ctx.engine.song.patterns.empty() ? 64
                   : ctx.engine.song.patterns[(size_t) juce::jlimit (0, (int) ctx.engine.song.patterns.size() - 1, ctx.state.pattern)].numRows;
    return juce::jlimit (0, juce::jmax (0, rows - visible), ctx.state.cursorRow - visible / 2);
}

int PatternGrid::getDisplayedPlayRow() const
{
    auto& e = ctx.engine;
    if (! e.isPlaying()) return -1;
    const int slot = e.playSeqIndex.load();
    if (! juce::isPositiveAndBelow (slot, (int) e.song.sequence.size())) return -1;
    if (e.song.sequence[(size_t) slot] != ctx.state.pattern) return -1;
    return e.playRow.load();
}

float PatternGrid::charWidth() const
{
    static float w = Theme::monoFont (14.0f).getStringWidthFloat ("0");
    return w;
}

int PatternGrid::fieldCharOffset (int f) const
{
    static const int off[kNumFields] = { 0, 5, 6, 9, 10, 13, 14, 15 };
    return off[juce::jlimit (0, kNumFields - 1, f)];
}

int PatternGrid::fieldCharWidth (int f) const { return f == 0 ? 3 : 1; }

//==============================================================================
void PatternGrid::timerCallback()
{
    auto& e = ctx.engine;
    auto& st = ctx.state;
    bool changed = false;

    if (e.isPlaying())
    {
        const int seq = e.playSeqIndex.load();
        const int r = e.playRow.load();
        if (seq != lastPlaySeq || r != lastPlayRow)
        {
            lastPlaySeq = seq;
            lastPlayRow = r;
            changed = true;

            if (st.followPlayback && juce::isPositiveAndBelow (seq, (int) e.song.sequence.size()))
            {
                if (st.seqSlot != seq || st.pattern != e.song.sequence[(size_t) seq])
                {
                    st.seqSlot = seq;
                    st.pattern = e.song.sequence[(size_t) seq];
                    if (ctx.onPatternChanged) ctx.onPatternChanged();
                }
                st.cursorRow = r;
            }
        }
    }
    else if (lastPlayRow >= 0)
    {
        lastPlayRow = -1;
        lastPlaySeq = -1;
        changed = true;
    }

    if (changed)
    {
        repaint();
        if (onViewChanged) onViewChanged();
    }
}

//==============================================================================
void PatternGrid::paint (juce::Graphics& g)
{
    g.fillAll (Theme::gridBg);
    auto* pat = ctx.currentPattern();
    auto& song = ctx.engine.song;
    if (pat == nullptr || song.tracks.empty())
    {
        g.setColour (Theme::gridDim);
        g.setFont (Theme::uiFont (15.0f, 600));
        g.drawText ("Add a track to start writing music", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const auto& st = ctx.state;
    const int rowH = Layout::rowHeight;
    const int first = getFirstVisibleRow();
    const int visible = getNumVisibleRows() + 2;
    const int lpb = juce::jmax (1, song.lpb);
    const int playRow = getDisplayedPlayRow();
    const float cw = charWidth();
    const bool focused = hasKeyboardFocus (false);

    const auto mono = Theme::monoFont (14.0f);
    const auto monoB = Theme::monoFont (14.0f, true);

    int sr0 = 0, sr1 = -1;
    if (st.hasSelection) getSelectionRows (sr0, sr1);

    int loopFirst = -1, loopLast = -1;
    const bool loopActive = ctx.engine.isPlaying() && ctx.engine.isLoopingPattern() && ctx.engine.loopFirstRow.load() >= 0
                            && ctx.engine.playSeqIndex.load() == st.seqSlot;
    if (loopActive)
    {
        loopFirst = ctx.engine.loopFirstRow.load();
        loopLast = ctx.engine.loopLastRow.load();
    }
    else if (st.hasLoopRange (st.pattern))
    {
        loopFirst = st.loopFirst;
        loopLast = st.loopLast;
    }

    // NB: no song.lock here. Pattern data is only ever modified on the message thread
    // (which is the thread painting), and the audio thread only reads it. Holding the
    // lock while drawing hundreds of text runs blocked the real-time audio callback for
    // the whole paint - on macOS (CoreGraphics text rendering, Retina) that easily takes
    // longer than one audio buffer and caused crackling / stuttering during playback.
    for (int i = 0; i < visible; ++i)
    {
        const int r = first + i;
        const int y = i * rowH;
        if (r < 0 || r >= pat->numRows)
            continue;

        const bool beat = (r % lpb) == 0;
        g.setColour (beat ? Theme::gridBeat : ((r & 1) ? Theme::gridRow : Theme::gridRowAlt));
        g.fillRect (0, y, totalWidth, rowH);

        // rows of an active row-range loop
        if (loopFirst >= 0 && r >= loopFirst && r <= loopLast)
        {
            g.setColour (Theme::accent.withAlpha (loopActive ? 0.12f : 0.06f));
            g.fillRect (0, y, totalWidth, rowH);
            g.setColour (Theme::accent.withAlpha (loopActive ? 0.9f : 0.45f));
            if (r == loopFirst) g.fillRect (0, y, totalWidth, 2);
            if (r == loopLast)  g.fillRect (0, y + rowH - 2, totalWidth, 2);
        }

        for (int t = 0; t < (int) song.tracks.size() && t < (int) pat->tracks.size(); ++t)
        {
            const auto& tr = song.tracks[(size_t) t];
            const auto colour = tr.colour;
            const int tx = trackX[(size_t) t];
            const int tw = trackW[(size_t) t];

            if (r == st.cursorRow)
            {
                g.setColour (colour.withAlpha (st.editMode ? 0.30f : 0.18f));
                g.fillRect (tx, y, tw, rowH);
            }
            if (r == playRow && (r != st.cursorRow || ! st.followPlayback))
            {
                g.setColour (Theme::accent.withAlpha (0.22f));
                g.fillRect (tx, y, tw, rowH);
            }

            for (int c = 0; c < tr.noteColumns; ++c)
            {
                const auto& cell = pat->cell (t, r, c);
                const float cx = (float) (tx + c * Layout::noteColumnWidth + kPadLeft);

                if (st.hasSelection && isSelected (t, c, r))
                {
                    g.setColour (Theme::accentBlue.withAlpha (0.32f));
                    g.fillRect ((float) (tx + c * Layout::noteColumnWidth), (float) y, (float) Layout::noteColumnWidth, (float) rowH);
                }

                auto drawField = [&] (int charOff, const juce::String& text, juce::Colour col, bool bold)
                {
                    g.setFont (bold ? monoB : mono);
                    g.setColour (col);
                    g.drawSingleLineText (text, (int) (cx + (float) charOff * cw), y + rowH - 5);
                };

                // note
                if (cell.note == kNoteOff)
                    drawField (0, "OFF", juce::Colour (0xff8fa0bb), true);
                else if (cell.note >= 0)
                    drawField (0, noteString (cell.note), colour.brighter (0.35f), true);
                else
                    drawField (0, "---", Theme::gridDim, false);

                // instrument
                if (cell.ins != 0xFF)
                    drawField (5, hexString ((int) cell.ins), colour.brighter (0.1f).withAlpha (0.95f), true);
                else
                    drawField (5, "--", Theme::gridDim, false);

                // volume
                if (cell.vol != 0xFF)
                    drawField (9, hexString ((int) cell.vol), Theme::gridText, false);
                else
                    drawField (9, "--", Theme::gridDim, false);

                // effect
                if (cell.fxCmd != 0)
                    drawField (13, juce::String::charToString ((juce::juce_wchar) cell.fxCmd) + hexString ((int) cell.fxVal),
                               juce::Colour (0xffffc46b), false);
                else
                    drawField (13, "---", Theme::gridDim, false);
            }
        }
    }

    // separators
    for (int t = 0; t < (int) trackX.size(); ++t)
    {
        const int tx = trackX[(size_t) t];
        g.setColour (Theme::gridLine);
        for (int c = 1; c < song.tracks[(size_t) t].noteColumns; ++c)
            g.fillRect (tx + c * Layout::noteColumnWidth, 0, 1, getHeight());
        g.setColour (juce::Colour (0xff0f1520));
        g.fillRect (tx + trackW[(size_t) t], 0, Layout::trackGap, getHeight());
    }

    // cursor
    if (juce::isPositiveAndBelow (st.cursorTrack, (int) trackX.size()))
    {
        const int i = st.cursorRow - first;
        const float x = (float) (trackX[(size_t) st.cursorTrack] + st.cursorColumn * Layout::noteColumnWidth + kPadLeft)
                        + (float) fieldCharOffset (st.cursorField) * cw - 2.0f;
        const float w = (float) fieldCharWidth (st.cursorField) * cw + 4.0f;
        juce::Rectangle<float> cr (x, (float) (i * rowH) + 1.0f, w, (float) rowH - 2.0f);
        const auto col = st.editMode ? Theme::record : Theme::accent;
        g.setColour (col.withAlpha (focused ? 0.35f : 0.18f));
        g.fillRoundedRectangle (cr, 3.0f);
        g.setColour (col.withAlpha (focused ? 1.0f : 0.5f));
        g.drawRoundedRectangle (cr, 3.0f, 1.2f);
    }
}

//==============================================================================
PatternGrid::HitResult PatternGrid::hitTest (juce::Point<int> p) const
{
    HitResult h;
    auto& song = ctx.engine.song;
    for (int t = 0; t < (int) trackX.size(); ++t)
    {
        if (p.x >= trackX[(size_t) t] && p.x < trackX[(size_t) t] + trackW[(size_t) t] + Layout::trackGap)
        {
            h.track = t;
            const int lx = p.x - trackX[(size_t) t];
            h.column = juce::jlimit (0, song.tracks[(size_t) t].noteColumns - 1, lx / Layout::noteColumnWidth);
            const float fx = (float) (lx - h.column * Layout::noteColumnWidth - kPadLeft) / charWidth();
            if (fx < 4.0f)        h.field = 0;
            else if (fx < 6.0f)   h.field = 1;
            else if (fx < 7.5f)   h.field = 2;
            else if (fx < 10.0f)  h.field = 3;
            else if (fx < 11.5f)  h.field = 4;
            else if (fx < 14.0f)  h.field = 5;
            else if (fx < 15.0f)  h.field = 6;
            else                  h.field = 7;
            break;
        }
    }
    if (h.track < 0 && ! trackX.empty())
        h.track = (int) trackX.size() - 1;

    h.row = getFirstVisibleRow() + p.y / Layout::rowHeight;
    if (auto* pat = ctx.engine.song.patterns.empty() ? nullptr : &ctx.engine.song.patterns[(size_t) ctx.state.pattern])
        h.row = juce::jlimit (0, pat->numRows - 1, h.row);
    return h;
}

void PatternGrid::notifyCursorTrack (int oldTrack)
{
    ctx.engine.midiInputTrack = ctx.state.cursorTrack;
    if (oldTrack != ctx.state.cursorTrack)
    {
        if (onCursorTrackChanged) onCursorTrackChanged();
        if (onRequestTrackVisible) onRequestTrackVisible (ctx.state.cursorTrack);
    }
}

void PatternGrid::setCursor (int track, int column, int field, int row)
{
    auto& st = ctx.state;
    const int old = st.cursorTrack;
    st.cursorTrack = track;
    st.cursorColumn = column;
    st.cursorField = field;
    st.cursorRow = row;
    updateLayout();
    notifyCursorTrack (old);
    if (onViewChanged) onViewChanged();
}

void PatternGrid::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    auto h = hitTest (e.getPosition());
    if (h.track < 0) return;

    auto& st = ctx.state;
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, "Cut", st.hasSelection);
        m.addItem (2, "Copy", st.hasSelection);
        m.addItem (3, "Paste", ! clipboard.empty());
        m.addItem (4, "Delete");
        m.addSeparator();
        m.addItem (5, "Transpose +1");
        m.addItem (6, "Transpose -1");
        m.addItem (7, "Transpose +12");
        m.addItem (8, "Transpose -12");
        m.addItem (9, "Interpolate volume", st.hasSelection);
        m.addItem (10, "Select whole track column");
        m.addSeparator();
        juce::PopupMenu fx;
        for (auto& info : getEffectList())
            fx.addItem (-1, info.description, false);
        m.addSubMenu ("Effect commands", fx);

        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe = juce::Component::SafePointer<PatternGrid> (this)] (int r)
        {
            if (safe == nullptr) return;
            switch (r)
            {
                case 1: safe->copySelection (true); break;
                case 2: safe->copySelection (false); break;
                case 3: safe->paste(); break;
                case 4: safe->deleteSelectionOrCell(); break;
                case 5: safe->transpose (1); break;
                case 6: safe->transpose (-1); break;
                case 7: safe->transpose (12); break;
                case 8: safe->transpose (-12); break;
                case 9: safe->interpolateVolumes(); break;
                case 10: safe->selectAllInTrack(); break;
                default: break;
            }
        });
        return;
    }

    if (e.mods.isShiftDown())
    {
        if (! st.hasSelection)
        {
            st.hasSelection = true;
            st.selStartTrack = st.cursorTrack; st.selStartCol = st.cursorColumn; st.selStartRow = st.cursorRow;
        }
        st.selEndTrack = h.track; st.selEndCol = h.column; st.selEndRow = h.row;
    }
    else
    {
        st.hasSelection = false;
    }

    dragAnchorTrack = h.track;
    dragAnchorCol = h.column;
    dragAnchorRow = h.row;
    setCursor (h.track, h.column, h.field, h.row);
}

void PatternGrid::mouseUp (const juce::MouseEvent& e)
{
    // A plain click on a note field in edit mode opens the note picker.
    if (! e.mouseWasClicked() || e.mods.isPopupMenu() || e.mods.isShiftDown() || e.mods.isCommandDown())
        return;
    auto& st = ctx.state;
    if (! st.editMode || st.hasSelection) return;
    auto h = hitTest (e.getMouseDownPosition());
    if (h.track < 0) return;

    const auto pos = e.getMouseDownScreenPosition();
    showFieldPicker (h.field, { pos.x - 12, pos.y - Layout::rowHeight / 2, 24, Layout::rowHeight });
}

void PatternGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    auto h = hitTest (e.getPosition());
    if (h.track < 0) return;

    auto& st = ctx.state;
    if (h.track != dragAnchorTrack || h.column != dragAnchorCol || h.row != dragAnchorRow || st.hasSelection)
    {
        st.hasSelection = true;
        st.selStartTrack = dragAnchorTrack; st.selStartCol = dragAnchorCol; st.selStartRow = dragAnchorRow;
        st.selEndTrack = h.track; st.selEndCol = h.column; st.selEndRow = h.row;
        repaint();
    }
}

void PatternGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        Component::mouseWheelMove (e, w); // let the viewport scroll horizontally
        return;
    }
    const int delta = w.deltaY > 0 ? -2 : 2;
    moveCursorRow (delta);
}

//==============================================================================
void PatternGrid::moveCursorRow (int delta)
{
    auto* p = ctx.currentPattern();
    if (p == nullptr) return;
    auto& st = ctx.state;
    int r = st.cursorRow + delta;
    if (r < 0) r = (std::abs (delta) == 1) ? p->numRows - 1 : 0;
    else if (r >= p->numRows) r = (std::abs (delta) == 1) ? 0 : p->numRows - 1;
    st.cursorRow = r;
    repaint();
    if (onViewChanged) onViewChanged();
}

void PatternGrid::scrollRows (int delta)
{
    auto* p = ctx.currentPattern();
    if (p == nullptr) return;
    auto& st = ctx.state;
    // the view is centred on the cursor, so first bring the cursor to the centre of the view
    const int visible = getNumVisibleRows();
    const int centre = getFirstVisibleRow() + visible / 2;
    st.cursorRow = juce::jlimit (0, p->numRows - 1, centre + delta);
    repaint();
    if (onViewChanged) onViewChanged();
}

void PatternGrid::moveCursorField (int delta)
{
    auto& st = ctx.state;
    auto& song = ctx.engine.song;
    if (song.tracks.empty()) return;
    const int old = st.cursorTrack;

    st.cursorField += delta;
    if (st.cursorField >= kNumFields)
    {
        st.cursorField = 0;
        if (++st.cursorColumn >= song.tracks[(size_t) st.cursorTrack].noteColumns)
        {
            st.cursorColumn = 0;
            st.cursorTrack = (st.cursorTrack + 1) % (int) song.tracks.size();
        }
    }
    else if (st.cursorField < 0)
    {
        st.cursorField = kNumFields - 1;
        if (--st.cursorColumn < 0)
        {
            st.cursorTrack = (st.cursorTrack - 1 + (int) song.tracks.size()) % (int) song.tracks.size();
            st.cursorColumn = song.tracks[(size_t) st.cursorTrack].noteColumns - 1;
        }
    }
    notifyCursorTrack (old);
    repaint();
}

void PatternGrid::moveCursorTrack (int delta)
{
    auto& st = ctx.state;
    auto& song = ctx.engine.song;
    if (song.tracks.empty()) return;
    const int old = st.cursorTrack;
    st.cursorTrack = (st.cursorTrack + delta + (int) song.tracks.size()) % (int) song.tracks.size();
    st.cursorColumn = 0;
    st.cursorField = 0;
    notifyCursorTrack (old);
    repaint();
}

void PatternGrid::afterCursorMove (bool extend)
{
    auto& st = ctx.state;
    if (extend)
    {
        st.selEndTrack = st.cursorTrack;
        st.selEndCol = st.cursorColumn;
        st.selEndRow = st.cursorRow;
    }
    repaint();
    if (onViewChanged) onViewChanged();
}

//==============================================================================
int PatternGrid::previewInstrument() const
{
    auto& st = ctx.state;
    int inst = st.cursorTrack;
    if (auto* p = ctx.currentPattern())
    {
        const auto& c = p->cell (st.cursorTrack, st.cursorRow, st.cursorColumn);
        if (c.ins != 0xFF && c.ins >= 1 && c.ins <= ctx.engine.song.numTracks())
            inst = c.ins - 1;
    }
    return inst;
}

void PatternGrid::editCursorCell (const juce::String& undoName, const std::function<void (Cell&)>& edit)
{
    auto& st = ctx.state;
    auto* p = ctx.currentPattern();
    if (p == nullptr || ! st.editMode || ctx.engine.song.tracks.empty()) return;

    ctx.beginPatternEdit();
    {
        const juce::ScopedLock sl (ctx.engine.song.lock);
        edit (p->cell (st.cursorTrack, st.cursorRow, st.cursorColumn));
    }
    ctx.endPatternEdit (undoName);

    if (st.editStep > 0) moveCursorRow (st.editStep);
    else repaint();
    grabKeyboardFocus();
}

void PatternGrid::showFieldPicker (int field, juce::Rectangle<int> screenArea)
{
    auto& st = ctx.state;
    auto* p = ctx.currentPattern();
    auto& song = ctx.engine.song;
    if (p == nullptr || song.tracks.empty()) return;

    if (field == 0) { showNotePicker (screenArea); return; }

    const auto cell = p->cell (st.cursorTrack, st.cursorRow, st.cursorColumn);
    juce::Component::SafePointer<PatternGrid> safe (this);
    std::unique_ptr<juce::Component> content;

    if (field <= 2)
    {
        std::vector<InstrumentPicker::Entry> entries;
        for (int t = 0; t < song.numTracks(); ++t)
            entries.push_back ({ song.tracks[(size_t) t].name, ctx.engine.getStrip (t).getInstrumentName(), song.tracks[(size_t) t].colour });
        auto picker = std::make_unique<InstrumentPicker> (std::move (entries), cell.ins == 0xFF ? 0 : cell.ins, st.cursorTrack + 1);
        picker->onInsert = [safe] (int ins)
        {
            if (safe != nullptr)
                safe->editCursorCell ("Set instrument", [ins] (Cell& c) { c.ins = ins <= 0 ? (uint8_t) 0xFF : (uint8_t) juce::jmin (0xFE, ins); });
        };
        content = std::move (picker);
    }
    else if (field <= 4)
    {
        auto picker = std::make_unique<VolumePicker> (cell.vol == 0xFF ? -1 : cell.vol);
        picker->onInsert = [safe] (int vol)
        {
            if (safe != nullptr)
                safe->editCursorCell ("Set volume", [vol] (Cell& c) { c.vol = vol < 0 ? (uint8_t) 0xFF : (uint8_t) juce::jlimit (0, 0x7F, vol); });
        };
        content = std::move (picker);
    }
    else
    {
        auto picker = std::make_unique<EffectPicker> (cell.fxCmd, cell.fxVal);
        picker->onInsert = [safe] (char cmd, int value)
        {
            if (safe != nullptr)
                safe->editCursorCell ("Set effect", [cmd, value] (Cell& c) { c.fxCmd = cmd; c.fxVal = (uint8_t) (cmd == 0 ? 0 : juce::jlimit (0, 0xFF, value)); });
        };
        content = std::move (picker);
    }

    juce::CallOutBox::launchAsynchronously (std::move (content), screenArea, nullptr);
}

void PatternGrid::showNotePicker (juce::Rectangle<int> screenArea)
{
    auto& st = ctx.state;
    auto* p = ctx.currentPattern();
    if (p == nullptr || ctx.engine.song.tracks.empty()) return;

    const int current = p->cell (st.cursorTrack, st.cursorRow, st.cursorColumn).note;
    auto picker = std::make_unique<NotePicker> (current >= 0 ? current : -1, st.octave);
    const int inst = previewInstrument();
    juce::Component::SafePointer<PatternGrid> safe (this);

    picker->onPreview = [safe, inst] (int note, bool down)
    {
        if (safe == nullptr) return;
        if (down) safe->ctx.engine.previewNoteOn (inst, note, 0.8f);
        else      safe->ctx.engine.previewNoteOff (inst, note);
    };
    picker->onInsert = [safe] (int note)
    {
        if (safe == nullptr) return;
        if (note >= 0)
        {
            safe->ctx.state.octave = note / 12;   // the next pick starts in the same octave
            safe->ctx.state.changed();
        }
        safe->ctx.state.cursorField = 0;
        safe->writeNote (note, -1, true);
        safe->grabKeyboardFocus();
    };

    juce::CallOutBox::launchAsynchronously (std::move (picker), screenArea, nullptr);
}

void PatternGrid::writeNote (int note, int velocity, bool advance)
{
    auto& st = ctx.state;
    auto* p = ctx.currentPattern();
    if (p == nullptr || ! st.editMode || ctx.engine.song.tracks.empty()) return;

    ctx.beginPatternEdit();
    {
        const juce::ScopedLock sl (ctx.engine.song.lock);
        auto& c = p->cell (st.cursorTrack, st.cursorRow, st.cursorColumn);
        c.note = (int8_t) note;
        if (note >= 0 && c.ins == 0xFF)
            c.ins = (uint8_t) (st.cursorTrack + 1);
        if (velocity >= 0)
            c.vol = (uint8_t) juce::jlimit (0, 0x7F, velocity);
    }
    ctx.endPatternEdit ("Enter note");

    if (advance && st.editStep > 0)
        moveCursorRow (st.editStep);
    else
        repaint();
}

void PatternGrid::enterMidiNote (int note, int velocity, bool isNoteOn)
{
    if (! isNoteOn)
    {
        midiHeld.erase (std::remove (midiHeld.begin(), midiHeld.end(), note), midiHeld.end());
        return;
    }
    if (note < 0 || note > 119) return;   // the pattern stores C-0 .. B-9
    auto& st = ctx.state;
    if (! st.editMode || ! juce::isPositiveAndBelow (st.cursorTrack, ctx.engine.song.numTracks())) return;

    // Keys pressed while others are still held form a chord: they go into the next note
    // columns of the row where the chord started (if the track has more columns).
    const int columns = ctx.engine.song.tracks[(size_t) st.cursorTrack].noteColumns;
    if (! midiHeld.empty() && chordRow >= 0 && chordTrack == st.cursorTrack && chordNextColumn < columns)
    {
        const int row = st.cursorRow, col = st.cursorColumn;
        st.cursorRow = chordRow;
        st.cursorColumn = chordNextColumn++;
        st.cursorField = 0;
        writeNote (note, velocity, false);
        st.cursorRow = row;
        st.cursorColumn = col;
        repaint();
    }
    else
    {
        chordRow = st.cursorRow;
        chordTrack = st.cursorTrack;
        chordNextColumn = st.cursorColumn + 1;
        st.cursorField = 0;
        writeNote (note, velocity, true);
    }
    midiHeld.push_back (note);
}

bool PatternGrid::handleHexKey (const juce::KeyPress& key)
{
    auto& st = ctx.state;
    auto* p = ctx.currentPattern();
    if (p == nullptr || ! st.editMode) return false;

    const auto ch = juce::CharacterFunctions::toUpperCase (key.getTextCharacter());
    int v = -1;
    if (ch >= '0' && ch <= '9') v = ch - '0';
    else if (ch >= 'A' && ch <= 'F') v = ch - 'A' + 10;

    if (st.cursorField == 5)
    {
        if (! ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')))
            return false;
    }
    else if (v < 0)
    {
        return false;
    }

    ctx.beginPatternEdit();
    {
        const juce::ScopedLock sl (ctx.engine.song.lock);
        auto& c = p->cell (st.cursorTrack, st.cursorRow, st.cursorColumn);
        switch (st.cursorField)
        {
            case 1: { int ins = c.ins == 0xFF ? 0 : c.ins; ins = (v << 4) | (ins & 0x0F); c.ins = (uint8_t) juce::jmin (0xFE, ins); break; }
            case 2: { int ins = c.ins == 0xFF ? 0 : c.ins; ins = (ins & 0xF0) | v; c.ins = (uint8_t) juce::jmin (0xFE, ins); break; }
            case 3: { int vol = c.vol == 0xFF ? 0 : c.vol; vol = (v << 4) | (vol & 0x0F); c.vol = (uint8_t) juce::jmin (0x7F, vol); break; }
            case 4: { int vol = c.vol == 0xFF ? 0 : c.vol; vol = (vol & 0xF0) | v; c.vol = (uint8_t) juce::jmin (0x7F, vol); break; }
            case 5: c.fxCmd = (char) ch; break;
            case 6: if (c.fxCmd == 0) c.fxCmd = '0'; c.fxVal = (uint8_t) ((v << 4) | (c.fxVal & 0x0F)); break;
            case 7: if (c.fxCmd == 0) c.fxCmd = '0'; c.fxVal = (uint8_t) ((c.fxVal & 0xF0) | v); break;
            default: break;
        }
    }
    ctx.endPatternEdit ("Edit value");

    if (st.editStep > 0) moveCursorRow (st.editStep);
    else repaint();
    return true;
}

//==============================================================================
std::vector<PatternGrid::ColRef> PatternGrid::selectedColumns() const
{
    std::vector<ColRef> cols;
    auto& st = ctx.state;
    auto& song = ctx.engine.song;
    if (song.tracks.empty()) return cols;

    int ta = st.selStartTrack, ca = st.selStartCol, tb = st.selEndTrack, cb = st.selEndCol;
    if (tb < ta || (tb == ta && cb < ca)) { std::swap (ta, tb); std::swap (ca, cb); }
    ta = juce::jlimit (0, song.numTracks() - 1, ta);
    tb = juce::jlimit (0, song.numTracks() - 1, tb);

    for (int t = ta; t <= tb; ++t)
    {
        const int n = song.tracks[(size_t) t].noteColumns;
        const int c0 = t == ta ? juce::jmin (ca, n - 1) : 0;
        const int c1 = t == tb ? juce::jmin (cb, n - 1) : n - 1;
        for (int c = c0; c <= c1; ++c)
            cols.push_back ({ t, c });
    }
    return cols;
}

void PatternGrid::getSelectionRows (int& r0, int& r1) const
{
    r0 = juce::jmin (ctx.state.selStartRow, ctx.state.selEndRow);
    r1 = juce::jmax (ctx.state.selStartRow, ctx.state.selEndRow);
}

bool PatternGrid::isSelected (int track, int column, int row) const
{
    int r0, r1;
    getSelectionRows (r0, r1);
    if (row < r0 || row > r1) return false;

    auto& st = ctx.state;
    int ta = st.selStartTrack, ca = st.selStartCol, tb = st.selEndTrack, cb = st.selEndCol;
    if (tb < ta || (tb == ta && cb < ca)) { std::swap (ta, tb); std::swap (ca, cb); }
    const auto key = [] (int t, int c) { return t * 64 + c; };
    const int k = key (track, column);
    return k >= key (ta, ca) && k <= key (tb, cb);
}

void PatternGrid::copySelection (bool cut)
{
    auto* p = ctx.currentPattern();
    if (p == nullptr) return;
    auto& st = ctx.state;

    std::vector<ColRef> cols;
    int r0 = st.cursorRow, r1 = st.cursorRow;
    if (st.hasSelection) { cols = selectedColumns(); getSelectionRows (r0, r1); }
    else cols.push_back ({ st.cursorTrack, st.cursorColumn });

    clipboard.clear();
    if (cut) ctx.beginPatternEdit();
    {
        const juce::ScopedLock sl (ctx.engine.song.lock);
        for (auto& c : cols)
        {
            std::vector<Cell> column;
            for (int r = r0; r <= r1 && r < p->numRows; ++r)
            {
                column.push_back (p->cell (c.track, r, c.column));
                if (cut) p->cell (c.track, r, c.column).clear();
            }
            clipboard.push_back (column);
        }
    }
    if (cut) ctx.endPatternEdit ("Cut");
    repaint();
}

void PatternGrid::paste()
{
    auto* p = ctx.currentPattern();
    if (p == nullptr || clipboard.empty()) return;
    auto& st = ctx.state;
    auto& song = ctx.engine.song;

    ctx.beginPatternEdit();
    {
        const juce::ScopedLock sl (song.lock);
        int t = st.cursorTrack, c = st.cursorColumn;
        for (auto& column : clipboard)
        {
            if (t >= song.numTracks()) break;
            for (size_t i = 0; i < column.size(); ++i)
            {
                const int r = st.cursorRow + (int) i;
                if (r >= p->numRows) break;
                p->cell (t, r, c) = column[i];
            }
            if (++c >= song.tracks[(size_t) t].noteColumns) { c = 0; ++t; }
        }
    }
    ctx.endPatternEdit ("Paste");
    repaint();
}

void PatternGrid::deleteSelectionOrCell()
{
    auto* p = ctx.currentPattern();
    if (p == nullptr) return;
    auto& st = ctx.state;

    ctx.beginPatternEdit();
    {
        const juce::ScopedLock sl (ctx.engine.song.lock);
        if (st.hasSelection)
        {
            int r0, r1;
            getSelectionRows (r0, r1);
            for (auto& c : selectedColumns())
                for (int r = r0; r <= r1 && r < p->numRows; ++r)
                    p->cell (c.track, r, c.column).clear();
        }
        else
        {
            auto& c = p->cell (st.cursorTrack, st.cursorRow, st.cursorColumn);
            switch (st.cursorField)
            {
                case 0: c.clear(); break;
                case 1: case 2: c.ins = 0xFF; break;
                case 3: case 4: c.vol = 0xFF; break;
                default: c.fxCmd = 0; c.fxVal = 0; break;
            }
        }
    }
    ctx.endPatternEdit ("Delete");

    if (! st.hasSelection && st.editStep > 0)
        moveCursorRow (st.editStep);
    repaint();
}

void PatternGrid::transpose (int semis)
{
    auto* p = ctx.currentPattern();
    if (p == nullptr) return;
    auto& st = ctx.state;

    std::vector<ColRef> cols;
    int r0 = st.cursorRow, r1 = st.cursorRow;
    if (st.hasSelection) { cols = selectedColumns(); getSelectionRows (r0, r1); }
    else cols.push_back ({ st.cursorTrack, st.cursorColumn });

    ctx.beginPatternEdit();
    {
        const juce::ScopedLock sl (ctx.engine.song.lock);
        for (auto& c : cols)
            for (int r = r0; r <= r1 && r < p->numRows; ++r)
            {
                auto& cell = p->cell (c.track, r, c.column);
                if (cell.note >= 0)
                    cell.note = (int8_t) juce::jlimit (0, 119, cell.note + semis);
            }
    }
    ctx.endPatternEdit ("Transpose");
    repaint();
}

void PatternGrid::interpolateVolumes()
{
    auto* p = ctx.currentPattern();
    if (p == nullptr || ! ctx.state.hasSelection) return;
    int r0, r1;
    getSelectionRows (r0, r1);
    if (r1 <= r0) return;

    ctx.beginPatternEdit();
    {
        const juce::ScopedLock sl (ctx.engine.song.lock);
        for (auto& c : selectedColumns())
        {
            auto& a = p->cell (c.track, r0, c.column);
            auto& b = p->cell (c.track, r1, c.column);
            const int va = a.vol != 0xFF ? a.vol : 0x7F;
            const int vb = b.vol != 0xFF ? b.vol : 0x00;
            for (int r = r0; r <= r1; ++r)
            {
                const float t = (float) (r - r0) / (float) (r1 - r0);
                p->cell (c.track, r, c.column).vol = (uint8_t) juce::roundToInt ((float) va + t * (float) (vb - va));
            }
        }
    }
    ctx.endPatternEdit ("Interpolate");
    repaint();
}

void PatternGrid::selectAllInTrack()
{
    auto* p = ctx.currentPattern();
    auto& st = ctx.state;
    if (p == nullptr || ctx.engine.song.tracks.empty()) return;
    st.hasSelection = true;
    st.selStartTrack = st.selEndTrack = st.cursorTrack;
    st.selStartCol = 0;
    st.selEndCol = ctx.engine.song.tracks[(size_t) st.cursorTrack].noteColumns - 1;
    st.selStartRow = 0;
    st.selEndRow = p->numRows - 1;
    repaint();
}

void PatternGrid::insertRow()
{
    auto* p = ctx.currentPattern();
    if (p == nullptr) return;
    auto& st = ctx.state;
    ctx.beginPatternEdit();
    {
        const juce::ScopedLock sl (ctx.engine.song.lock);
        for (int r = p->numRows - 1; r > st.cursorRow; --r)
            p->cell (st.cursorTrack, r, st.cursorColumn) = p->cell (st.cursorTrack, r - 1, st.cursorColumn);
        p->cell (st.cursorTrack, st.cursorRow, st.cursorColumn).clear();
    }
    ctx.endPatternEdit ("Insert row");
    repaint();
}

void PatternGrid::deleteRow()
{
    auto* p = ctx.currentPattern();
    if (p == nullptr) return;
    auto& st = ctx.state;
    ctx.beginPatternEdit();
    {
        const juce::ScopedLock sl (ctx.engine.song.lock);
        for (int r = st.cursorRow; r < p->numRows - 1; ++r)
            p->cell (st.cursorTrack, r, st.cursorColumn) = p->cell (st.cursorTrack, r + 1, st.cursorColumn);
        p->cell (st.cursorTrack, p->numRows - 1, st.cursorColumn).clear();
    }
    ctx.endPatternEdit ("Delete row");
    repaint();
}

//==============================================================================
bool PatternGrid::keyPressed (const juce::KeyPress& key)
{
    auto& st = ctx.state;
    auto* p = ctx.currentPattern();
    if (p == nullptr || ctx.engine.song.tracks.empty())
        return false;

    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();
    const bool shift = mods.isShiftDown();
    const bool cmd = mods.isCommandDown();

    // clipboard & undo
    if (cmd && ! mods.isAltDown())
    {
        const auto ch = juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) code);
        if (ch == 'c') { copySelection (false); return true; }
        if (ch == 'x') { copySelection (true); return true; }
        if (ch == 'v') { paste(); return true; }
        if (ch == 'a') { selectAllInTrack(); return true; }
        if (code == juce::KeyPress::upKey && shift)   { transpose (12); return true; }
        if (code == juce::KeyPress::downKey && shift) { transpose (-12); return true; }
        if (code == juce::KeyPress::upKey)   { transpose (1); return true; }
        if (code == juce::KeyPress::downKey) { transpose (-1); return true; }
        if (code == juce::KeyPress::leftKey)  { moveCursorTrack (-1); afterCursorMove (false); return true; }
        if (code == juce::KeyPress::rightKey) { moveCursorTrack (1); afterCursorMove (false); return true; }
        return false;
    }

    // navigation
    auto beginSel = [&]
    {
        if (shift && ! st.hasSelection)
        {
            st.hasSelection = true;
            st.selStartTrack = st.cursorTrack; st.selStartCol = st.cursorColumn; st.selStartRow = st.cursorRow;
        }
        else if (! shift)
        {
            st.hasSelection = false;
        }
    };

    if (code == juce::KeyPress::upKey)       { beginSel(); moveCursorRow (-1); afterCursorMove (shift); return true; }
    if (code == juce::KeyPress::downKey)     { beginSel(); moveCursorRow (1);  afterCursorMove (shift); return true; }
    if (code == juce::KeyPress::pageUpKey)   { beginSel(); moveCursorRow (-juce::jmax (4, ctx.engine.song.lpb * 4)); afterCursorMove (shift); return true; }
    if (code == juce::KeyPress::pageDownKey) { beginSel(); moveCursorRow (juce::jmax (4, ctx.engine.song.lpb * 4)); afterCursorMove (shift); return true; }
    if (code == juce::KeyPress::homeKey)     { beginSel(); st.cursorRow = 0; afterCursorMove (shift); return true; }
    if (code == juce::KeyPress::endKey)      { beginSel(); st.cursorRow = p->numRows - 1; afterCursorMove (shift); return true; }
    if (code == juce::KeyPress::leftKey)
    {
        beginSel();
        moveCursorField (shift ? -kNumFields : -1);
        afterCursorMove (shift);
        return true;
    }
    if (code == juce::KeyPress::rightKey)
    {
        beginSel();
        moveCursorField (shift ? kNumFields : 1);
        afterCursorMove (shift);
        return true;
    }
    if (code == juce::KeyPress::tabKey)      { st.hasSelection = false; moveCursorTrack (shift ? -1 : 1); afterCursorMove (false); return true; }

    // octave
    if (code == juce::KeyPress::numberPadMultiply) { st.octave = juce::jmin (9, st.octave + 1); st.changed(); return true; }
    if (code == juce::KeyPress::numberPadDivide)   { st.octave = juce::jmax (0, st.octave - 1); st.changed(); return true; }

    if (code == juce::KeyPress::escapeKey)
    {
        st.editMode = ! st.editMode;
        st.changed();
        repaint();
        return true;
    }

    if (code == juce::KeyPress::deleteKey)    { if (st.editMode || st.hasSelection) deleteSelectionOrCell(); return true; }
    if (code == juce::KeyPress::backspaceKey) { if (st.editMode) deleteRow(); return true; }
    if (code == juce::KeyPress::insertKey)    { if (st.editMode) insertRow(); return true; }

    if (mods.isAltDown() || mods.isCtrlDown())
        return false;

    if (st.cursorField == 0)
        return false;   // notes are entered with the note picker (click the field) or a MIDI keyboard

    return handleHexKey (key);
}

} // namespace artilody
