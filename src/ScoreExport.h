#pragma once

#include "Song.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace artilody
{

/** Exports the song as a Standard MIDI File or as MusicXML.

    The song is "performed" exactly like the sequencer plays it (sequence order, Bxx jumps,
    tempo / lines-per-beat changes, delays, cuts, note-offs and the Ins column), and the result
    is written as one MIDI track / MusicXML part per instrument (track) that plays notes.
*/
namespace ScoreExport
{
    static constexpr int ticksPerQuarter = 480;

    /** What the exporter needs to know about each track's instrument (gathered on the message thread). */
    struct InstrumentInfo
    {
        juce::String name;      // track name
        int program = 0;        // General MIDI program 0-127
        bool drums = false;     // plays on MIDI channel 10 / percussion staff
    };

    struct NoteEvent
    {
        int part;               // instrument track index
        double start, end;      // in ticks (ticksPerQuarter per quarter note)
        int pitch, velocity;
        bool retrigger = false;
    };

    struct ControlEvent { int part; double tick; bool pitchBend; int value; };
    struct TempoEvent   { double tick; double bpm; };

    struct Performance
    {
        std::vector<NoteEvent> notes;
        std::vector<ControlEvent> controls;
        std::vector<TempoEvent> tempos;
        double length = 0.0;    // ticks
    };

    /** Plays the song once (from sequence slot 0 until it ends or jumps backwards). */
    Performance perform (const Song&, bool includeRetriggers);

    bool writeMidi (const Song&, const std::vector<InstrumentInfo>&, const juce::File&, juce::String& error);
    bool writeMusicXml (const Song&, const std::vector<InstrumentInfo>&, const juce::File&, juce::String& error);
}

} // namespace artilody
