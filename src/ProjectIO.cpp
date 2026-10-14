#include "ProjectIO.h"
#include "PluginHost.h"

namespace artilody
{
namespace ProjectIO
{

static juce::ValueTree pluginToTree (juce::AudioPluginInstance& p)
{
    juce::ValueTree v ("PLUGIN");
    if (auto xml = p.getPluginDescription().createXml())
        v.appendChild (juce::ValueTree::fromXml (*xml), nullptr);
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    v.setProperty ("state", mb.toBase64Encoding(), nullptr);
    return v;
}

static std::unique_ptr<juce::AudioPluginInstance> pluginFromTree (const juce::ValueTree& v, AudioEngine& e, juce::StringArray& problems)
{
    if (v.getNumChildren() == 0) return {};
    auto xml = v.getChild (0).createXml();
    juce::PluginDescription d;
    if (xml == nullptr || ! d.loadFromXml (*xml)) return {};

    juce::String err;
    auto p = PluginHost::get().createPlugin (d, e.getSampleRate(), e.getBlockSize(), err);
    if (p == nullptr)
    {
        problems.add (d.name + " (" + d.pluginFormatName + "): " + err);
        return {};
    }
    juce::MemoryBlock mb;
    if (mb.fromBase64Encoding (v.getProperty ("state").toString()) && mb.getSize() > 0)
        p->setStateInformation (mb.getData(), (int) mb.getSize());
    return p;
}

static juce::ValueTree stripToTree (StripProcessor& s)
{
    juce::ValueTree v ("STRIP");
    v.setProperty ("volume", s.volumeDb.load(), nullptr);
    v.setProperty ("pan", s.pan.load(), nullptr);
    v.setProperty ("mute", s.mute.load(), nullptr);
    v.setProperty ("solo", s.solo.load(), nullptr);
    v.setProperty ("fxCollapsed", s.fxCollapsed, nullptr);

    juce::ValueTree inst ("INSTRUMENT");
    if (auto* sf = s.getSoundFont())
    {
        inst.setProperty ("kind", "soundfont", nullptr);
        inst.setProperty ("genre", sf->genre, nullptr);
        inst.setProperty ("name", sf->getPresetName(), nullptr);
        inst.setProperty ("bank", sf->getBank(), nullptr);
        inst.setProperty ("program", sf->getProgram(), nullptr);
        inst.setProperty ("drums", sf->isDrums(), nullptr);
        auto font = sf->getSoundFont();
        auto def = SoundFontFile::getDefault();
        if (font != nullptr && (def == nullptr || font->getFile() != def->getFile()))
            inst.setProperty ("file", font->getFile().getFullPathName(), nullptr);
    }
    else if (auto* p = s.getInstrumentPlugin())
    {
        inst.setProperty ("kind", "plugin", nullptr);
        inst.appendChild (pluginToTree (*p), nullptr);
    }
    else
    {
        inst.setProperty ("kind", "none", nullptr);
    }
    v.appendChild (inst, nullptr);

    juce::ValueTree chain ("FXCHAIN");
    for (int i = 0; i < s.getNumFx(); ++i)
    {
        auto* slot = s.getFx (i);
        auto pt = pluginToTree (*slot->plugin);
        pt.setProperty ("bypass", slot->bypassed.load(), nullptr);
        chain.appendChild (pt, nullptr);
    }
    v.appendChild (chain, nullptr);
    v.appendChild (s.builtinsToValueTree(), nullptr);
    return v;
}

static void stripFromTree (StripProcessor& s, const juce::ValueTree& v, AudioEngine& e, juce::StringArray& problems)
{
    s.volumeDb = (float) v.getProperty ("volume", 0.0f);
    s.pan = (float) v.getProperty ("pan", 0.0f);
    s.mute = (bool) v.getProperty ("mute", false);
    s.solo = (bool) v.getProperty ("solo", false);
    s.fxCollapsed = (bool) v.getProperty ("fxCollapsed", false);

    auto inst = v.getChildWithName ("INSTRUMENT");
    const auto kind = inst.getProperty ("kind").toString();
    if (kind == "soundfont")
    {
        std::shared_ptr<SoundFontFile> font;
        const auto file = inst.getProperty ("file").toString();
        if (file.isNotEmpty())
        {
            juce::String err;
            font = SoundFontFile::load (juce::File (file), err);
            if (font == nullptr) problems.add (err);
        }
        if (font == nullptr)
            font = SoundFontFile::getDefault();

        auto sf = std::make_unique<SoundFontInstrument>();
        sf->genre = inst.getProperty ("genre").toString();
        sf->setPreset ((int) inst.getProperty ("bank"), (int) inst.getProperty ("program"), (bool) inst.getProperty ("drums"), inst.getProperty ("name").toString());
        sf->setSoundFont (font);
        s.setInstrument (std::move (sf), InstrumentKind::SoundFont);
    }
    else if (kind == "plugin")
    {
        if (auto p = pluginFromTree (inst.getChildWithName ("PLUGIN"), e, problems))
            s.setInstrument (std::move (p), InstrumentKind::Plugin);
    }

    for (auto pt : v.getChildWithName ("FXCHAIN"))
    {
        if (auto p = pluginFromTree (pt, e, problems))
        {
            s.insertFx (std::move (p));
            if (auto* slot = s.getFx (s.getNumFx() - 1))
                slot->bypassed = (bool) pt.getProperty ("bypass", false);
        }
    }

    s.builtinsFromValueTree (v.getChildWithName ("BUILTINS"));
}

bool save (AudioEngine& e, const juce::File& file, juce::String& error)
{
    juce::ValueTree root ("ARTILODY_PROJECT");
    root.setProperty ("version", ARTILODY_VERSION, nullptr);
    {
        const juce::ScopedLock sl (e.song.lock);
        root.appendChild (e.song.toValueTree(), nullptr);
    }

    juce::ValueTree strips ("STRIPS");
    for (int t = 0; t < e.getNumStrips(); ++t)
        strips.appendChild (stripToTree (e.getStrip (t)), nullptr);
    root.appendChild (strips, nullptr);

    auto master = stripToTree (e.getMaster());
    master.setProperty ("master", true, nullptr);
    root.appendChild (master, nullptr);

    auto xml = root.createXml();
    if (xml == nullptr || ! xml->writeTo (file))
    {
        error = "Could not write " + file.getFullPathName();
        return false;
    }
    return true;
}

bool load (AudioEngine& e, const juce::File& file, juce::String& error)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("ARTILODY_PROJECT"))
    {
        error = "Not an Artilody Tracker project: " + file.getFileName();
        return false;
    }
    auto root = juce::ValueTree::fromXml (*xml);
    auto songTree = root.getChildWithName ("SONG");
    const int numTracks = songTree.getChildWithName ("TRACKS").getNumChildren();

    e.resetToEmpty (0);
    for (int i = 0; i < numTracks; ++i)
        e.addTrack ("Track");

    {
        const juce::ScopedLock sl (e.song.lock);
        e.song.fromValueTree (songTree);
        e.runtimeBpm = e.song.bpm;
        e.runtimeLpb = e.song.lpb;
    }

    juce::StringArray problems;
    auto strips = root.getChildWithName ("STRIPS");
    for (int t = 0; t < e.getNumStrips() && t < strips.getNumChildren(); ++t)
        stripFromTree (e.getStrip (t), strips.getChild (t), e, problems);

    for (auto c : root)
        if (c.hasType ("STRIP") && (bool) c.getProperty ("master", false))
            stripFromTree (e.getMaster(), c, e, problems);

    e.songStructureChanged();

    if (! problems.isEmpty())
        error = "Some plug-ins could not be loaded:\n" + problems.joinIntoString ("\n");
    return true;
}

//==============================================================================
void setSoundFontInstrument (StripProcessor& s, const juce::String& genre, const juce::String& presetName)
{
    for (auto& g : getGenres())
    {
        if (genre != g.name) continue;
        for (auto& p : g.presets)
        {
            if (presetName != p.name) continue;
            auto sf = std::make_unique<SoundFontInstrument>();
            sf->genre = g.name;
            sf->setPreset (p.bank, p.program, p.drums, p.name);
            sf->setSoundFont (SoundFontFile::getDefault());
            s.setInstrument (std::move (sf), InstrumentKind::SoundFont);
            return;
        }
    }
}

void createEmptySong (AudioEngine& e)
{
    e.resetToEmpty (4);
    const char* names[] = { "Drums", "Bass", "Chords", "Lead" };
    for (int i = 0; i < 4; ++i)
        e.song.tracks[(size_t) i].name = names[i];

    setSoundFontInstrument (e.getStrip (0), "Pop", "Pop Drum Kit");
    setSoundFontInstrument (e.getStrip (1), "Pop", "Finger Bass");
    setSoundFontInstrument (e.getStrip (2), "Pop", "Grand Piano");
    setSoundFontInstrument (e.getStrip (3), "Pop", "Square Lead");
    e.setTrackColumns (2, 3);
    e.song.patterns[0].name = "Init";
    e.songStructureChanged();
}

void createDemoSong (AudioEngine& e)
{
    e.resetToEmpty (5);
    const char* names[] = { "Drums", "Bass", "Chords", "Lead", "Pad" };
    for (int i = 0; i < 5; ++i)
        e.song.tracks[(size_t) i].name = names[i];

    setSoundFontInstrument (e.getStrip (0), "Pop", "Pop Drum Kit");
    setSoundFontInstrument (e.getStrip (1), "Pop", "Finger Bass");
    setSoundFontInstrument (e.getStrip (2), "Jazz", "Rhodes Piano");
    setSoundFontInstrument (e.getStrip (3), "Electro", "Saw Lead");
    setSoundFontInstrument (e.getStrip (4), "Electro", "Polysynth Pad");
    e.setTrackColumns (0, 2);
    e.setTrackColumns (2, 3);
    e.setTrackColumns (4, 2);

    e.setBpm (112.0);
    e.setLpb (4);
    e.song.title = "Sunrise Circuits";

    // channel strip settings
    e.getStrip (0).comp.enabled = true;
    e.getStrip (0).volumeDb = -2.0f;
    e.getStrip (1).volumeDb = -3.0f;
    e.getStrip (1).eq.enabled = true;
    e.getStrip (1).eq.set (BuiltinEQ::LowGain, 3.0f);
    e.getStrip (2).reverb.enabled = true;
    e.getStrip (2).reverb.set (BuiltinReverb::Mix, 0.22f);
    e.getStrip (2).volumeDb = -5.0f;
    e.getStrip (2).pan = -0.15f;
    e.getStrip (3).reverb.enabled = true;
    e.getStrip (3).reverb.set (BuiltinReverb::Mix, 0.3f);
    e.getStrip (3).volumeDb = -11.0f;
    e.getStrip (3).pan = 0.15f;
    e.getStrip (4).reverb.enabled = true;
    e.getStrip (4).reverb.set (BuiltinReverb::Mix, 0.45f);
    e.getStrip (4).reverb.set (BuiltinReverb::Size, 0.8f);
    e.getStrip (4).volumeDb = -14.0f;
    e.getMaster().comp.enabled = true;
    e.getMaster().comp.set (BuiltinCompressor::Threshold, -10.0f);
    e.getMaster().comp.set (BuiltinCompressor::Ratio, 2.0f);

    const juce::ScopedLock sl (e.song.lock);
    auto& song = e.song;
    song.patterns.clear();
    const int a = song.addPattern (64, "Groove");
    const int b = song.addPattern (64, "Groove + Lead");
    song.patterns[(size_t) a].colour = Song::palette()[1];
    song.patterns[(size_t) b].colour = Song::palette()[3];
    song.patterns[(size_t) a].notes = "Drums, bass and Rhodes chords: C - Am - F - G.";
    song.patterns[(size_t) b].notes = "Same groove with a saw lead and a polysynth pad.";

    auto put = [&song] (int pat, int track, int row, int col, int note, int vol = -1, char fx = 0, int fxv = 0)
    {
        auto& c = song.patterns[(size_t) pat].cell (track, row, col);
        c.note = (int8_t) note;
        c.ins = note >= 0 ? (uint8_t) (track + 1) : (uint8_t) 0xFF;
        if (vol >= 0) c.vol = (uint8_t) vol;
        if (fx != 0) { c.fxCmd = fx; c.fxVal = (uint8_t) fxv; }
    };

    const int roots[4] = { 36, 33, 29, 31 };                         // C, A, F, G (bass)
    const int chords[4][3] = { { 60, 64, 67 }, { 57, 60, 64 }, { 57, 60, 65 }, { 59, 62, 67 } };
    const int pads[4][2] = { { 48, 55 }, { 45, 52 }, { 41, 48 }, { 43, 50 } };
    const int lead[4][6][2] = {
        { { 0, 76 }, { 4, 79 }, { 6, 76 }, { 8, 74 }, { 12, 72 }, { 14, 74 } },
        { { 0, 72 }, { 4, 76 }, { 8, 81 }, { 10, 79 }, { 12, 76 }, { -1, 0 } },
        { { 0, 77 }, { 4, 76 }, { 6, 74 }, { 8, 72 }, { 12, 69 }, { -1, 0 } },
        { { 0, 71 }, { 4, 74 }, { 8, 79 }, { 11, 77 }, { 12, 74 }, { 14, kNoteOff } },
    };

    for (int pat : { a, b })
    {
        for (int bar = 0; bar < 4; ++bar)
        {
            const int o = bar * 16;
            // drums
            for (int r : { 0, 7, 8 })    put (pat, 0, o + r, 0, 36, 0x7F);
            if (bar == 3 && pat == b)    put (pat, 0, o + 14, 0, 38, 0x50, 'R', 0x23);
            for (int r : { 4, 12 })      put (pat, 0, o + r, 0, 38, 0x70);
            for (int r = 0; r < 16; r += 2)
                put (pat, 0, o + r, 1, r == 14 ? 46 : 42, (r % 4 == 0) ? 0x50 : 0x34);

            // bass
            const int bassRows[] = { 0, 3, 6, 8, 10, 14 };
            for (int i = 0; i < 6; ++i)
                put (pat, 1, o + bassRows[i], 0, roots[bar] + (bassRows[i] == 10 ? 12 : 0), bassRows[i] == 0 ? 0x7A : 0x5C);
            put (pat, 1, o + 15, 0, kNoteOff);

            // chords
            for (int c = 0; c < 3; ++c)
            {
                put (pat, 2, o, c, chords[bar][c], 0x48);
                put (pat, 2, o + 10, c, chords[bar][c], 0x38, c == 0 ? 0 : 'D', c * 0x18);
                put (pat, 2, o + 14, c, kNoteOff);
            }

            if (pat == b)
            {
                for (auto& n : lead[bar])
                    if (n[0] >= 0)
                        put (pat, 3, o + n[0], 0, n[1], 0x60);
                for (int c = 0; c < 2; ++c)
                    put (pat, 4, o, c, pads[bar][c], 0x50);
            }
        }
    }
    song.patterns[(size_t) a].cell (0, 0, 1).fxCmd = 0;
    song.sequence = { a, a, b, b };
}

//==============================================================================
void createJazzDemo (AudioEngine& e)
{
    e.resetToEmpty (5);
    const char* names[] = { "Drums", "Bass", "Piano", "Tenor Sax", "Vibes" };
    for (int i = 0; i < 5; ++i)
        e.song.tracks[(size_t) i].name = names[i];

    setSoundFontInstrument (e.getStrip (0), "Jazz", "Jazz Kit");
    setSoundFontInstrument (e.getStrip (1), "Jazz", "Acoustic Bass");
    setSoundFontInstrument (e.getStrip (2), "Jazz", "Grand Piano");
    setSoundFontInstrument (e.getStrip (3), "Jazz", "Tenor Sax");
    setSoundFontInstrument (e.getStrip (4), "Jazz", "Vibraphone");
    e.setTrackColumns (0, 3);
    e.setTrackColumns (2, 4);

    e.setBpm (132.0);
    e.setLpb (4);
    e.song.title = "Blue Corner Swing";

    // mix
    e.getStrip (0).volumeDb = -4.0f;
    e.getStrip (0).reverb.enabled = true;
    e.getStrip (0).reverb.set (BuiltinReverb::Mix, 0.12f);
    e.getStrip (1).volumeDb = -1.0f;
    e.getStrip (1).eq.enabled = true;
    e.getStrip (1).eq.set (BuiltinEQ::LowGain, 2.5f);
    e.getStrip (1).eq.set (BuiltinEQ::LowCut, 35.0f);
    e.getStrip (1).comp.enabled = true;
    e.getStrip (1).comp.set (BuiltinCompressor::Threshold, -16.0f);
    e.getStrip (1).comp.set (BuiltinCompressor::Ratio, 3.0f);
    e.getStrip (2).volumeDb = -7.0f;
    e.getStrip (2).pan = -0.3f;
    e.getStrip (2).reverb.enabled = true;
    e.getStrip (2).reverb.set (BuiltinReverb::Mix, 0.2f);
    e.getStrip (3).volumeDb = -5.0f;
    e.getStrip (3).pan = 0.2f;
    e.getStrip (3).reverb.enabled = true;
    e.getStrip (3).reverb.set (BuiltinReverb::Mix, 0.25f);
    e.getStrip (3).eq.enabled = true;
    e.getStrip (3).eq.set (BuiltinEQ::MidFreq, 1800.0f);
    e.getStrip (3).eq.set (BuiltinEQ::MidGain, 2.0f);
    e.getStrip (4).volumeDb = -6.0f;
    e.getStrip (4).pan = 0.4f;
    e.getStrip (4).reverb.enabled = true;
    e.getStrip (4).reverb.set (BuiltinReverb::Mix, 0.3f);
    e.getMaster().comp.enabled = true;
    e.getMaster().comp.set (BuiltinCompressor::Threshold, -12.0f);
    e.getMaster().comp.set (BuiltinCompressor::Ratio, 2.0f);
    e.getMaster().reverb.enabled = false;

    const juce::ScopedLock sl (e.song.lock);
    auto& song = e.song;
    song.patterns.clear();

    const int intro  = song.addPattern (64,  "Intro");
    const int head   = song.addPattern (128, "Head");
    const int solo   = song.addPattern (128, "Vibes Solo");
    const int ending = song.addPattern (48,  "Ending");
    song.patterns[(size_t) intro].colour  = Song::palette()[7];
    song.patterns[(size_t) head].colour   = Song::palette()[2];
    song.patterns[(size_t) solo].colour   = Song::palette()[1];
    song.patterns[(size_t) ending].colour = Song::palette()[0];
    song.patterns[(size_t) intro].notes  = "Piano, bass and drums set up the groove: Dm7 - G7 - Cmaj7 - A7.";
    song.patterns[(size_t) head].notes   = "Tenor sax plays the melody over a ii-V-I in C. Swing 8ths are made with D-effects (DAA = triplet feel).";
    song.patterns[(size_t) solo].notes   = "Vibraphone bebop solo over the same changes.";
    song.patterns[(size_t) ending].notes = "ii-V into a held Cmaj9 with a ritardando (T-effects).";

    constexpr int swing = 0xAA; // push the off-beat 8th to the last triplet

    auto put = [&song] (int pat, int track, int row, int col, int note, int vol = -1, char fx = 0, int fxv = 0)
    {
        auto& p = song.patterns[(size_t) pat];
        if (row < 0 || row >= p.numRows) return;
        auto& c = p.cell (track, row, col);
        c.note = (int8_t) note;
        c.ins = note >= 0 ? (uint8_t) (track + 1) : (uint8_t) 0xFF;
        c.vol = vol >= 0 ? (uint8_t) vol : (uint8_t) 0xFF;
        c.fxCmd = fx;
        c.fxVal = (uint8_t) fxv;
    };

    // eight bar form: | Dm7 | G7 | Cmaj7 | A7 | Dm7 | G7 | Cmaj7 | Cmaj7 |
    const int walk[8][4] = {
        { 38, 41, 45, 44 }, { 43, 47, 50, 49 }, { 48, 52, 55, 46 }, { 45, 49, 52, 51 },
        { 50, 48, 45, 44 }, { 43, 45, 47, 49 }, { 48, 47, 45, 43 }, { 40, 43, 45, 37 },
    };
    const int voicing[8][4] = {
        { 53, 57, 60, 64 }, { 53, 57, 59, 64 }, { 52, 55, 59, 62 }, { 55, 59, 61, 66 },
        { 53, 57, 60, 64 }, { 53, 57, 59, 64 }, { 52, 55, 59, 62 }, { 52, 57, 59, 62 },
    };
    // chord + scale tones for the improvised vibes line
    const std::vector<int> scales[8] = {
        { 62, 64, 65, 67, 69, 71, 72, 74, 76, 77 },          // D dorian
        { 62, 64, 65, 67, 68, 70, 71, 74, 75, 77 },          // G altered-ish / mixolydian b9
        { 60, 62, 64, 67, 69, 71, 72, 74, 76, 79 },          // C major / lydian colours
        { 61, 64, 65, 67, 69, 70, 73, 76, 77, 79 },          // A7b9
        { 62, 64, 65, 67, 69, 71, 72, 74, 76, 77 },
        { 62, 65, 67, 68, 70, 71, 74, 75, 77, 79 },
        { 60, 62, 64, 66, 67, 69, 71, 72, 74, 76 },
        { 60, 62, 64, 67, 69, 71, 72, 74, 76, 79 },
    };

    auto rhythmSection = [&] (int pat, int bars, int firstChord, bool busy)
    {
        for (int bar = 0; bar < bars; ++bar)
        {
            const int ch = (firstChord + bar) % 8;
            const int o = bar * 16;

            // drums: ride "ding ding-a ding", hi-hat on 2 & 4, feathered kick, ghost snare comping
            put (pat, 0, o + 0,  0, 51, 0x5A);
            put (pat, 0, o + 4,  0, 51, 0x6E);
            put (pat, 0, o + 6,  0, 51, 0x3C, 'D', swing);
            put (pat, 0, o + 8,  0, 51, 0x5A);
            put (pat, 0, o + 12, 0, 51, 0x6E);
            put (pat, 0, o + 14, 0, 51, 0x3C, 'D', swing);
            put (pat, 0, o + 4,  1, 44, 0x50);
            put (pat, 0, o + 12, 1, 44, 0x50);
            for (int b = 0; b < 4; ++b)
                put (pat, 0, o + b * 4, 2, 36, 0x16);
            if (busy || bar % 2 == 1)
            {
                put (pat, 0, o + 10, 2, 38, 0x26, 'D', swing);
                if (bar % 4 == 3)
                {
                    put (pat, 0, o + 14, 2, 38, 0x40, 'D', swing);
                    put (pat, 0, o + 12, 2, 38, 0x30);
                }
            }

            // walking bass, quarter notes with a little ghost skip
            for (int b = 0; b < 4; ++b)
                put (pat, 1, o + b * 4, 0, walk[ch][b], b == 0 ? 0x70 : 0x62);
            if (bar % 4 == 1)
                put (pat, 1, o + 14, 0, walk[ch][3] + 2, 0x40, 'D', swing);

            // piano comping: Charleston (1, and-of-2) or anticipations
            const bool charleston = (bar % 2 == 0);
            for (int c = 0; c < 4; ++c)
            {
                const int n = voicing[ch][c];
                if (charleston)
                {
                    put (pat, 2, o + 0, c, n, 0x4A);
                    put (pat, 2, o + 2, c, kNoteOff);
                    put (pat, 2, o + 6, c, n, 0x42, 'D', swing);
                    put (pat, 2, o + 11, c, kNoteOff);
                }
                else
                {
                    put (pat, 2, o + 2, c, n, 0x40, 'D', swing);
                    put (pat, 2, o + 5, c, kNoteOff);
                    put (pat, 2, o + 14, c, voicing[(ch + 1) % 8][c], 0x44, 'D', swing);
                }
            }
        }
    };

    // ---- Intro (bars 1-4 of the form, no melody)
    rhythmSection (intro, 4, 0, false);
    put (intro, 0, 0, 1, 49, 0x50, 'T', 132); // crash + tempo reset when the song loops

    // ---- Head
    rhythmSection (head, 8, 0, false);
    struct N { int bar, row, note; bool sw; int vol; };
    const N melody[] = {
        { 0, 0, 69, false, 0x60 }, { 0, 2, 72, true, 0x58 }, { 0, 4, 74, false, 0x64 }, { 0, 8, 77, false, 0x6C }, { 0, 14, 76, true, 0x5A },
        { 1, 0, 74, false, 0x62 }, { 1, 4, 71, false, 0x5C }, { 1, 6, 67, true, 0x58 }, { 1, 8, 65, false, 0x60 }, { 1, 12, 64, false, 0x5C }, { 1, 15, kNoteOff, false, -1 },
        { 2, 0, 64, false, 0x64 }, { 2, 8, 67, false, 0x5C }, { 2, 10, 71, true, 0x5C }, { 2, 13, kNoteOff, false, -1 },
        { 3, 0, 73, false, 0x66 }, { 3, 4, 76, false, 0x60 }, { 3, 8, 79, false, 0x6A }, { 3, 12, 77, false, 0x5E }, { 3, 14, 76, true, 0x58 },
        { 4, 0, 74, false, 0x66 }, { 4, 8, 72, false, 0x5C }, { 4, 10, 69, true, 0x58 }, { 4, 12, 65, false, 0x5C },
        { 5, 0, 67, false, 0x60 }, { 5, 2, 69, true, 0x58 }, { 5, 4, 71, false, 0x60 }, { 5, 6, 74, true, 0x5C }, { 5, 8, 77, false, 0x68 }, { 5, 12, 76, false, 0x62 },
        { 6, 0, 76, false, 0x68 }, { 6, 12, kNoteOff, false, -1 },
        { 7, 8, 67, false, 0x58 }, { 7, 10, 69, true, 0x58 }, { 7, 12, 71, false, 0x5C }, { 7, 14, 74, true, 0x60 },
    };
    for (auto& n : melody)
        put (head, 3, n.bar * 16 + n.row, 0, n.note, n.vol, n.sw ? 'D' : 0, n.sw ? swing : 0);

    // ---- Vibes solo: deterministic bebop-ish line on swung 8ths
    rhythmSection (solo, 8, 0, true);
    {
        juce::Random rng (2026);
        int idx = 4;
        for (int bar = 0; bar < 8; ++bar)
        {
            const auto& sc = scales[bar];
            idx = juce::jlimit (0, (int) sc.size() - 1, idx);
            for (int eighth = 0; eighth < 8; ++eighth)
            {
                const int row = bar * 16 + eighth * 2;
                const bool off = (eighth & 1) != 0;
                if (bar == 7 && eighth >= 5) break;                     // breathe before the head
                if (rng.nextInt (10) == 0 && eighth > 0) continue;      // occasional rest
                const int step = rng.nextInt (5) - 2;                   // mostly stepwise
                idx = juce::jlimit (0, (int) sc.size() - 1, idx + (step == 0 ? 1 : step));
                if (idx == 0 || idx == (int) sc.size() - 1) idx = (int) sc.size() / 2;
                const int vol = off ? 0x48 + rng.nextInt (12) : 0x5C + rng.nextInt (16);
                put (solo, 4, row, 0, sc[(size_t) idx], vol, off ? 'D' : 0, off ? swing : 0);
            }
        }
        put (solo, 4, 7 * 16 + 10, 0, kNoteOff);
    }

    // ---- Ending: ii - V - held Cmaj9 with ritardando
    rhythmSection (ending, 2, 4, false);
    for (int r = 32; r < 48; ++r)
        for (int c = 0; c < 4; ++c)
            song.patterns[(size_t) ending].cell (0, r, c).clear();
    {
        const int cmaj9[4] = { 52, 55, 59, 62 };
        for (int c = 0; c < 4; ++c)
            put (ending, 2, 32, c, cmaj9[c], 0x50);
        put (ending, 1, 32, 0, 36, 0x6A);
        put (ending, 3, 32, 0, 76, 0x5A);
        put (ending, 4, 32, 0, 79, 0x50);
        put (ending, 0, 32, 0, 49, 0x60);
        put (ending, 0, 32, 2, 36, 0x50);
        // ritardando
        put (ending, 0, 24, 1, -1, -1, 'T', 120);
        put (ending, 0, 28, 1, -1, -1, 'T', 100);
        put (ending, 0, 30, 1, -1, -1, 'T', 80);
        put (ending, 0, 33, 1, -1, -1, 'T', 60);
        // tenor sax pickup into the last note
        put (ending, 3, 24, 0, 74, 0x5A);
        put (ending, 3, 28, 0, 71, 0x58);
        put (ending, 3, 30, 0, 72, 0x5C, 'D', swing);
        for (int c = 0; c < 4; ++c)
            put (ending, 2, 47, c, kNoteOff);
        put (ending, 1, 47, 0, kNoteOff);
        put (ending, 3, 46, 0, kNoteOff);
        put (ending, 4, 47, 0, kNoteOff);
    }

    song.sequence = { intro, head, solo, head, ending };
}

} // namespace ProjectIO
} // namespace artilody
