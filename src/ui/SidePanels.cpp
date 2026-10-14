#include "SidePanels.h"
#include "PatternGrid.h"

namespace artilody
{

static juce::String hex2 (int v) { return juce::String::toHexString (v).paddedLeft ('0', 2).toUpperCase(); }

//==============================================================================
SequenceList::SequenceList (AppContext& c) : ctx (c)
{
    addButton.drawBackground = false;
    addButton.setTooltip ("Insert a new pattern after the selected slot");
    addButton.onClick = [this] { insertNewPattern(); };
    addAndMakeVisible (addButton);
    startTimerHz (15);
}

void SequenceList::timerCallback()
{
    const int p = ctx.engine.isPlaying() ? ctx.engine.playSeqIndex.load() : -1;
    if (p != lastPlaying)
    {
        lastPlaying = p;
        repaint();
    }
}

void SequenceList::resized()
{
    addButton.setBounds (getWidth() - 30, 8, 22, 22);
}

int SequenceList::slotAt (juce::Point<int> p) const
{
    if (p.y < headerH) return -1;
    const int s = (p.y - headerH) / rowH + scroll;
    return juce::isPositiveAndBelow (s, (int) ctx.engine.song.sequence.size()) ? s : -1;
}

void SequenceList::selectSlot (int slot)
{
    auto& song = ctx.engine.song;
    if (! juce::isPositiveAndBelow (slot, (int) song.sequence.size())) return;
    ctx.state.seqSlot = slot;
    ctx.state.pattern = song.sequence[(size_t) slot];
    const int visible = (getHeight() - headerH) / rowH;
    if (slot < scroll) scroll = slot;
    else if (slot >= scroll + visible) scroll = slot - visible + 1;
    if (ctx.onPatternChanged) ctx.onPatternChanged();
    repaint();
}

void SequenceList::insertNewPattern()
{
    auto& song = ctx.engine.song;
    const int rows = ctx.currentPattern() != nullptr ? ctx.currentPattern()->numRows : 64;
    {
        const juce::ScopedLock sl (song.lock);
        const int p = song.addPattern (rows, "Pattern " + hex2 ((int) song.patterns.size()));
        const int at = juce::jlimit (0, (int) song.sequence.size(), ctx.state.seqSlot + 1);
        song.sequence.insert (song.sequence.begin() + at, p);
        ctx.state.seqSlot = at;
        ctx.state.pattern = p;
    }
    ctx.state.dirty = true;
    if (ctx.onStructureChanged) ctx.onStructureChanged();
    selectSlot (ctx.state.seqSlot);
}

void SequenceList::mouseDown (const juce::MouseEvent& e)
{
    const int s = slotAt (e.getPosition());
    if (s < 0) return;
    selectSlot (s);
    if (e.mods.isPopupMenu())
        showSlotMenu (s);
}

void SequenceList::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int visible = juce::jmax (1, (getHeight() - headerH) / rowH);
    const int maxScroll = juce::jmax (0, (int) ctx.engine.song.sequence.size() - visible);
    scroll = juce::jlimit (0, maxScroll, scroll + (w.deltaY > 0 ? -1 : 1));
    repaint();
}

void SequenceList::showSlotMenu (int slot)
{
    auto& song = ctx.engine.song;
    juce::PopupMenu m;
    m.addSectionHeader ("Sequence slot " + hex2 (slot));
    m.addItem (1, "Insert new pattern after");
    m.addItem (2, "Repeat this pattern after");
    m.addItem (3, "Duplicate pattern into new slot");
    juce::PopupMenu setP;
    for (int i = 0; i < (int) song.patterns.size(); ++i)
        setP.addItem (100 + i, hex2 (i) + "  " + song.patterns[(size_t) i].name, true, song.sequence[(size_t) slot] == i);
    m.addSubMenu ("Use pattern", setP);
    m.addSeparator();
    m.addItem (4, "Move up", slot > 0);
    m.addItem (5, "Move down", slot < (int) song.sequence.size() - 1);
    m.addItem (6, "Remove slot", song.sequence.size() > 1);
    m.addSeparator();
    m.addItem (7, "Play from here");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, slot] (int r)
    {
        auto& s = ctx.engine.song;
        if (r <= 0 || ! juce::isPositiveAndBelow (slot, (int) s.sequence.size())) return;
        {
            const juce::ScopedLock sl (s.lock);
            if (r == 1)
            {
                ctx.state.seqSlot = slot;
                insertNewPattern();
                return;
            }
            if (r == 2) s.sequence.insert (s.sequence.begin() + slot + 1, s.sequence[(size_t) slot]), ctx.state.seqSlot = slot + 1;
            if (r == 3)
            {
                const int p = s.duplicatePattern (s.sequence[(size_t) slot]);
                if (p >= 0)
                {
                    s.sequence.insert (s.sequence.begin() + slot + 1, p);
                    ctx.state.seqSlot = slot + 1;
                }
            }
            if (r >= 100) s.sequence[(size_t) slot] = r - 100;
            if (r == 4) std::swap (s.sequence[(size_t) slot], s.sequence[(size_t) slot - 1]), ctx.state.seqSlot = slot - 1;
            if (r == 5) std::swap (s.sequence[(size_t) slot], s.sequence[(size_t) slot + 1]), ctx.state.seqSlot = slot + 1;
            if (r == 6)
            {
                s.sequence.erase (s.sequence.begin() + slot);
                ctx.state.seqSlot = juce::jmin (slot, (int) s.sequence.size() - 1);
            }
        }
        if (r == 7)
        {
            ctx.engine.play (slot, 0, false);
            return;
        }
        ctx.state.dirty = true;
        if (ctx.onStructureChanged) ctx.onStructureChanged();
        selectSlot (ctx.state.seqSlot);
    });
}

void SequenceList::paint (juce::Graphics& g)
{
    auto& song = ctx.engine.song;
    Theme::drawCard (g, getLocalBounds().toFloat().reduced (1.0f), 10.0f);

    g.setColour (Theme::text);
    g.setFont (Theme::uiFont (14.5f, 700));
    g.drawText ("Seq", 12, 8, 60, 22, juce::Justification::centredLeft);

    const int visible = (getHeight() - headerH) / rowH + 1;
    for (int i = 0; i < visible; ++i)
    {
        const int s = scroll + i;
        if (s >= (int) song.sequence.size()) break;
        const int p = song.sequence[(size_t) s];
        const int y = headerH + i * rowH;
        auto row = juce::Rectangle<int> (5, y + 1, getWidth() - 10, rowH - 2);

        const bool selected = s == ctx.state.seqSlot;
        const bool playingHere = ctx.engine.isPlaying() && ctx.engine.playSeqIndex.load() == s;
        if (selected)
        {
            g.setColour (Theme::selection);
            g.fillRoundedRectangle (row.toFloat(), 6.0f);
        }

        g.setColour (selected ? Theme::accentBlue : Theme::textDim);
        g.setFont (Theme::uiFont (12.5f, selected ? 700 : 400));
        g.drawText (hex2 (s), row.withWidth (26).translated (3, 0), juce::Justification::centredLeft);

        auto block = juce::Rectangle<int> (row.getX() + 32, row.getY() + 3, row.getWidth() - 36, row.getHeight() - 6).toFloat();
        const auto col = juce::isPositiveAndBelow (p, (int) song.patterns.size()) ? song.patterns[(size_t) p].colour : Theme::textFaint;
        g.setColour (playingHere ? col.brighter (0.2f) : col);
        g.fillRoundedRectangle (block, 5.0f);
        if (playingHere)
        {
            Theme::drawIcon (g, Theme::Icon::Play, block.withSizeKeepingCentre (14, 14), juce::Colours::white);
        }
        else
        {
            g.setColour (juce::Colours::white);
            g.setFont (Theme::uiFont (11.5f, 700));
            g.drawText (hex2 (p), block, juce::Justification::centred);
        }
    }
}

//==============================================================================
RowNumbers::RowNumbers (AppContext& c, PatternGrid& g) : ctx (c), grid (g)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

int RowNumbers::rowAt (int y) const
{
    auto* p = ctx.currentPattern();
    if (p == nullptr) return -1;
    return juce::jlimit (0, p->numRows - 1, grid.getFirstVisibleRow() + y / Layout::rowHeight);
}

void RowNumbers::mouseDown (const juce::MouseEvent& e)
{
    auto& st = ctx.state;
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, "Clear loop rows", st.hasLoopRange (st.pattern));
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe = juce::Component::SafePointer<RowNumbers> (this)] (int r)
        {
            if (safe == nullptr || r != 1) return;
            safe->ctx.state.clearLoopRange();
            safe->repaint();
            if (safe->ctx.onLoopRangeChanged) safe->ctx.onLoopRangeChanged();
        });
        return;
    }
    const int r = rowAt (e.y);
    if (r < 0) return;

    // Shift+click extends the marking from its first row - scroll in between to reach rows further away
    if (e.mods.isShiftDown() && rangeAnchor >= 0 && st.hasLoopRange (st.pattern))
    {
        anchorRow = rangeAnchor;
        st.loopFirst = juce::jmin (rangeAnchor, r);
        st.loopLast = juce::jmax (rangeAnchor, r);
    }
    else
    {
        anchorRow = rangeAnchor = r;
        st.loopPattern = st.pattern;
        st.loopFirst = st.loopLast = r;
    }
    beginDragAutoRepeat (40);   // keeps mouseDrag coming so dragging past the edge auto-scrolls
    repaint();
    grid.repaint();
}

void RowNumbers::mouseDrag (const juce::MouseEvent& e)
{
    if (anchorRow < 0 || e.mods.isPopupMenu()) return;

    // dragging above / below the visible rows scrolls the pattern
    if (e.y < 0)                grid.scrollRows (-1);
    else if (e.y > getHeight()) grid.scrollRows (1);

    const int r = rowAt (juce::jlimit (0, juce::jmax (0, getHeight() - 1), e.y));
    auto& st = ctx.state;
    st.loopFirst = juce::jmin (anchorRow, r);
    st.loopLast = juce::jmax (anchorRow, r);
    repaint();
    grid.repaint();
}

void RowNumbers::mouseUp (const juce::MouseEvent& e)
{
    if (anchorRow < 0 || e.mods.isPopupMenu()) return;
    anchorRow = -1;
    if (ctx.onLoopRangeChanged) ctx.onLoopRangeChanged();
}

void RowNumbers::mouseDoubleClick (const juce::MouseEvent&)
{
    ctx.state.clearLoopRange();
    repaint();
    grid.repaint();
    if (ctx.onLoopRangeChanged) ctx.onLoopRangeChanged();
}

void RowNumbers::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    // always scroll the rows here - also with Shift held (macOS turns Shift+wheel into a horizontal
    // wheel), so you can mark a row, scroll and Shift+click a row further down
    const float d = std::abs (w.deltaY) >= std::abs (w.deltaX) ? w.deltaY : w.deltaX;
    if (d == 0.0f) return;
    grid.scrollRows (d > 0 ? -2 : 2);
    repaint();
}

void RowNumbers::paint (juce::Graphics& g)
{
    auto* p = ctx.currentPattern();
    if (p == nullptr) return;

    const int first = grid.getFirstVisibleRow();
    const int visible = grid.getNumVisibleRows() + 2;
    const int lpb = juce::jmax (1, ctx.engine.song.lpb);
    const int playRow = grid.getDisplayedPlayRow();

    // marked loop rows: a rounded band behind the numbers
    const auto& st = ctx.state;
    if (st.hasLoopRange (st.pattern))
    {
        const int y0 = (st.loopFirst - first) * Layout::rowHeight, y1 = (st.loopLast - first + 1) * Layout::rowHeight;
        const bool active = ctx.engine.isPlaying() && ctx.engine.isLoopingPattern() && ctx.engine.loopFirstRow.load() >= 0;
        auto band = juce::Rectangle<float> (1.0f, (float) y0 + 1.0f, (float) getWidth() - 2.0f, (float) (y1 - y0) - 2.0f);
        g.setColour (Theme::accent.withAlpha (active ? 0.30f : 0.16f));
        g.fillRoundedRectangle (band, 5.0f);
        g.setColour (Theme::accent.withAlpha (active ? 1.0f : 0.7f));
        g.drawRoundedRectangle (band, 5.0f, 1.5f);
    }

    g.setFont (Theme::monoFont (13.0f, false));
    for (int i = 0; i < visible; ++i)
    {
        const int r = first + i;
        if (r < 0 || r >= p->numRows) continue;
        auto rect = juce::Rectangle<int> (0, i * Layout::rowHeight, getWidth(), Layout::rowHeight);

        if (r == ctx.state.cursorRow)
        {
            g.setColour (Theme::selection);
            g.fillRoundedRectangle (rect.toFloat().reduced (2.0f, 1.0f), 4.0f);
        }
        if (r == playRow)
        {
            g.setColour (Theme::accent);
            g.fillRoundedRectangle ((float) rect.getX() + 1.0f, (float) rect.getY() + 3.0f, 3.0f, (float) rect.getHeight() - 6.0f, 1.5f);
        }

        const bool beat = r % lpb == 0;
        g.setColour (r == ctx.state.cursorRow ? Theme::accentBlue : (beat ? Theme::text : Theme::textFaint));
        g.setFont (Theme::monoFont (13.0f, beat || r == ctx.state.cursorRow));
        g.drawText (hex2 (r), rect.withTrimmedRight (6), juce::Justification::centredRight);
    }
}

//==============================================================================
class RightPanel::PatternList final : public juce::Component
{
public:
    PatternList (RightPanel& o) : owner (o) {}

    static constexpr int rowH = 27;

    int idealHeight() const { return (int) owner.ctx.engine.song.patterns.size() * rowH; }

    void paint (juce::Graphics& g) override
    {
        auto& song = owner.ctx.engine.song;
        for (int i = 0; i < (int) song.patterns.size(); ++i)
        {
            auto r = juce::Rectangle<int> (0, i * rowH, getWidth(), rowH);
            const auto& p = song.patterns[(size_t) i];
            const bool sel = i == owner.ctx.state.pattern;
            if (sel)
            {
                g.setColour (Theme::selection);
                g.fillRoundedRectangle (r.toFloat().reduced (0, 1.0f), 6.0f);
            }
            g.setColour (sel ? Theme::accentBlue : Theme::textDim);
            g.setFont (Theme::uiFont (12.5f, sel ? 700 : 400));
            g.drawText (hex2 (i), r.withWidth (30).translated (8, 0), juce::Justification::centredLeft);

            g.setColour (p.colour);
            g.fillRoundedRectangle ((float) r.getX() + 38.0f, (float) r.getY() + 6.0f, 15.0f, 15.0f, 4.0f);

            g.setColour (sel ? Theme::accentBlue : Theme::text);
            g.setFont (Theme::uiFont (13.5f, sel ? 600 : 400));
            g.drawFittedText (p.name, r.withTrimmedLeft (62).withTrimmedRight (60), juce::Justification::centredLeft, 1, 0.8f);

            g.setColour (Theme::textFaint);
            g.setFont (Theme::uiFont (11.0f));
            g.drawText (juce::String (p.numRows) + " rows", r.withTrimmedRight (8), juce::Justification::centredRight);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& song = owner.ctx.engine.song;
        const int i = e.y / rowH;
        if (! juce::isPositiveAndBelow (i, (int) song.patterns.size())) return;

        owner.ctx.state.pattern = i;
        // jump to the first sequence slot that uses this pattern
        for (int s = 0; s < (int) song.sequence.size(); ++s)
            if (song.sequence[(size_t) s] == i) { owner.ctx.state.seqSlot = s; break; }
        if (owner.ctx.onPatternChanged) owner.ctx.onPatternChanged();

        if (e.mods.isPopupMenu())
            owner.patternMenuButton.triggerClick();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        auto& song = owner.ctx.engine.song;
        const int i = e.y / rowH;
        if (! juce::isPositiveAndBelow (i, (int) song.patterns.size())) return;
        if (juce::isPositiveAndBelow (owner.ctx.state.seqSlot, (int) song.sequence.size()))
        {
            const juce::ScopedLock sl (song.lock);
            song.sequence[(size_t) owner.ctx.state.seqSlot] = i;
        }
        owner.ctx.state.dirty = true;
        if (owner.ctx.onStructureChanged) owner.ctx.onStructureChanged();
    }

    RightPanel& owner;
};

RightPanel::RightPanel (AppContext& c) : ctx (c)
{
    patternList = std::make_unique<PatternList> (*this);
    addAndMakeVisible (*patternList);

    for (auto* b : { &addPatternButton, &patternMenuButton })
    {
        b->drawBackground = false;
        addAndMakeVisible (b);
    }
    addPatternButton.setTooltip ("New pattern");
    addPatternButton.onClick = [this]
    {
        auto& song = ctx.engine.song;
        {
            const juce::ScopedLock sl (song.lock);
            ctx.state.pattern = song.addPattern (ctx.currentPattern() != nullptr ? ctx.currentPattern()->numRows : 64,
                                                 "Pattern " + hex2 ((int) song.patterns.size()));
        }
        ctx.state.dirty = true;
        if (ctx.onStructureChanged) ctx.onStructureChanged();
    };

    patternMenuButton.onClick = [this]
    {
        auto& song = ctx.engine.song;
        juce::PopupMenu m;
        m.addItem (1, "Duplicate pattern");
        m.addItem (2, "Clear pattern");
        m.addItem (3, "Use in selected sequence slot");
        m.addItem (4, "Append to end of sequence");
        m.addSeparator();
        m.addItem (5, "Delete pattern", song.patterns.size() > 1);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&patternMenuButton), [this] (int r)
        {
            auto& s = ctx.engine.song;
            const int p = ctx.state.pattern;
            if (r <= 0 || ! juce::isPositiveAndBelow (p, (int) s.patterns.size())) return;
            if (r == 2)
            {
                ctx.beginPatternEdit();
                {
                    const juce::ScopedLock sl (s.lock);
                    for (auto& t : s.patterns[(size_t) p].tracks)
                        for (auto& cell : t) cell.clear();
                }
                ctx.endPatternEdit ("Clear pattern");
            }
            else
            {
                const juce::ScopedLock sl (s.lock);
                if (r == 1) ctx.state.pattern = s.duplicatePattern (p);
                if (r == 3 && juce::isPositiveAndBelow (ctx.state.seqSlot, (int) s.sequence.size())) s.sequence[(size_t) ctx.state.seqSlot] = p;
                if (r == 4) s.sequence.push_back (p);
                if (r == 5)
                {
                    ctx.undo.clearUndoHistory();
                    s.removePattern (p);
                    ctx.state.seqSlot = juce::jlimit (0, (int) s.sequence.size() - 1, ctx.state.seqSlot);
                    ctx.state.pattern = s.sequence[(size_t) ctx.state.seqSlot];
                }
            }
            ctx.state.dirty = true;
            if (ctx.onStructureChanged) ctx.onStructureChanged();
        });
    };

    nameEditor.setFont (Theme::uiFont (13.5f, 600));
    nameEditor.setIndents (8, 5);
    nameEditor.onTextChange = [this]
    {
        if (updating) return;
        if (auto* p = ctx.currentPattern())
        {
            p->name = nameEditor.getText();
            ctx.state.dirty = true;
            patternList->repaint();
        }
    };
    nameEditor.onReturnKey = [this] { if (ctx.onPatternChanged) ctx.onPatternChanged(); };
    addAndMakeVisible (nameEditor);

    for (int len : { 8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512 })
        lengthBox.addItem (juce::String (len), len);
    lengthBox.onChange = [this]
    {
        if (updating) return;
        const int len = lengthBox.getSelectedId();
        if (len <= 0) return;
        ctx.beginPatternEdit();
        {
            const juce::ScopedLock sl (ctx.engine.song.lock);
            if (auto* p = ctx.currentPattern()) p->setNumRows (len);
        }
        ctx.undo.clearUndoHistory();
        ctx.state.dirty = true;
        if (ctx.onStructureChanged) ctx.onStructureChanged();
    };
    addAndMakeVisible (lengthBox);

    for (int i = 0; i < Song::palette().size(); ++i)
    {
        auto* b = colourButtons.add (new juce::TextButton());
        b->setColour (juce::TextButton::buttonColourId, Song::palette()[i]);
        b->onClick = [this, i]
        {
            if (auto* p = ctx.currentPattern())
            {
                p->colour = Song::palette()[i];
                ctx.state.dirty = true;
                if (ctx.onPatternChanged) ctx.onPatternChanged();
            }
        };
        addAndMakeVisible (b);
    }

    notesEditor.setMultiLine (true, true);
    notesEditor.setReturnKeyStartsNewLine (true);
    notesEditor.setFont (Theme::uiFont (12.5f));
    notesEditor.setColour (juce::TextEditor::textColourId, Theme::textDim);
    notesEditor.setTextToShowWhenEmpty ("Notes for this pattern...", Theme::textFaint);
    notesEditor.onTextChange = [this]
    {
        if (updating) return;
        if (auto* p = ctx.currentPattern()) { p->notes = notesEditor.getText(); ctx.state.dirty = true; }
    };
    addAndMakeVisible (notesEditor);

    refresh();
}

RightPanel::~RightPanel() = default;

void RightPanel::refresh()
{
    updating = true;
    if (auto* p = ctx.currentPattern())
    {
        if (! nameEditor.hasKeyboardFocus (true)) nameEditor.setText (p->name, false);
        if (! notesEditor.hasKeyboardFocus (true)) notesEditor.setText (p->notes, false);
        if (lengthBox.indexOfItemId (p->numRows) < 0)
            lengthBox.addItem (juce::String (p->numRows), p->numRows);
        lengthBox.setSelectedId (p->numRows, juce::dontSendNotification);
        for (int i = 0; i < colourButtons.size(); ++i)
            colourButtons[i]->setButtonText (p->colour == Song::palette()[i] ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93")) : juce::String());
    }
    updating = false;
    resized();
    repaint();
}

void RightPanel::resized()
{
    auto r = getLocalBounds();
    const int listH = juce::jlimit (3 * PatternList::rowH, 12 * PatternList::rowH, patternList->idealHeight());

    patternsCard = r.removeFromTop (46 + listH + 10);
    r.removeFromTop (12);
    propsCard = r.removeFromTop (300);
    r.removeFromTop (12);
    helpCard = r;

    {
        auto c = patternsCard.reduced (12, 0);
        auto header = c.removeFromTop (44);
        patternMenuButton.setBounds (header.removeFromRight (26).withSizeKeepingCentre (26, 26));
        addPatternButton.setBounds (header.removeFromRight (28).withSizeKeepingCentre (26, 26));
        patternList->setBounds (c.removeFromTop (listH).withTrimmedLeft (-4).withTrimmedRight (-4));
    }
    {
        auto c = propsCard.reduced (14, 0);
        c.removeFromTop (44);
        auto row = [&c] (int h) { auto rr = c.removeFromTop (h); c.removeFromTop (8); return rr; };
        nameEditor.setBounds (row (30).withTrimmedLeft (80));
        lengthBox.setBounds (row (30).withTrimmedLeft (80));
        auto cr = row (24).withTrimmedLeft (80);
        const int n = colourButtons.size();
        const int sz = juce::jmin (20, (cr.getWidth() - (n - 1) * 3) / juce::jmax (1, n));
        for (int i = 0; i < n; ++i)
            colourButtons[i]->setBounds (cr.getX() + i * (sz + 3), cr.getY() + 2, sz, sz);
        c.removeFromTop (22);
        notesEditor.setBounds (c.removeFromTop (juce::jmax (40, c.getHeight() - 12)));
    }
}

void RightPanel::paint (juce::Graphics& g)
{
    for (auto card : { patternsCard, propsCard, helpCard })
        if (card.getHeight() > 30)
            Theme::drawCard (g, card.toFloat().reduced (1.0f), 12.0f);

    g.setColour (Theme::text);
    g.setFont (Theme::uiFont (15.5f, 700));
    g.drawText ("Patterns", patternsCard.reduced (14, 0).removeFromTop (44), juce::Justification::centredLeft);
    g.drawText ("Pattern Properties", propsCard.reduced (14, 0).removeFromTop (44), juce::Justification::centredLeft);

    g.setFont (Theme::uiFont (13.0f));
    g.setColour (Theme::textDim);
    auto lab = [&] (juce::Component& c, const char* text)
    {
        g.drawText (text, propsCard.getX() + 14, c.getY(), 80, c.getHeight(), juce::Justification::centredLeft);
    };
    lab (nameEditor, "Name");
    lab (lengthBox, "Length");
    if (! colourButtons.isEmpty()) lab (*colourButtons[0], "Colour");
    g.setColour (Theme::text);
    g.setFont (Theme::uiFont (13.5f, 700));
    g.drawText ("Notes", propsCard.getX() + 14, notesEditor.getY() - 22, 100, 20, juce::Justification::centredLeft);

    if (helpCard.getHeight() > 60)
    {
        auto c = helpCard.reduced (14, 10);
        g.setColour (Theme::text);
        g.setFont (Theme::uiFont (15.5f, 700));
        g.drawText ("Keys", c.removeFromTop (26), juce::Justification::centredLeft);
        g.setFont (Theme::uiFont (12.0f));
        const char* lines[] = {
            "Esc  -  edit mode on / off",
            "Space  -  play / stop song",
            "Shift+Space  -  loop",
            "Drag row nos.  -  loop rows",
            "Click field  -  edit it",
            "MIDI keys  -  enter notes",
            "0-9 A-F  -  hex values",
            "Tab  -  next track",
            "Del / Ins / Bksp  -  edit rows",
            "Ctrl+C / X / V  -  clipboard",
            "Ctrl+Up / Down  -  transpose",
            "F1 / F2  -  default octave",
            "FX: T tempo, D delay, C cut,",
            "R retrig, P bend, M mod, B jump",
        };
        for (auto* l : lines)
        {
            if (c.getHeight() < 16) break;
            const juce::String s (l);
            const int dash = s.indexOf ("  -  ");
            auto rr = c.removeFromTop (17);
            if (dash > 0)
            {
                g.setColour (Theme::text);
                g.setFont (Theme::uiFont (12.0f, 600));
                g.drawText (s.substring (0, dash), rr, juce::Justification::centredLeft);
                g.setColour (Theme::textDim);
                g.setFont (Theme::uiFont (12.0f));
                g.drawText (s.substring (dash + 5), rr.withTrimmedLeft (110), juce::Justification::centredLeft);
            }
            else
            {
                g.setColour (Theme::textDim);
                g.drawText (s, rr, juce::Justification::centredLeft);
            }
        }
    }
}

} // namespace artilody
