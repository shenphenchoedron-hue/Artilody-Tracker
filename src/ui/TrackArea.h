#pragma once

#include "PatternGrid.h"
#include "MixerStrip.h"

namespace artilody
{

/** Horizontal strip of tracks: header, pattern grid and the matching channel strip directly underneath. */
class TrackArea final : public juce::Component
{
public:
    explicit TrackArea (AppContext&);
    ~TrackArea() override;

    void rebuild();
    void setMixerCollapsed (bool);
    int getMixerHeight() const;
    int getGridTop() const { return Layout::trackHeaderHeight + Layout::columnLabelHeight; }
    int getGridBottom() const;
    int getIdealWidth() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    PatternGrid grid;
    MixerStrip* getStripComponent (int track) const;

    std::function<void()> onAddTrack;

private:
    int headerHitTrack (juce::Point<int>) const;

    AppContext& ctx;
    juce::OwnedArray<MixerStrip> strips;
    juce::OwnedArray<IconButton> headerMenus;
    IconButton addTrackButton { "Add track", Theme::Icon::Plus };
    bool mixerCollapsed = false;
};

} // namespace artilody
