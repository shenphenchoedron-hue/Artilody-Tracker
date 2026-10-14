#include "PatternGrid.h"

namespace artilody
{

static constexpr int kPadLeft = 9;
static constexpr int kNumFields = 8;

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

    const juce::ScopedLock sl (song.lock);

    for (int i = 0; i < visible; ++i)
    {
        const int r = first + i;
        const int y = i * rowH;
        if (r < 0 || r >= pat->numRows)
            continue;

        const bool beat = (r % lpb) == 0;
        g.setColour (beat ? Theme::gridBeat : ((r & 1) ? Theme::gridRow : Theme::gridRowAlt));
        g.fillRect (0, y, totalWidth, rowH);

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
                    drawField (0, noteToString (cell.note), colour.brighter (0.35f), true);
                else
                    drawField (0, "---", Theme::gridDim, false);

                // instrument
                if (cell.ins != 0xFF)
                    drawField (5, juce::String::toHexString ((int) cell.ins).paddedLeft ('0', 2).toUpperCase(), colour.brighter (0.1f).withAlpha (0.95f), true);
                else
                    drawField (5, "--", Theme::gridDim, false);

                // volume
                if (cell.vol != 0xFF)
                    drawField (9, juce::String::toHexString ((int) cell.vol).paddedLeft ('0', 2).toUpperCase(), Theme::gridText, false);
                else
                    drawField (9, "--", Theme::gridDim, false);

                // effect
                if (cell.fxCmd != 0)
                    drawField (13, juce::String::charToString ((juce::juce_wchar) cell.fxCmd) + juce::String::toHexString ((int) cell.fxVal).paddedLeft ('0', 2).toUpperCase(),
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
static int noteOffsetForKey (juce::juce_wchar ch)
{
    switch (juce::CharacterFunctions::toLowerCase (ch))
    {
        case 'z': return 0;  case 's': return 1;  case 'x': return 2;  case 'd': return 3;
        case 'c': return 4;  case 'v': return 5;  case 'g': return 6;  case 'b': return 7;
        case 'h': return 8;  case 'n': return 9;  case 'j': return 10; case 'm': return 11;
        case ',': return 12; case 'l': return 13; case '.': return 14;
        case 'q': return 12; case '2': return 13; case 'w': return 14; case '3': return 15;
        case 'e': return 16; case 'r': return 17; case '5': return 18; case 't': return 19;
        case '6': return 20; case 'y': return 21; case '7': return 22; case 'u': return 23;
        case 'i': return 24; case '9': return 25; case 'o': return 26; case '0': return 27;
        case 'p': return 28;
        default: return -1;
    }
}

void PatternGrid::previewNote (int note, float velocity)
{
    auto& st = ctx.state;
    int inst = st.cursorTrack;
    if (auto* p = ctx.currentPattern())
    {
        const auto& c = p->cell (st.cursorTrack, st.cursorRow, st.cursorColumn);
        if (c.ins != 0xFF && c.ins >= 1 && c.ins <= ctx.engine.song.numTracks())
            inst = c.ins - 1;
    }
    ctx.engine.previewNoteOn (inst, note, velocity);
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

void PatternGrid::enterMidiNote (int note, int velocity)
{
    if (! ctx.state.editMode) return;
    ctx.state.cursorField = 0;
    writeNote (note, velocity, true);
}

bool PatternGrid::handleNoteKey (const juce::KeyPress& key)
{
    auto& st = ctx.state;
    const auto ch = key.getTextCharacter();

    if (ch == '1' || ch == 'a' || ch == 'A')
    {
        writeNote (kNoteOff, -1, true);
        return true;
    }

    const int off = noteOffsetForKey (ch);
    if (off < 0) return false;

    for (auto& h : heldKeys)
        if (h.keyCode == key.getKeyCode())
            return true; // auto-repeat

    const int note = juce::jlimit (0, 119, st.octave * 12 + off);
    previewNote (note, 0.8f);

    int inst = st.cursorTrack;
    if (auto* p = ctx.currentPattern())
    {
        const auto& c = p->cell (st.cursorTrack, st.cursorRow, st.cursorColumn);
        if (c.ins != 0xFF && c.ins >= 1 && c.ins <= ctx.engine.song.numTracks())
            inst = c.ins - 1;
    }
    heldKeys.push_back ({ key.getKeyCode(), note, inst });

    writeNote (note, -1, true);
    return true;
}

bool PatternGrid::keyStateChanged (bool)
{
    for (size_t i = 0; i < heldKeys.size();)
    {
        if (! juce::KeyPress::isKeyCurrentlyDown (heldKeys[i].keyCode))
        {
            ctx.engine.previewNoteOff (heldKeys[i].track, heldKeys[i].note);
            heldKeys.erase (heldKeys.begin() + (long) i);
        }
        else
        {
            ++i;
        }
    }
    return false;
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
        if (ch == 'i') { interpolateVolumes(); return true; }
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
        return handleNoteKey (key);

    return handleHexKey (key);
}

} // namespace artilody
