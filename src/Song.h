#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

namespace artilody
{

constexpr int kMaxNoteColumns = 8;
constexpr int kMaxRows = 512;
constexpr int kMaxTracks = 64;

enum : int8_t
{
    kNoteEmpty = -1,
    kNoteOff   = -2
};

/** One cell in a note column: note, instrument, volume and effect. */
struct Cell
{
    int8_t  note  = kNoteEmpty;  // 0..119, kNoteOff or kNoteEmpty
    uint8_t ins   = 0xFF;        // 0..0xFE, 0xFF = empty
    uint8_t vol   = 0xFF;        // 0..0x7F, 0xFF = empty
    char    fxCmd = 0;           // 0 = empty, otherwise 'A'..'Z'
    uint8_t fxVal = 0;

    bool isEmpty() const noexcept { return note == kNoteEmpty && ins == 0xFF && vol == 0xFF && fxCmd == 0; }
    void clear() noexcept         { *this = Cell(); }
    bool operator== (const Cell& o) const noexcept
    {
        return note == o.note && ins == o.ins && vol == o.vol && fxCmd == o.fxCmd && fxVal == o.fxVal;
    }
};

juce::String noteToString (int note);

struct TrackInfo
{
    juce::String name;
    juce::Colour colour;
    int noteColumns = 1;
    uint32_t uid = 0;
};

struct Pattern
{
    juce::String name;
    juce::Colour colour { 0xff2f80ed };
    juce::String notes;
    int numRows = 64;

    /** tracks[t][row * kMaxNoteColumns + column] */
    std::vector<std::vector<Cell>> tracks;

    Cell& cell (int track, int row, int column)             { return tracks[(size_t) track][(size_t) (row * kMaxNoteColumns + column)]; }
    const Cell& cell (int track, int row, int column) const { return tracks[(size_t) track][(size_t) (row * kMaxNoteColumns + column)]; }

    void setNumTracks (int n);
    void insertTrack (int index);
    void removeTrack (int index);
    void moveTrack (int from, int to);
    void setNumRows (int rows);
};

/** Effect commands understood by the sequencer. */
struct EffectInfo
{
    char cmd;
    const char* description;
};

const std::vector<EffectInfo>& getEffectList();

class Song
{
public:
    Song();

    juce::String title { "Untitled" };
    double bpm = 128.0;
    int lpb = 4;

    std::vector<TrackInfo> tracks;
    std::vector<Pattern> patterns;
    std::vector<int> sequence;

    void clear() { title = "Untitled"; bpm = 128.0; lpb = 4; tracks.clear(); patterns.clear(); sequence.clear(); }

    int numTracks() const noexcept { return (int) tracks.size(); }

    /** Data used by audio + ui. Hold this while reading/writing pattern data
        from a thread that isn't the message thread, or while changing structure. */
    juce::CriticalSection lock;

    int addPattern (int numRows, const juce::String& name);
    int duplicatePattern (int index);
    void removePattern (int index);

    juce::Colour nextTrackColour() const;
    uint32_t nextTrackUid() const;

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);

    static const juce::Array<juce::Colour>& palette();
};

} // namespace artilody
