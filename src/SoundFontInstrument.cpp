#include <map>
#include <mutex>
#include <algorithm>

#define TSF_IMPLEMENTATION
#include "tsf.h"

#include "SoundFontInstrument.h"

namespace artilody
{

//==============================================================================
static juce::File& defaultOverride()
{
    static juce::File f;
    return f;
}

void SoundFontFile::setDefaultOverride (const juce::File& f) { defaultOverride() = f; }

SoundFontFile::~SoundFontFile()
{
    if (base != nullptr)
        tsf_close (base);
}

std::shared_ptr<SoundFontFile> SoundFontFile::load (const juce::File& f, juce::String& error)
{
    static std::mutex cacheLock;
    static std::map<juce::String, std::weak_ptr<SoundFontFile>> cache;

    std::lock_guard<std::mutex> sl (cacheLock);
    if (auto existing = cache[f.getFullPathName()].lock())
        return existing;

    juce::MemoryBlock data;
    if (! f.existsAsFile() || ! f.loadFileAsData (data))
    {
        error = "Could not read " + f.getFullPathName();
        return {};
    }

    auto* t = tsf_load_memory (data.getData(), (int) data.getSize());
    if (t == nullptr)
    {
        error = "Not a valid SoundFont: " + f.getFileName();
        return {};
    }

    std::shared_ptr<SoundFontFile> sf (new SoundFontFile());
    sf->base = t;
    sf->file = f;

    for (int i = 0; i < t->presetNum; ++i)
        sf->presets.push_back ({ juce::String (t->presets[i].presetName).trim(), (int) t->presets[i].bank, (int) t->presets[i].preset });

    std::sort (sf->presets.begin(), sf->presets.end(), [] (const PresetInfo& a, const PresetInfo& b)
    {
        return a.bank != b.bank ? a.bank < b.bank : a.program < b.program;
    });

    cache[f.getFullPathName()] = sf;
    return sf;
}

juce::File SoundFontFile::findDefaultSoundFont()
{
    if (defaultOverride().existsAsFile())
        return defaultOverride();

    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    const juce::String name = "GeneralUser-GS.sf2";

    juce::Array<juce::File> candidates {
        exe.getChildFile ("soundfonts").getChildFile (name),
        exe.getParentDirectory().getChildFile ("Resources/soundfonts").getChildFile (name),
        exe.getParentDirectory().getParentDirectory().getChildFile ("share/artilody-tracker/soundfonts").getChildFile (name),
        juce::File ("/usr/share/artilody-tracker/soundfonts").getChildFile (name),
        juce::File::getCurrentWorkingDirectory().getChildFile ("soundfonts").getChildFile (name),
        juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Artilody Tracker/soundfonts").getChildFile (name),
        juce::File ("/usr/share/sounds/sf2/FluidR3_GM.sf2"),
        juce::File ("/usr/share/sounds/sf2/default-GM.sf2"),
        juce::File ("/usr/share/soundfonts/default.sf2"),
        juce::File ("/usr/share/sounds/sf2/TimGM6mb.sf2"),
    };

    for (auto& c : candidates)
        if (c.existsAsFile())
            return c;

    return {};
}

std::shared_ptr<SoundFontFile> SoundFontFile::getDefault()
{
    static std::shared_ptr<SoundFontFile> def;
    static juce::File loadedFrom;

    auto f = findDefaultSoundFont();
    if (def == nullptr || f != loadedFrom)
    {
        juce::String err;
        if (f.existsAsFile())
            def = load (f, err);
        loadedFrom = f;
    }
    return def;
}

tsf* SoundFontFile::createVoiceEngine() const
{
    return base != nullptr ? tsf_copy (base) : nullptr;
}

//==============================================================================
const std::vector<Genre>& getGenres()
{
    static const std::vector<Genre> genres {
        { "Pop", {
            { "Pop Drum Kit",       128,  0, true  },
            { "Room Kit",           128,  8, true  },
            { "Grand Piano",          0,  0, false },
            { "Bright Piano",         0,  1, false },
            { "Electric Piano",       0,  5, false },
            { "Finger Bass",          0, 33, false },
            { "Steel Guitar",         0, 25, false },
            { "Nylon Guitar",         0, 24, false },
            { "String Ensemble",      0, 48, false },
            { "Synth Strings",        0, 50, false },
            { "Warm Pad",             0, 89, false },
            { "Choir Aahs",           0, 52, false },
            { "Square Lead",          0, 80, false },
            { "Glockenspiel",         0,  9, false },
            { "Flute",                0, 73, false },
        } },
        { "Rock", {
            { "Rock Drum Kit",      128, 16, true  },
            { "Standard Kit",       128,  0, true  },
            { "Overdrive Guitar",     0, 29, false },
            { "Distortion Guitar",    0, 30, false },
            { "Clean Guitar",         0, 27, false },
            { "Muted Guitar",         0, 28, false },
            { "Guitar Harmonics",     0, 31, false },
            { "Pick Bass",            0, 34, false },
            { "Finger Bass",          0, 33, false },
            { "Rock Organ",           0, 18, false },
            { "Drawbar Organ",        0, 16, false },
            { "Brass Section",        0, 61, false },
            { "Honky-Tonk Piano",     0,  3, false },
            { "Harmonica",            0, 22, false },
        } },
        { "Electro", {
            { "Electronic Kit",     128, 24, true  },
            { "808 Kit",            128, 25, true  },
            { "Synth Bass 1",         0, 38, false },
            { "Synth Bass 2",         0, 39, false },
            { "Saw Lead",             0, 81, false },
            { "Square Lead",          0, 80, false },
            { "Bass & Lead",          0, 87, false },
            { "Polysynth Pad",        0, 90, false },
            { "New Age Pad",          0, 88, false },
            { "Sweep Pad",            0, 95, false },
            { "Halo Pad",             0, 94, false },
            { "Synth Brass",          0, 62, false },
            { "Synth Strings",        0, 51, false },
            { "Atmosphere FX",        0, 99, false },
            { "Sci-Fi FX",            0,103, false },
            { "Synth Drum",           0,118, false },
        } },
        { "Jazz", {
            { "Jazz Kit",           128, 32, true  },
            { "Brush Kit",          128, 40, true  },
            { "Acoustic Bass",        0, 32, false },
            { "Fretless Bass",        0, 35, false },
            { "Jazz Guitar",          0, 26, false },
            { "Grand Piano",          0,  0, false },
            { "Rhodes Piano",         0,  4, false },
            { "Vibraphone",           0, 11, false },
            { "Tenor Sax",            0, 66, false },
            { "Alto Sax",             0, 65, false },
            { "Baritone Sax",         0, 67, false },
            { "Trumpet",              0, 56, false },
            { "Muted Trumpet",        0, 59, false },
            { "Trombone",             0, 57, false },
            { "Clarinet",             0, 71, false },
            { "Jazz Organ",           0, 17, false },
        } },
    };
    return genres;
}

//==============================================================================
SoundFontInstrument::SoundFontInstrument()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

SoundFontInstrument::~SoundFontInstrument()
{
    if (engine != nullptr)
        tsf_close (engine);
}

bool SoundFontInstrument::setSoundFont (std::shared_ptr<SoundFontFile> newFont)
{
    font = std::move (newFont);
    setPreset (bank, program, drums, presetName);
    return font != nullptr;
}

void SoundFontInstrument::applyPresetToEngine (tsf* e)
{
    tsf_set_output (e, TSF_STEREO_UNWEAVED, (int) currentRate, -3.0f);
    tsf_set_max_voices (e, 96);

    int idx = tsf_get_presetindex (e, drums ? 128 : bank, program);
    if (idx < 0 && drums) idx = tsf_get_presetindex (e, 128, 0);
    if (idx < 0)          idx = tsf_get_presetindex (e, 0, program);
    if (idx < 0)          idx = 0;

    tsf_channel_set_presetindex (e, 0, idx);
    tsf_channel_set_pitchrange (e, 0, (float) pitchRangeSemis);
}

void SoundFontInstrument::setPreset (int newBank, int newProgram, bool isDrumKit, const juce::String& displayName)
{
    bank = newBank;
    program = newProgram;
    drums = isDrumKit;
    presetName = displayName;

    tsf* newEngine = font != nullptr ? font->createVoiceEngine() : nullptr;
    if (newEngine != nullptr)
        applyPresetToEngine (newEngine);

    tsf* old = nullptr;
    {
        const juce::SpinLock::ScopedLockType sl (engineLock);
        old = engine;
        engine = newEngine;
    }
    if (old != nullptr)
        tsf_close (old);
}

void SoundFontInstrument::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    renderBuffer.assign ((size_t) juce::jmax (2048, samplesPerBlock) * 2, 0.0f);
    if (! juce::approximatelyEqual (sampleRate, currentRate) || engine == nullptr)
    {
        currentRate = sampleRate;
        setPreset (bank, program, drums, presetName);
    }
}

void SoundFontInstrument::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    const juce::GenericScopedTryLock<juce::SpinLock> sl (engineLock);
    if (! sl.isLocked() || engine == nullptr || buffer.getNumChannels() < 1)
        return;

    if ((int) renderBuffer.size() < numSamples * 2)
        return;

    auto renderChunk = [&] (int start, int len)
    {
        if (len <= 0) return;
        tsf_render_float (engine, renderBuffer.data(), len, 0);
        const float* l = renderBuffer.data();
        const float* r = renderBuffer.data() + len;
        buffer.copyFrom (0, start, l, len);
        if (buffer.getNumChannels() > 1)
            buffer.copyFrom (1, start, r, len);
        else
            buffer.addFrom (0, start, r, len);
    };

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, numSamples, meta.samplePosition);
        renderChunk (pos, t - pos);
        pos = t;

        const auto m = meta.getMessage();
        if (m.isNoteOn())
            tsf_channel_note_on (engine, 0, m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())
            tsf_channel_note_off (engine, 0, m.getNoteNumber());
        else if (m.isAllNotesOff())
            tsf_channel_note_off_all (engine, 0);
        else if (m.isAllSoundOff())
            tsf_channel_sounds_off_all (engine, 0);
        else if (m.isPitchWheel())
            tsf_channel_set_pitchwheel (engine, 0, m.getPitchWheelValue());
        else if (m.isController())
            tsf_channel_midi_control (engine, 0, m.getControllerNumber(), m.getControllerValue());
    }
    renderChunk (pos, numSamples - pos);
}

} // namespace artilody
