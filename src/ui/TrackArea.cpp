#include "TrackArea.h"

namespace artilody
{

TrackArea::TrackArea (AppContext& c) : grid (c), ctx (c)
{
    addAndMakeVisible (grid);

    addTrackButton.fill = Theme::panel;
    addTrackButton.iconColour = Theme::textDim;
    addTrackButton.setTooltip ("Add a new track (and its channel strip)");
    addTrackButton.onClick = [this] { if (onAddTrack) onAddTrack(); };
    addAndMakeVisible (addTrackButton);
}

TrackArea::~TrackArea() = default;

MixerStrip* TrackArea::getStripComponent (int t) const
{
    return juce::isPositiveAndBelow (t, strips.size()) ? strips[t] : nullptr;
}

void TrackArea::rebuild()
{
    auto& e = ctx.engine;
    grid.updateLayout();

    // reuse strip components where possible
    juce::OwnedArray<MixerStrip> old;
    old.swapWith (strips);

    for (int t = 0; t < e.getNumStrips(); ++t)
    {
        auto* sp = &e.getStrip (t);
        MixerStrip* found = nullptr;
        for (int i = 0; i < old.size(); ++i)
            if (&old[i]->getStrip() == sp)
            {
                found = old.removeAndReturn (i);
                break;
            }
        if (found == nullptr)
            found = new MixerStrip (ctx, *sp, t);
        found->setTrackIndex (t);
        found->setCompact (mixerCollapsed);
        found->refresh();
        strips.add (found);
        addAndMakeVisible (found);
    }
    old.clear();

    headerMenus.clear();
    for (int t = 0; t < e.getNumStrips(); ++t)
    {
        auto* b = headerMenus.add (new IconButton ("track menu", Theme::Icon::Dots));
        b->drawBackground = false;
        b->onClick = [this, t] { if (auto* s = getStripComponent (t)) s->showTrackMenu(); };
        addAndMakeVisible (b);
    }

    setSize (getIdealWidth(), getHeight());
    resized();
    repaint();
}

void TrackArea::setMixerCollapsed (bool c)
{
    mixerCollapsed = c;
    for (auto* s : strips)
        s->setCompact (c);
    resized();
    repaint();
}

int TrackArea::getMixerHeight() const
{
    return mixerCollapsed ? Layout::mixerCollapsedHeight : Layout::mixerExpandedHeight;
}

int TrackArea::getGridBottom() const
{
    return juce::jmax (getGridTop() + Layout::rowHeight * 4, getHeight() - getMixerHeight() - 10);
}

int TrackArea::getIdealWidth() const
{
    return grid.getTotalWidth() + Layout::addTrackWidth + 8;
}

void TrackArea::resized()
{
    const int gridTop = getGridTop();
    const int gridBottom = getGridBottom();
    grid.setBounds (0, gridTop, juce::jmax (1, grid.getTotalWidth()), gridBottom - gridTop);

    const int mixerY = gridBottom + 10;
    for (int t = 0; t < strips.size(); ++t)
    {
        const int x = grid.getTrackX (t);
        const int w = grid.getTrackWidth (t);
        strips[t]->setBounds (x, mixerY, w, getHeight() - mixerY);
        if (auto* b = headerMenus[t])
            b->setBounds (x + w - 28, 5, 24, Layout::trackHeaderHeight - 10);
    }

    addTrackButton.setBounds (grid.getTotalWidth() + 6, 0, Layout::addTrackWidth - 4, Layout::trackHeaderHeight);
}

int TrackArea::headerHitTrack (juce::Point<int> p) const
{
    if (p.y >= Layout::trackHeaderHeight) return -1;
    for (int t = 0; t < ctx.engine.song.numTracks(); ++t)
        if (p.x >= grid.getTrackX (t) && p.x < grid.getTrackX (t) + grid.getTrackWidth (t))
            return t;
    return -1;
}

void TrackArea::mouseDown (const juce::MouseEvent& e)
{
    const int t = headerHitTrack (e.getPosition());
    if (t >= 0)
    {
        if (e.mods.isPopupMenu())
        {
            if (auto* s = getStripComponent (t)) s->showTrackMenu();
        }
        else if (ctx.onSelectTrack)
        {
            ctx.onSelectTrack (t);
        }
    }
}

void TrackArea::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int t = headerHitTrack (e.getPosition());
    if (t >= 0)
        if (auto* s = getStripComponent (t))
            s->mouseDoubleClick (e.getEventRelativeTo (s).withNewPosition (juce::Point<int> (10, 10)));
}

void TrackArea::paint (juce::Graphics& g)
{
    auto& song = ctx.engine.song;
    const int gridTop = getGridTop();
    const int gridBottom = getGridBottom();

    // dark frame behind grid + labels
    {
        auto frame = juce::Rectangle<float> (0, (float) Layout::trackHeaderHeight - 8.0f, (float) juce::jmax (0, grid.getTotalWidth()), (float) (gridBottom - Layout::trackHeaderHeight + 8));
        g.setColour (Theme::gridHeader);
        g.fillRoundedRectangle (frame, 8.0f);
    }

    const float cw = Theme::monoFont (14.0f).getStringWidthFloat ("0");

    for (int t = 0; t < song.numTracks(); ++t)
    {
        const auto& tr = song.tracks[(size_t) t];
        const int x = grid.getTrackX (t);
        const int w = grid.getTrackWidth (t);
        const bool selected = t == ctx.state.cursorTrack;

        // header tab
        auto hr = juce::Rectangle<float> ((float) x, 0.0f, (float) w, (float) Layout::trackHeaderHeight);
        juce::Path tab;
        tab.addRoundedRectangle (hr.getX(), hr.getY(), hr.getWidth(), hr.getHeight(), 9.0f, 9.0f, true, true, false, false);
        g.setColour (selected ? tr.colour : tr.colour.withAlpha (0.2f).overlaidWith (juce::Colours::transparentWhite));
        if (! selected)
        {
            g.setColour (Theme::panel);
            g.fillPath (tab);
            g.setColour (tr.colour.withAlpha (0.22f));
        }
        g.fillPath (tab);

        auto inner = hr.reduced (8.0f, 7.0f);
        auto badge = inner.removeFromLeft (26.0f);
        g.setColour (selected ? juce::Colours::white.withAlpha (0.25f) : tr.colour);
        g.fillRoundedRectangle (badge, 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (Theme::uiFont (12.0f, 700));
        g.drawText (juce::String (t + 1).paddedLeft ('0', 2), badge, juce::Justification::centred);
        inner.removeFromLeft (8.0f);
        inner.removeFromRight (26.0f);
        g.setColour (selected ? juce::Colours::white : Theme::text);
        g.setFont (Theme::uiFont (14.5f, 700));
        g.drawFittedText (tr.name, inner.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);

        // column labels
        auto lr = juce::Rectangle<int> (x, Layout::trackHeaderHeight, w, Layout::columnLabelHeight);
        g.setColour (Theme::gridHeader);
        g.fillRect (lr);
        g.setColour (tr.colour.withAlpha (0.7f));
        g.fillRect (lr.getX(), lr.getY(), lr.getWidth(), 2);
        g.setFont (Theme::uiFont (11.5f, 600));
        g.setColour (juce::Colour (0xff9aa7bd));
        for (int c = 0; c < tr.noteColumns; ++c)
        {
            const float cx = (float) (x + c * Layout::noteColumnWidth + 9);
            auto lab = [&] (float charOff, float chars, const char* text)
            {
                g.drawText (text, juce::Rectangle<float> (cx + charOff * cw - 4.0f, (float) lr.getY() + 2.0f, chars * cw + 8.0f, (float) lr.getHeight() - 2.0f),
                            juce::Justification::centredLeft);
            };
            lab (0, 3, "Note");
            lab (5, 2, "Ins");
            lab (9, 2, "Vol");
            lab (13, 3, "FX");
        }
    }

    juce::ignoreUnused (gridTop);
}

} // namespace artilody
