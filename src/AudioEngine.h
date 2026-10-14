#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include "Song.h"
#include "StripProcessor.h"
#include <array>

namespace artilody
{

class AudioEngine final : public juce::AudioIODeviceCallback,
                          public juce::AudioPlayHead,
                          public juce::MidiInputCallback,
                          public juce::ChangeBroadcaster
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void initialiseAudio (const juce::XmlElement* savedDeviceState);
    void shutdownAudio();

    juce::AudioDeviceManager deviceManager;
    Song song;

    StripProcessor& getStrip (int track) { return *strips[(size_t) track]; }
    StripProcessor& getMaster() { return master; }
    int getNumStrips() const { return (int) strips.size(); }

    //==============================================================================
    // Structure (message thread). These keep patterns, tracks and strips in sync.
    int addTrack (const juce::String& name, int index = -1);
    void removeTrack (int index);
    void moveTrack (int from, int to);
    void setTrackColumns (int track, int columns);
    void resetToEmpty (int numTracks);
    std::unique_ptr<StripProcessor> takeStripForRemoval (int index);

    /** Called (message thread) before a hosted processor is destroyed, so editor windows can close. */
    std::function<void (juce::AudioProcessor*)> onProcessorDeleted;
    void notifyProcessorDeleted (juce::AudioProcessor*);
    void notifyStripDeleted (StripProcessor&);

    /** Called on the message thread for incoming MIDI notes (for recording). */
    std::function<void (int note, int velocity, bool isNoteOn)> onMidiNote;

    void songStructureChanged() { sendChangeMessage(); }

    //==============================================================================
    // Transport
    void play (int sequenceIndex, int row, bool loopPattern);
    void stop();
    bool isPlaying() const noexcept { return playing.load(); }
    bool isLoopingPattern() const noexcept { return loopingPattern.load(); }

    std::atomic<int> playSeqIndex { 0 }, playRow { 0 };
    std::atomic<double> runtimeBpm { 128.0 };
    std::atomic<int> runtimeLpb { 4 };
    void setBpm (double bpm);
    void setLpb (int lpb);

    //==============================================================================
    // Note preview (any thread)
    void previewNoteOn (int track, int note, float velocity);
    void previewNoteOff (int track, int note);
    void previewAllOff();
    std::atomic<int> midiInputTrack { 0 };

    double getSampleRate() const noexcept { return sampleRate; }
    int getBlockSize() const noexcept { return blockSize; }
    float getCpuLoad() { return (float) deviceManager.getCpuUsage(); }

    //==============================================================================
    /** Offline render of the whole song to a WAV file (call from a background thread). */
    bool renderToFile (const juce::File& file, double rate, int bitDepth, std::function<bool (float)> progress, juce::String& error);

    //==============================================================================
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

    juce::Optional<PositionInfo> getPosition() const override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    void prepareAll (double rate, int block);

private:
    struct Voice { int note = -1; int instTrack = -1; };
    using ColumnVoices = std::array<Voice, kMaxNoteColumns>;

    enum class EvType : uint8_t { NoteOn, NoteOff };
    struct ScheduledEvent
    {
        int64_t time;
        EvType type;
        int16_t track;
        int8_t column;
        int8_t note;
        int16_t instTrack;
        float velocity;
    };

    struct PreviewEvent { int track; int note; float velocity; bool on; };

    void renderBlock (int numSamples);
    void runSequencer (int64_t blockStart, int numSamples);
    void triggerRow (int64_t rowTime, int64_t blockStart, int numSamples);
    void dispatchScheduled (int64_t blockStart, int numSamples);
    void startColumnNote (int track, int column, int note, int instTrack, float velocity, int offset);
    void stopColumnNote (int track, int column, int offset);
    void allVoicesOff (int offset);
    int resolveInstrument (int track, const Cell& c) const;
    double samplesPerRow() const;
    void syncVoiceArrays();

    std::vector<std::unique_ptr<StripProcessor>> strips;
    StripProcessor master { true };

    std::vector<juce::MidiBuffer> midiBuffers;
    std::vector<ColumnVoices> voices;
    std::vector<ScheduledEvent> scheduled;
    juce::AudioBuffer<float> trackBuffer, masterBuffer;

    juce::SpinLock previewLock;
    std::vector<PreviewEvent> previewQueue, previewWork;

    double sampleRate = 44100.0;
    int blockSize = 512;
    bool prepared = false;

    // sequencer state (audio thread)
    std::atomic<bool> playing { false }, loopingPattern { false };
    std::atomic<int> transportRequest { 0 }; // 1 = start, 2 = stop
    std::atomic<int> requestSeq { 0 }, requestRow { 0 };
    std::atomic<bool> requestLoop { false };
    int seqIndex = 0, row = 0;
    bool firstRow = true;
    int pendingJump = -1;
    double nextRowTime = 0.0;
    double lastRowTime = 0.0;
    int64_t sampleCounter = 0;
    double linesPlayed = 0.0;
    std::atomic<double> ppqAtBlockStart { 0.0 };
    std::atomic<int64_t> samplesPlayed { 0 };
    bool songWrapped = false;
    bool exporting = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace artilody
