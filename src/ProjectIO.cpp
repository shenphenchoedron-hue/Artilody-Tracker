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

} // namespace ProjectIO
} // namespace artilody
