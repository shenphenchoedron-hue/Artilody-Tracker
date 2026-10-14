#pragma once

#include "AppContext.h"
#include "MixerStrip.h"

namespace artilody
{

/** Small labelled numeric box: drag / wheel / double-click to edit, chevron for presets. */
class NumberBox final : public juce::Component
{
public:
    NumberBox (const juce::String& label, double minV, double maxV, double step);

    std::function<double()> getValue;
    std::function<void (double)> setValue;
    juce::Array<double> presets;
    int decimals = 0;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void apply (double v);
    juce::String label;
    double minValue, maxValue, step;
    double dragStartValue = 0;
    bool dragging = false;
};

class TopBar final : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    explicit TopBar (AppContext&);
    ~TopBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onPlaySong, onToggleLoop, onStop, onRewind;
    std::function<void()> onUndo, onRedo;

    void updateState();

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    AppContext& ctx;
    IconButton rewindButton { "Rewind", Theme::Icon::Rewind }, playButton { "Play", Theme::Icon::Play },
               loopButton { "Loop pattern", Theme::Icon::Loop }, stopButton { "Stop", Theme::Icon::Stop };

    /** Big round record (edit mode) button that glows and pulses while recording. */
    class RecordButton final : public juce::Button, private juce::Timer
    {
    public:
        RecordButton() : juce::Button ("Edit mode") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        void setRecording (bool);
    private:
        void timerCallback() override { phase += 0.06f; if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi; repaint(); }
        float phase = 0.0f;
    } recordButton;
    NumberBox bpmBox { "BPM", 20, 999, 1 }, lpbBox { "LPB", 1, 32, 1 }, octBox { "Oct", 0, 9, 1 }, stepBox { "Step", 0, 16, 1 };
    IconButton followButton { "Follow", Theme::Icon::Follow };
    IconButton undoButton { "Undo", Theme::Icon::Undo }, redoButton { "Redo", Theme::Icon::Redo };
    LevelMeter masterMeter;
    juce::Rectangle<int> logoArea, projectArea, meterArea;
    float cpu = 0.0f;
};

} // namespace artilody
