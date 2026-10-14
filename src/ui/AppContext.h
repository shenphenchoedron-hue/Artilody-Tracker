#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "../AudioEngine.h"
#include "../Theme.h"

namespace artilody
{

/** Layout constants shared by the track area, row numbers and mixer. */
namespace Layout
{
    constexpr int trackHeaderHeight = 34;
    constexpr int columnLabelHeight = 22;
    constexpr int rowHeight = 20;
    constexpr int noteColumnWidth = 150;
    constexpr int stripWidth = 150;
    constexpr int mixerExpandedHeight = 352;
    constexpr int mixerCollapsedHeight = 70;
    constexpr int trackGap = 2;
    constexpr int addTrackWidth = 64;
    constexpr int seqWidth = 82;
    constexpr int rowNumberWidth = 40;
    constexpr int leftColumnWidth = seqWidth + rowNumberWidth + 8;

    inline int trackWidth (int noteColumns) { return juce::jmax (stripWidth, noteColumns * noteColumnWidth); }
}

/** UI editing state (message thread only). */
struct EditorState : public juce::ChangeBroadcaster
{
    int seqSlot = 0;
    int pattern = 0;

    int cursorTrack = 0;
    int cursorColumn = 0;
    int cursorField = 0;   // 0 note, 1-2 ins, 3-4 vol, 5 fx cmd, 6-7 fx value
    int cursorRow = 0;

    int octave = 4;
    int editStep = 1;
    bool editMode = false;
    bool followPlayback = true;
    bool mixerCollapsed = false;

    bool hasSelection = false;
    int selStartTrack = 0, selStartCol = 0, selStartRow = 0;
    int selEndTrack = 0, selEndCol = 0, selEndRow = 0;

    juce::File currentFile;
    bool dirty = false;

    void changed() { sendChangeMessage(); }
};

/** Undo action that swaps a whole pattern snapshot. */
class PatternSnapshotAction final : public juce::UndoableAction
{
public:
    PatternSnapshotAction (AudioEngine& e, int index, Pattern before, Pattern after)
        : engine (e), patternIndex (index), beforeState (std::move (before)), afterState (std::move (after)) {}

    bool perform() override { return apply (afterState); }
    bool undo() override    { return apply (beforeState); }
    int getSizeInUnits() override { return (int) (afterState.tracks.size() * (size_t) afterState.numRows * kMaxNoteColumns); }

private:
    bool apply (const Pattern& p)
    {
        const juce::ScopedLock sl (engine.song.lock);
        if (! juce::isPositiveAndBelow (patternIndex, (int) engine.song.patterns.size()))
            return false;
        if (p.tracks.size() != engine.song.patterns[(size_t) patternIndex].tracks.size())
            return false;
        engine.song.patterns[(size_t) patternIndex] = p;
        return true;
    }

    AudioEngine& engine;
    int patternIndex;
    Pattern beforeState, afterState;
};

struct AppContext
{
    explicit AppContext (AudioEngine& e) : engine (e) {}

    AudioEngine& engine;
    EditorState state;
    juce::UndoManager undo { 30000000, 200 };

    Pattern* currentPattern()
    {
        auto& s = engine.song;
        if (s.patterns.empty()) return nullptr;
        state.pattern = juce::jlimit (0, (int) s.patterns.size() - 1, state.pattern);
        return &s.patterns[(size_t) state.pattern];
    }

    /** Snapshot-based undo helpers. */
    void beginPatternEdit()
    {
        if (auto* p = currentPattern())
        {
            const juce::ScopedLock sl (engine.song.lock);
            editBefore = *p;
            editPatternIndex = state.pattern;
        }
    }

    void endPatternEdit (const juce::String& name)
    {
        auto* p = currentPattern();
        if (p == nullptr || editPatternIndex != state.pattern) return;
        Pattern after;
        {
            const juce::ScopedLock sl (engine.song.lock);
            after = *p;
        }
        undo.beginNewTransaction (name);
        undo.perform (new PatternSnapshotAction (engine, editPatternIndex, std::move (editBefore), std::move (after)));
        state.dirty = true;
    }

    std::function<void()> onStructureChanged;   // tracks/patterns changed -> relayout
    std::function<void()> onPatternChanged;     // repaint grid & seq
    std::function<void (StripProcessor&, int track)> onInstrumentChanged;
    std::function<void()> onShowAudioSettings;
    std::function<void (int track)> onSelectTrack;

private:
    Pattern editBefore;
    int editPatternIndex = -1;
};

} // namespace artilody
