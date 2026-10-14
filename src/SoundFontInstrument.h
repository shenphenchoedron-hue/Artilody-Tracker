#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

struct tsf;

namespace artilody
{

/** A loaded .sf2 file shared by every instrument that uses it. */
class SoundFontFile
{
public:
    ~SoundFontFile();

    static std::shared_ptr<SoundFontFile> load (const juce::File& file, juce::String& error);

    /** Shared instance for the bundled general MIDI font (may be null if none was found). */
    static std::shared_ptr<SoundFontFile> getDefault();
    static juce::File findDefaultSoundFont();
    static void setDefaultOverride (const juce::File&);

    struct PresetInfo { juce::String name; int bank; int program; };
    const std::vector<PresetInfo>& getPresets() const noexcept { return presets; }

    juce::File getFile() const { return file; }
    tsf* createVoiceEngine() const; // returns a tsf_copy - caller must tsf_close

private:
    SoundFontFile() = default;
    tsf* base = nullptr;
    juce::File file;
    std::vector<PresetInfo> presets;
};

//==============================================================================
struct GenrePreset
{
    const char* name;
    int bank;
    int program;
    bool drums;
};

struct Genre
{
    const char* name;
    std::vector<GenrePreset> presets;
};

/** Built-in genre libraries: Pop, Rock, Electro and Jazz. */
const std::vector<Genre>& getGenres();

//==============================================================================
/** Instrument processor playing a preset from a SoundFont. */
class SoundFontInstrument final : public juce::AudioProcessor
{
public:
    SoundFontInstrument();
    ~SoundFontInstrument() override;

    /** Message thread. Builds a new voice engine and hands it to the audio thread. */
    bool setSoundFont (std::shared_ptr<SoundFontFile> font);
    void setPreset (int bank, int program, bool drums, const juce::String& displayName);

    std::shared_ptr<SoundFontFile> getSoundFont() const { return font; }
    int getBank() const noexcept       { return bank; }
    int getProgram() const noexcept    { return program; }
    bool isDrums() const noexcept      { return drums; }
    juce::String getPresetName() const { return presetName; }
    juce::String genre;

    //==============================================================================
    const juce::String getName() const override { return presetName.isNotEmpty() ? presetName : juce::String ("SoundFont"); }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    double getTailLengthSeconds() const override { return 2.0; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return presetName; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    void applyPresetToEngine (tsf* engine);

    std::shared_ptr<SoundFontFile> font;
    int bank = 0, program = 0;
    bool drums = false;
    juce::String presetName;

    juce::SpinLock engineLock;
    tsf* engine = nullptr;
    double currentRate = 44100.0;
    std::vector<float> renderBuffer;
    int pitchRangeSemis = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundFontInstrument)
};

} // namespace artilody
