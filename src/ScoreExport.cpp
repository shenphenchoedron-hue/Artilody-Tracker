#include "ScoreExport.h"
#include <map>

namespace artilody
{
namespace ScoreExport
{

static constexpr int kTicksPerLine = 12;        // retrigger resolution, same as the sequencer
static constexpr int kDefaultVelocity = 102;    // empty Vol column = 0.8 * 127

//==============================================================================
Performance perform (const Song& song, bool includeRetriggers)
{
    Performance perf;
    if (song.sequence.empty() || song.patterns.empty() || song.tracks.empty())
        return perf;

    const int numTracks = song.numTracks();
    std::map<std::pair<int, int>, int> active;   // (track, column) -> index into perf.notes

    auto stopColumn = [&] (int t, int col, double tick)
    {
        auto it = active.find ({ t, col });
        if (it == active.end()) return;
        auto& n = perf.notes[(size_t) it->second];
        n.end = juce::jmax (n.start, tick);
        active.erase (it);
    };

    auto startColumn = [&] (int t, int col, int part, int pitch, int velocity, double tick, bool retrig)
    {
        stopColumn (t, col, tick);
        perf.notes.push_back ({ part, tick, tick, pitch, velocity, retrig });
        active[{ t, col }] = (int) perf.notes.size() - 1;
    };

    double tick = 0.0;
    double bpm = song.bpm;
    int lpb = juce::jmax (1, song.lpb);
    perf.tempos.push_back ({ 0.0, bpm });

    int seq = 0, row = 0, guard = 0;
    struct RowEvent { double tick; int type; int t, col, pitch, part, velocity; bool retrig; };   // type 0 = on, 1 = off
    std::vector<RowEvent> events;

    while (guard++ < 200000)
    {
        const int patIndex = juce::jlimit (0, (int) song.patterns.size() - 1, song.sequence[(size_t) seq]);
        const auto& pat = song.patterns[(size_t) patIndex];
        if (row >= pat.numRows) row = 0;

        // global effects first, like the sequencer
        int jump = -1;
        for (int t = 0; t < numTracks && t < (int) pat.tracks.size(); ++t)
            for (int col = 0; col < song.tracks[(size_t) t].noteColumns; ++col)
            {
                const auto& c = pat.cell (t, row, col);
                if (c.fxCmd == 'T' && c.fxVal >= 0x20)
                {
                    bpm = (double) c.fxVal;
                    perf.tempos.push_back ({ tick, bpm });
                }
                else if (c.fxCmd == 'L' && c.fxVal >= 1) lpb = juce::jlimit (1, 32, (int) c.fxVal);
                else if (c.fxCmd == 'B') jump = c.fxVal;
            }

        const double rowTicks = (double) ticksPerQuarter / (double) lpb;
        events.clear();

        for (int t = 0; t < numTracks && t < (int) pat.tracks.size(); ++t)
            for (int col = 0; col < song.tracks[(size_t) t].noteColumns; ++col)
            {
                const auto& c = pat.cell (t, row, col);
                if (c.isEmpty()) continue;

                const int part = (c.ins != 0xFF && c.ins >= 1 && c.ins <= numTracks) ? c.ins - 1 : t;
                const int vel = c.vol != 0xFF ? juce::jlimit (1, 127, (int) c.vol) : kDefaultVelocity;
                const double when = tick + (c.fxCmd == 'D' ? rowTicks * c.fxVal / 256.0 : 0.0);

                if (c.note >= 0)
                {
                    events.push_back ({ when, 0, t, col, c.note, part, vel, false });
                    if (includeRetriggers && c.fxCmd == 'R' && (c.fxVal & 0x0F) > 0)
                    {
                        const int every = c.fxVal & 0x0F;
                        const double fall = (double) ((c.fxVal >> 4) & 0x0F) / 16.0;
                        double v = vel;
                        for (int k = every; k < kTicksPerLine; k += every)
                        {
                            v *= (1.0 - fall);
                            events.push_back ({ tick + rowTicks * k / kTicksPerLine, 0, t, col, c.note, part, juce::jlimit (1, 127, (int) std::round (v)), true });
                        }
                    }
                }
                else if (c.note == kNoteOff)
                {
                    events.push_back ({ when, 1, t, col, -1, part, 0, false });
                }

                if (c.fxCmd == 'C')
                    events.push_back ({ tick + rowTicks * c.fxVal / 256.0, 1, t, col, -1, part, 0, false });
                if (c.fxCmd == 'P')
                    perf.controls.push_back ({ part, tick, true, juce::jlimit (0, 16383, (int) c.fxVal * 64) });
                if (c.fxCmd == 'M')
                    perf.controls.push_back ({ part, tick, false, juce::jmin (127, (int) c.fxVal) });
            }

        std::stable_sort (events.begin(), events.end(), [] (const RowEvent& a, const RowEvent& b) { return a.tick < b.tick; });
        for (auto& e : events)
        {
            if (e.type == 0) startColumn (e.t, e.col, e.part, e.pitch, e.velocity, e.tick, e.retrig);
            else             stopColumn (e.t, e.col, e.tick);
        }

        tick += rowTicks;

        // advance like the sequencer; a backwards jump or the end of the sequence ends the export
        if (jump >= 0)
        {
            if (jump <= seq || jump >= (int) song.sequence.size()) break;
            seq = jump;
            row = 0;
            continue;
        }
        if (++row >= pat.numRows)
        {
            row = 0;
            if (++seq >= (int) song.sequence.size()) break;
        }
    }

    for (auto& [key, index] : active)
        perf.notes[(size_t) index].end = tick;
    perf.length = tick;

    // drop zero-length notes (e.g. a note immediately replaced in the same column)
    perf.notes.erase (std::remove_if (perf.notes.begin(), perf.notes.end(), [] (const NoteEvent& n) { return n.end <= n.start; }), perf.notes.end());
    return perf;
}

//==============================================================================
static std::vector<int> partsInUse (const Performance& perf, int numTracks)
{
    std::vector<bool> used ((size_t) numTracks, false);
    for (auto& n : perf.notes)
        if (juce::isPositiveAndBelow (n.part, numTracks)) used[(size_t) n.part] = true;
    std::vector<int> parts;
    for (int t = 0; t < numTracks; ++t)
        if (used[(size_t) t]) parts.push_back (t);
    return parts;
}

static InstrumentInfo infoFor (const std::vector<InstrumentInfo>& infos, const Song& song, int t)
{
    if (juce::isPositiveAndBelow (t, (int) infos.size())) return infos[(size_t) t];
    InstrumentInfo i;
    i.name = song.tracks[(size_t) t].name;
    return i;
}

//==============================================================================
bool writeMidi (const Song& song, const std::vector<InstrumentInfo>& infos, const juce::File& file, juce::String& error)
{
    const auto perf = perform (song, true);
    const auto parts = partsInUse (perf, song.numTracks());
    if (parts.empty()) { error = "The song has no notes to export."; return false; }

    auto toTick = [] (double t) { return std::round (t); };

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (ticksPerQuarter);

    // conductor track: name, time signature, tempo
    juce::MidiMessageSequence conductor;
    conductor.addEvent (juce::MidiMessage::textMetaEvent (3, song.title.isNotEmpty() ? song.title : juce::String ("Artilody Tracker")), 0);
    conductor.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    for (auto& tp : perf.tempos)
        conductor.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / juce::jmax (1.0, tp.bpm))), toTick (tp.tick));
    conductor.addEvent (juce::MidiMessage::endOfTrack(), toTick (perf.length));
    midi.addTrack (conductor);

    int nextChannel = 1;
    for (int part : parts)
    {
        const auto info = infoFor (infos, song, part);
        int channel = 10;
        if (! info.drums)
        {
            channel = nextChannel;
            nextChannel = nextChannel % 16 + 1;
            if (nextChannel == 10) nextChannel = 11;
        }

        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, info.name), 0);
        if (! info.drums)
            seq.addEvent (juce::MidiMessage::programChange (channel, juce::jlimit (0, 127, info.program)), 0);

        for (auto& c : perf.controls)
            if (c.part == part)
                seq.addEvent (c.pitchBend ? juce::MidiMessage::pitchWheel (channel, c.value)
                                          : juce::MidiMessage::controllerEvent (channel, 1, c.value), toTick (c.tick));

        for (auto& n : perf.notes)
        {
            if (n.part != part) continue;
            const double s = toTick (n.start), e = juce::jmax (s + 1.0, toTick (n.end));
            seq.addEvent (juce::MidiMessage::noteOn (channel, n.pitch, (juce::uint8) n.velocity), s);
            seq.addEvent (juce::MidiMessage::noteOff (channel, n.pitch), e);
        }

        seq.addEvent (juce::MidiMessage::endOfTrack(), toTick (perf.length));
        seq.sort();
        seq.updateMatchedPairs();
        midi.addTrack (seq);
    }

    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk()) { error = "Could not write " + file.getFullPathName(); return false; }
    if (! midi.writeTo (out, 1)) { error = "Writing the MIDI file failed."; return false; }
    return true;
}

//==============================================================================
// MusicXML
//==============================================================================
namespace
{
    constexpr int kQuantum = ticksPerQuarter / 8;        // 32nd note grid
    constexpr int kMeasure = ticksPerQuarter * 4;        // 4/4

    struct NoteValue { int duration; const char* type; bool dotted; };
    const NoteValue kValues[] = {
        { 1920, "whole", false }, { 1440, "half", true }, { 960, "half", false }, { 720, "quarter", true },
        { 480, "quarter", false }, { 360, "eighth", true }, { 240, "eighth", false }, { 180, "16th", true },
        { 120, "16th", false }, { 90, "32nd", true }, { 60, "32nd", false },
    };

    /** Splits a duration (multiple of kQuantum) into note values that can be written (tied together). */
    std::vector<NoteValue> decompose (int duration)
    {
        std::vector<NoteValue> out;
        while (duration >= kQuantum)
        {
            for (auto& v : kValues)
                if (v.duration <= duration) { out.push_back (v); duration -= v.duration; break; }
        }
        return out;
    }

    struct Group { int start, end, velocity; std::vector<int> pitches; };   // a chord (or single note)

    void addPitch (juce::XmlElement& note, int midi, bool drums)
    {
        static const char steps[]  = { 'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B' };
        static const int  alters[] = {  0,   1,   0,   1,   0,   0,   1,   0,   1,   0,   1,   0  };
        const int pc = ((midi % 12) + 12) % 12, octave = midi / 12 - 1;
        if (drums)
        {
            auto* u = note.createNewChildElement ("unpitched");
            u->createNewChildElement ("display-step")->addTextElement (juce::String::charToString (steps[pc]));
            u->createNewChildElement ("display-octave")->addTextElement (juce::String (octave));
        }
        else
        {
            auto* p = note.createNewChildElement ("pitch");
            p->createNewChildElement ("step")->addTextElement (juce::String::charToString (steps[pc]));
            if (alters[pc] != 0) p->createNewChildElement ("alter")->addTextElement (juce::String (alters[pc]));
            p->createNewChildElement ("octave")->addTextElement (juce::String (octave));
        }
    }

    void addRest (juce::XmlElement& measure, int duration, int voice, bool wholeMeasure)
    {
        if (wholeMeasure)
        {
            auto* n = measure.createNewChildElement ("note");
            n->createNewChildElement ("rest")->setAttribute ("measure", "yes");
            n->createNewChildElement ("duration")->addTextElement (juce::String (duration));
            n->createNewChildElement ("voice")->addTextElement (juce::String (voice));
            return;
        }
        for (auto& v : decompose (duration))
        {
            auto* n = measure.createNewChildElement ("note");
            n->createNewChildElement ("rest");
            n->createNewChildElement ("duration")->addTextElement (juce::String (v.duration));
            n->createNewChildElement ("voice")->addTextElement (juce::String (voice));
            n->createNewChildElement ("type")->addTextElement (v.type);
            if (v.dotted) n->createNewChildElement ("dot");
        }
    }

    /** Writes [from, to) of a group into the measure, tied to what comes before / after. */
    void addGroupSegment (juce::XmlElement& measure, const Group& g, int from, int to, int voice, bool drums)
    {
        const auto pieces = decompose (to - from);
        int pos = from;
        for (size_t i = 0; i < pieces.size(); ++i)
        {
            const auto& v = pieces[i];
            const bool tieStop = pos > g.start;
            const bool tieStart = pos + v.duration < g.end;
            for (size_t k = 0; k < g.pitches.size(); ++k)
            {
                auto* n = measure.createNewChildElement ("note");
                n->setAttribute ("dynamics", juce::String (g.velocity / 90.0 * 100.0, 1));
                if (k > 0) n->createNewChildElement ("chord");
                addPitch (*n, g.pitches[k], drums);
                n->createNewChildElement ("duration")->addTextElement (juce::String (v.duration));
                if (tieStop)  n->createNewChildElement ("tie")->setAttribute ("type", "stop");
                if (tieStart) n->createNewChildElement ("tie")->setAttribute ("type", "start");
                n->createNewChildElement ("voice")->addTextElement (juce::String (voice));
                n->createNewChildElement ("type")->addTextElement (v.type);
                if (v.dotted) n->createNewChildElement ("dot");
                if (tieStop || tieStart)
                {
                    auto* nt = n->createNewChildElement ("notations");
                    if (tieStop)  nt->createNewChildElement ("tied")->setAttribute ("type", "stop");
                    if (tieStart) nt->createNewChildElement ("tied")->setAttribute ("type", "start");
                }
            }
            pos += v.duration;
        }
    }
}

bool writeMusicXml (const Song& song, const std::vector<InstrumentInfo>& infos, const juce::File& file, juce::String& error)
{
    const auto perf = perform (song, false);   // retriggers are a playback effect, not notation
    const auto parts = partsInUse (perf, song.numTracks());
    if (parts.empty()) { error = "The song has no notes to export."; return false; }

    auto quant = [] (double t) { return (int) std::round (t / kQuantum) * kQuantum; };
    const int numMeasures = juce::jmax (1, (quant (perf.length) + kMeasure - 1) / kMeasure);

    juce::XmlElement score ("score-partwise");
    score.setAttribute ("version", "4.0");
    score.createNewChildElement ("work")->createNewChildElement ("work-title")->addTextElement (song.title.isNotEmpty() ? song.title : juce::String ("Untitled"));
    auto* ident = score.createNewChildElement ("identification");
    auto* enc = ident->createNewChildElement ("encoding");
    enc->createNewChildElement ("software")->addTextElement ("Artilody Tracker");
    enc->createNewChildElement ("encoding-date")->addTextElement (juce::Time::getCurrentTime().formatted ("%Y-%m-%d"));

    auto* partList = score.createNewChildElement ("part-list");
    int nextChannel = 1;
    for (size_t pi = 0; pi < parts.size(); ++pi)
    {
        const auto info = infoFor (infos, song, parts[pi]);
        const juce::String id = "P" + juce::String ((int) pi + 1);
        auto* sp = partList->createNewChildElement ("score-part");
        sp->setAttribute ("id", id);
        sp->createNewChildElement ("part-name")->addTextElement (info.name);
        auto* si = sp->createNewChildElement ("score-instrument");
        si->setAttribute ("id", id + "-I1");
        si->createNewChildElement ("instrument-name")->addTextElement (info.name);
        auto* mi = sp->createNewChildElement ("midi-instrument");
        mi->setAttribute ("id", id + "-I1");
        int channel = 10;
        if (! info.drums) { channel = nextChannel; nextChannel = nextChannel % 16 + 1; if (nextChannel == 10) nextChannel = 11; }
        mi->createNewChildElement ("midi-channel")->addTextElement (juce::String (channel));
        if (! info.drums) mi->createNewChildElement ("midi-program")->addTextElement (juce::String (info.program + 1));
    }

    for (size_t pi = 0; pi < parts.size(); ++pi)
    {
        const int part = parts[pi];
        const auto info = infoFor (infos, song, part);

        // quantise to the 32nd grid and merge notes with the same start and end into chords
        std::vector<Group> groups;
        double pitchSum = 0; int pitchCount = 0;
        for (auto& n : perf.notes)
        {
            if (n.part != part) continue;
            const int s = quant (n.start);
            const int e = juce::jmax (s + kQuantum, quant (n.end));
            pitchSum += n.pitch; ++pitchCount;
            auto it = std::find_if (groups.begin(), groups.end(), [&] (const Group& g) { return g.start == s && g.end == e; });
            if (it != groups.end())
            {
                if (std::find (it->pitches.begin(), it->pitches.end(), n.pitch) == it->pitches.end())
                    it->pitches.push_back (n.pitch);
                it->velocity = juce::jmax (it->velocity, n.velocity);
            }
            else
            {
                groups.push_back ({ s, e, n.velocity, { n.pitch } });
            }
        }
        std::sort (groups.begin(), groups.end(), [] (const Group& a, const Group& b) { return a.start != b.start ? a.start < b.start : a.end < b.end; });
        for (auto& g : groups) std::sort (g.pitches.begin(), g.pitches.end());

        // overlapping notes with different lengths go into separate voices (max 4)
        constexpr int maxVoices = 4;
        std::vector<std::vector<Group>> voices (1);
        for (auto& g : groups)
        {
            bool placed = false;
            for (auto& v : voices)
                if (v.empty() || v.back().end <= g.start) { v.push_back (g); placed = true; break; }
            if (! placed && (int) voices.size() < maxVoices) { voices.push_back ({ g }); placed = true; }
            if (! placed)
            {
                // too much polyphony: shorten the note that ends first so this one fits
                auto& v = *std::min_element (voices.begin(), voices.end(), [] (auto& a, auto& b) { return a.back().end < b.back().end; });
                if (g.start - v.back().start >= kQuantum) { v.back().end = g.start; v.push_back (g); }
            }
        }

        const bool bass = pitchCount > 0 && pitchSum / pitchCount < 57.0 && ! info.drums;
        auto* xmlPart = score.createNewChildElement ("part");
        xmlPart->setAttribute ("id", "P" + juce::String ((int) pi + 1));

        for (int m = 0; m < numMeasures; ++m)
        {
            const int mStart = m * kMeasure, mEnd = mStart + kMeasure;
            auto* measure = xmlPart->createNewChildElement ("measure");
            measure->setAttribute ("number", m + 1);

            if (m == 0)
            {
                auto* attr = measure->createNewChildElement ("attributes");
                attr->createNewChildElement ("divisions")->addTextElement (juce::String (ticksPerQuarter));
                attr->createNewChildElement ("key")->createNewChildElement ("fifths")->addTextElement ("0");
                auto* time = attr->createNewChildElement ("time");
                time->createNewChildElement ("beats")->addTextElement ("4");
                time->createNewChildElement ("beat-type")->addTextElement ("4");
                auto* clef = attr->createNewChildElement ("clef");
                if (info.drums) clef->createNewChildElement ("sign")->addTextElement ("percussion");
                else
                {
                    clef->createNewChildElement ("sign")->addTextElement (bass ? "F" : "G");
                    clef->createNewChildElement ("line")->addTextElement (bass ? "4" : "2");
                }
            }

            // tempo marks (first part only)
            if (pi == 0)
                for (auto& tp : perf.tempos)
                {
                    const int t = quant (tp.tick);
                    if (t < mStart || t >= mEnd) continue;
                    auto* dir = measure->createNewChildElement ("direction");
                    dir->setAttribute ("placement", "above");
                    auto* met = dir->createNewChildElement ("direction-type")->createNewChildElement ("metronome");
                    met->createNewChildElement ("beat-unit")->addTextElement ("quarter");
                    met->createNewChildElement ("per-minute")->addTextElement (juce::String (juce::roundToInt (tp.bpm)));
                    if (t > mStart) dir->createNewChildElement ("offset")->addTextElement (juce::String (t - mStart));
                    dir->createNewChildElement ("sound")->setAttribute ("tempo", juce::String (tp.bpm, 2));
                }

            bool wroteVoice = false;
            for (size_t vi = 0; vi < voices.size(); ++vi)
            {
                const int voice = (int) vi + 1;
                std::vector<const Group*> inMeasure;
                for (auto& g : voices[vi])
                    if (g.start < mEnd && g.end > mStart) inMeasure.push_back (&g);

                if (inMeasure.empty() && vi > 0) continue;   // only voice 1 needs a rest for an empty measure

                if (wroteVoice)
                    measure->createNewChildElement ("backup")->createNewChildElement ("duration")->addTextElement (juce::String (kMeasure));
                wroteVoice = true;

                if (inMeasure.empty()) { addRest (*measure, kMeasure, voice, true); continue; }

                int cursor = mStart;
                for (auto* g : inMeasure)
                {
                    const int from = juce::jmax (g->start, mStart), to = juce::jmin (g->end, mEnd);
                    if (from > cursor) addRest (*measure, from - cursor, voice, false);
                    addGroupSegment (*measure, *g, from, to, voice, info.drums);
                    cursor = to;
                }
                if (cursor < mEnd) addRest (*measure, mEnd - cursor, voice, false);
            }
        }
    }

    juce::XmlElement::TextFormat format;
    format.dtd = "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\" \"http://www.musicxml.org/dtds/partwise.dtd\">";
    if (! score.writeTo (file, format)) { error = "Could not write " + file.getFullPathName(); return false; }
    return true;
}

} // namespace ScoreExport
} // namespace artilody
