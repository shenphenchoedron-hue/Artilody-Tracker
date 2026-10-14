#include "Song.h"

namespace artilody
{

juce::String noteToString (int note)
{
    if (note == kNoteOff)   return "OFF";
    if (note < 0)           return "---";
    static const char* names[] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
    return juce::String (names[note % 12]) + juce::String (note / 12);
}

//==============================================================================
void Pattern::setNumTracks (int n)
{
    tracks.resize ((size_t) n);
    for (auto& t : tracks)
        t.resize ((size_t) (numRows * kMaxNoteColumns));
}

void Pattern::insertTrack (int index)
{
    index = juce::jlimit (0, (int) tracks.size(), index);
    tracks.insert (tracks.begin() + index, std::vector<Cell> ((size_t) (numRows * kMaxNoteColumns)));
}

void Pattern::removeTrack (int index)
{
    if (juce::isPositiveAndBelow (index, (int) tracks.size()))
        tracks.erase (tracks.begin() + index);
}

void Pattern::moveTrack (int from, int to)
{
    if (! juce::isPositiveAndBelow (from, (int) tracks.size()) || ! juce::isPositiveAndBelow (to, (int) tracks.size()))
        return;
    auto t = std::move (tracks[(size_t) from]);
    tracks.erase (tracks.begin() + from);
    tracks.insert (tracks.begin() + to, std::move (t));
}

void Pattern::setNumRows (int rows)
{
    rows = juce::jlimit (1, kMaxRows, rows);
    numRows = rows;
    for (auto& t : tracks)
        t.resize ((size_t) (rows * kMaxNoteColumns));
}

//==============================================================================
const std::vector<EffectInfo>& getEffectList()
{
    static const std::vector<EffectInfo> list {
        { 'T', "Txx  Set tempo to xx BPM (hex, 20-FF)" },
        { 'L', "Lxx  Set lines per beat (01-10)" },
        { 'D', "Dxx  Delay note by xx/256 of a line" },
        { 'C', "Cxx  Cut note after xx/256 of a line" },
        { 'R', "Rxy  Retrigger note every y ticks (x = volume fall)" },
        { 'P', "Pxx  Pitch bend (00 = down, 80 = centre, FF = up)" },
        { 'M', "Mxx  Modulation wheel (CC1) 00-7F" },
        { 'B', "Bxx  Jump to sequence position xx after this line" },
    };
    return list;
}

//==============================================================================
const juce::Array<juce::Colour>& Song::palette()
{
    static const juce::Array<juce::Colour> colours {
        juce::Colour (0xffff5a4e), // red
        juce::Colour (0xff1fbfb0), // teal
        juce::Colour (0xffffa62b), // orange
        juce::Colour (0xff2f7bf0), // blue
        juce::Colour (0xff27c2ef), // cyan
        juce::Colour (0xffff7a59), // coral
        juce::Colour (0xff29c79c), // green
        juce::Colour (0xff9b6bf2), // purple
        juce::Colour (0xfff06abf), // pink
        juce::Colour (0xff8fa3bf), // slate
    };
    return colours;
}

Song::Song() = default;

juce::Colour Song::nextTrackColour() const
{
    return palette()[(int) tracks.size() % palette().size()];
}

uint32_t Song::nextTrackUid() const
{
    uint32_t m = 0;
    for (auto& t : tracks)
        m = juce::jmax (m, t.uid);
    return m + 1;
}

int Song::addPattern (int numRows, const juce::String& name)
{
    Pattern p;
    p.name = name;
    p.numRows = juce::jlimit (1, kMaxRows, numRows);
    p.colour = palette()[(int) patterns.size() % palette().size()];
    p.setNumTracks ((int) tracks.size());
    patterns.push_back (std::move (p));
    return (int) patterns.size() - 1;
}

int Song::duplicatePattern (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) patterns.size()))
        return -1;
    Pattern p = patterns[(size_t) index];
    p.name = p.name + " copy";
    patterns.push_back (std::move (p));
    return (int) patterns.size() - 1;
}

void Song::removePattern (int index)
{
    if (patterns.size() <= 1 || ! juce::isPositiveAndBelow (index, (int) patterns.size()))
        return;

    patterns.erase (patterns.begin() + index);

    std::vector<int> newSeq;
    for (auto s : sequence)
    {
        if (s == index) continue;
        newSeq.push_back (s > index ? s - 1 : s);
    }
    if (newSeq.empty())
        newSeq.push_back (0);
    sequence = newSeq;
}

//==============================================================================
juce::ValueTree Song::toValueTree() const
{
    juce::ValueTree v ("SONG");
    v.setProperty ("title", title, nullptr);
    v.setProperty ("bpm", bpm, nullptr);
    v.setProperty ("lpb", lpb, nullptr);

    juce::ValueTree tr ("TRACKS");
    for (auto& t : tracks)
    {
        juce::ValueTree tv ("TRACK");
        tv.setProperty ("name", t.name, nullptr);
        tv.setProperty ("colour", t.colour.toString(), nullptr);
        tv.setProperty ("columns", t.noteColumns, nullptr);
        tv.setProperty ("uid", (int) t.uid, nullptr);
        tr.appendChild (tv, nullptr);
    }
    v.appendChild (tr, nullptr);

    juce::ValueTree pats ("PATTERNS");
    for (auto& p : patterns)
    {
        juce::ValueTree pv ("PATTERN");
        pv.setProperty ("name", p.name, nullptr);
        pv.setProperty ("colour", p.colour.toString(), nullptr);
        pv.setProperty ("rows", p.numRows, nullptr);
        pv.setProperty ("notes", p.notes, nullptr);

        for (size_t t = 0; t < p.tracks.size(); ++t)
        {
            juce::StringArray tokens;
            for (int r = 0; r < p.numRows; ++r)
                for (int c = 0; c < kMaxNoteColumns; ++c)
                {
                    auto& cell = p.cell ((int) t, r, c);
                    if (cell.isEmpty()) continue;
                    tokens.add (juce::String (r) + "," + juce::String (c) + "," + juce::String ((int) cell.note) + ","
                                + juce::String ((int) cell.ins) + "," + juce::String ((int) cell.vol) + ","
                                + juce::String ((int) cell.fxCmd) + "," + juce::String ((int) cell.fxVal));
                }
            juce::ValueTree tv ("T");
            tv.setProperty ("d", tokens.joinIntoString (" "), nullptr);
            pv.appendChild (tv, nullptr);
        }
        pats.appendChild (pv, nullptr);
    }
    v.appendChild (pats, nullptr);

    juce::StringArray seq;
    for (auto s : sequence) seq.add (juce::String (s));
    v.setProperty ("sequence", seq.joinIntoString (","), nullptr);
    return v;
}

void Song::fromValueTree (const juce::ValueTree& v)
{
    title = v.getProperty ("title", "Untitled").toString();
    bpm = (double) v.getProperty ("bpm", 128.0);
    lpb = (int) v.getProperty ("lpb", 4);

    tracks.clear();
    for (auto tv : v.getChildWithName ("TRACKS"))
    {
        TrackInfo t;
        t.name = tv.getProperty ("name").toString();
        t.colour = juce::Colour::fromString (tv.getProperty ("colour", "ff2f7bf0").toString());
        t.noteColumns = juce::jlimit (1, kMaxNoteColumns, (int) tv.getProperty ("columns", 1));
        t.uid = (uint32_t) (int) tv.getProperty ("uid", (int) tracks.size() + 1);
        tracks.push_back (t);
    }

    patterns.clear();
    for (auto pv : v.getChildWithName ("PATTERNS"))
    {
        Pattern p;
        p.name = pv.getProperty ("name").toString();
        p.colour = juce::Colour::fromString (pv.getProperty ("colour", "ff2f7bf0").toString());
        p.numRows = juce::jlimit (1, kMaxRows, (int) pv.getProperty ("rows", 64));
        p.notes = pv.getProperty ("notes").toString();
        p.setNumTracks ((int) tracks.size());

        int t = 0;
        for (auto tv : pv)
        {
            if (t >= (int) tracks.size()) break;
            auto tokens = juce::StringArray::fromTokens (tv.getProperty ("d").toString(), " ", "");
            for (auto& tok : tokens)
            {
                auto f = juce::StringArray::fromTokens (tok, ",", "");
                if (f.size() < 7) continue;
                int r = f[0].getIntValue(), c = f[1].getIntValue();
                if (! juce::isPositiveAndBelow (r, p.numRows) || ! juce::isPositiveAndBelow (c, kMaxNoteColumns)) continue;
                auto& cell = p.cell (t, r, c);
                cell.note = (int8_t) f[2].getIntValue();
                cell.ins = (uint8_t) f[3].getIntValue();
                cell.vol = (uint8_t) f[4].getIntValue();
                cell.fxCmd = (char) f[5].getIntValue();
                cell.fxVal = (uint8_t) f[6].getIntValue();
            }
            ++t;
        }
        patterns.push_back (std::move (p));
    }

    if (patterns.empty())
        addPattern (64, "Init");

    sequence.clear();
    for (auto& s : juce::StringArray::fromTokens (v.getProperty ("sequence").toString(), ",", ""))
        sequence.push_back (juce::jlimit (0, (int) patterns.size() - 1, s.getIntValue()));
    if (sequence.empty())
        sequence.push_back (0);
}

} // namespace artilody
