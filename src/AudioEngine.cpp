#include "AudioEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace artilody
{

static constexpr int kTicksPerLine = 12;

AudioEngine::AudioEngine()
{
    previewQueue.reserve (512);
    previewWork.reserve (512);
    scheduled.reserve (2048);
    trackBuffer.setSize (2, 4096);
    masterBuffer.setSize (2, 4096);
}

AudioEngine::~AudioEngine()
{
    shutdownAudio();
}

void AudioEngine::initialiseAudio (const juce::XmlElement* saved)
{
    auto err = deviceManager.initialise (0, 2, saved, true);
    if (err.isNotEmpty())
        DBG ("Audio init: " << err);

    for (auto& m : juce::MidiInput::getAvailableDevices())
    {
        deviceManager.setMidiInputDeviceEnabled (m.identifier, true);
    }
    deviceManager.addMidiInputDeviceCallback ({}, this);
    deviceManager.addAudioCallback (this);
}

void AudioEngine::shutdownAudio()
{
    deviceManager.removeMidiInputDeviceCallback ({}, this);
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
}

//==============================================================================
void AudioEngine::syncVoiceArrays()
{
    midiBuffers.resize (strips.size());
    for (auto& m : midiBuffers)
        m.ensureSize (2048);
    voices.resize (strips.size());
}

int AudioEngine::addTrack (const juce::String& name, int index)
{
    if ((int) song.tracks.size() >= kMaxTracks)
        return -1;

    auto strip = std::make_unique<StripProcessor> (false);
    strip->prepare (sampleRate, blockSize, this);

    TrackInfo info;
    info.name = name;
    info.colour = song.nextTrackColour();
    info.uid = song.nextTrackUid();

    {
        const juce::ScopedLock sl (song.lock);
        if (index < 0 || index > (int) song.tracks.size())
            index = (int) song.tracks.size();

        // stop hanging notes before renumbering
        allVoicesOff (0);

        song.tracks.insert (song.tracks.begin() + index, info);
        for (auto& p : song.patterns)
            p.insertTrack (index);
        strips.insert (strips.begin() + index, std::move (strip));

        // instrument numbers referring to tracks after the insertion point shift up
        for (auto& p : song.patterns)
            for (auto& t : p.tracks)
                for (auto& c : t)
                    if (c.ins != 0xFF && c.ins >= index + 1 && c.ins < 0xFE)
                        ++c.ins;

        voices.insert (voices.begin() + index, ColumnVoices());
        syncVoiceArrays();
    }

    sendChangeMessage();
    return index;
}

std::unique_ptr<StripProcessor> AudioEngine::takeStripForRemoval (int index)
{
    std::unique_ptr<StripProcessor> s;
    const juce::ScopedLock sl (song.lock);
    if (! juce::isPositiveAndBelow (index, (int) strips.size()))
        return {};

    allVoicesOff (0);
    s = std::move (strips[(size_t) index]);
    strips.erase (strips.begin() + index);
    song.tracks.erase (song.tracks.begin() + index);
    for (auto& p : song.patterns)
    {
        p.removeTrack (index);
        for (auto& t : p.tracks)
            for (auto& c : t)
                if (c.ins != 0xFF)
                {
                    if (c.ins == index + 1) c.ins = 0xFF;
                    else if (c.ins > index + 1) --c.ins;
                }
    }
    voices.erase (voices.begin() + index);
    syncVoiceArrays();
    return s;
}

void AudioEngine::notifyProcessorDeleted (juce::AudioProcessor* p)
{
    if (p != nullptr && onProcessorDeleted)
        onProcessorDeleted (p);
}

void AudioEngine::notifyStripDeleted (StripProcessor& s)
{
    notifyProcessorDeleted (s.getInstrument());
    for (int i = 0; i < s.getNumFx(); ++i)
        notifyProcessorDeleted (s.getFx (i)->plugin.get());
}

void AudioEngine::removeTrack (int index)
{
    auto s = takeStripForRemoval (index);
    if (s != nullptr)
    {
        notifyStripDeleted (*s);
        s.reset();
        sendChangeMessage();
    }
}

void AudioEngine::moveTrack (int from, int to)
{
    {
        const juce::ScopedLock sl (song.lock);
        const int n = (int) strips.size();
        if (! juce::isPositiveAndBelow (from, n) || ! juce::isPositiveAndBelow (to, n) || from == to)
            return;

        allVoicesOff (0);
        auto s = std::move (strips[(size_t) from]);
        strips.erase (strips.begin() + from);
        strips.insert (strips.begin() + to, std::move (s));

        auto t = song.tracks[(size_t) from];
        song.tracks.erase (song.tracks.begin() + from);
        song.tracks.insert (song.tracks.begin() + to, t);

        // build mapping old track number -> new track number for instrument references
        std::vector<int> order (n);
        for (int i = 0; i < n; ++i) order[(size_t) i] = i;
        auto o = order[(size_t) from];
        order.erase (order.begin() + from);
        order.insert (order.begin() + to, o);
        std::vector<int> newIndexOf ((size_t) n);
        for (int i = 0; i < n; ++i) newIndexOf[(size_t) order[(size_t) i]] = i;

        for (auto& p : song.patterns)
        {
            p.moveTrack (from, to);
            for (auto& tr : p.tracks)
                for (auto& c : tr)
                    if (c.ins != 0xFF && c.ins >= 1 && c.ins <= n)
                        c.ins = (uint8_t) (newIndexOf[(size_t) (c.ins - 1)] + 1);
        }
        voices.assign (strips.size(), ColumnVoices());
        syncVoiceArrays();
    }
    sendChangeMessage();
}

void AudioEngine::setTrackColumns (int track, int columns)
{
    {
        const juce::ScopedLock sl (song.lock);
        if (! juce::isPositiveAndBelow (track, (int) song.tracks.size()))
            return;
        columns = juce::jlimit (1, kMaxNoteColumns, columns);
        for (int c = columns; c < kMaxNoteColumns; ++c)
            stopColumnNote (track, c, 0);
        song.tracks[(size_t) track].noteColumns = columns;
    }
    sendChangeMessage();
}

void AudioEngine::resetToEmpty (int numTracks)
{
    stop();
    juce::Thread::sleep (30);

    std::vector<std::unique_ptr<StripProcessor>> old;
    {
        const juce::ScopedLock sl (song.lock);
        old = std::move (strips);
        strips.clear();
        song.clear();
        song.addPattern (64, "Init");
        song.sequence = { 0 };
        voices.clear();
        scheduled.clear();
        syncVoiceArrays();
        runtimeBpm = song.bpm;
        runtimeLpb = song.lpb;
    }

    for (auto& s : old)
        notifyStripDeleted (*s);
    old.clear();

    {
        auto oldMaster = master.setInstrument (nullptr, InstrumentKind::None);
        while (master.getNumFx() > 0)
        {
            notifyProcessorDeleted (master.getFx (0)->plugin.get());
            master.removeFx (0);
        }
        for (int i = 0; i < 3; ++i)
        {
            auto* b = master.getBuiltin (i);
            for (int p = 0; p < b->numParams(); ++p)
                b->set (p, b->params[(size_t) p]->defaultValue);
            b->enabled = false;
        }
        master.volumeDb = 0.0f;
        master.pan = 0.0f;
    }

    for (int i = 0; i < numTracks; ++i)
        addTrack ("Track " + juce::String (i + 1).paddedLeft ('0', 2));

    sendChangeMessage();
}

//==============================================================================
void AudioEngine::play (int sequenceIndex, int startRow, bool loopPattern)
{
    requestSeq = sequenceIndex;
    requestRow = startRow;
    requestLoop = loopPattern;
    transportRequest = 1;
}

void AudioEngine::stop()
{
    transportRequest = 2;
}

void AudioEngine::setBpm (double bpm)
{
    bpm = juce::jlimit (20.0, 999.0, bpm);
    song.bpm = bpm;
    runtimeBpm = bpm;
}

void AudioEngine::setLpb (int lpb)
{
    lpb = juce::jlimit (1, 32, lpb);
    song.lpb = lpb;
    runtimeLpb = lpb;
}

void AudioEngine::previewNoteOn (int track, int note, float velocity)
{
    const juce::SpinLock::ScopedLockType sl (previewLock);
    if (previewQueue.size() < 500)
        previewQueue.push_back ({ track, note, velocity, true });
}

void AudioEngine::previewNoteOff (int track, int note)
{
    const juce::SpinLock::ScopedLockType sl (previewLock);
    if (previewQueue.size() < 500)
        previewQueue.push_back ({ track, note, 0.0f, false });
}

void AudioEngine::previewAllOff()
{
    const juce::SpinLock::ScopedLockType sl (previewLock);
    if (previewQueue.size() < previewQueue.capacity())
        previewQueue.push_back ({ -1, -1, 0.0f, false });
}

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (! (m.isNoteOn() || m.isNoteOff()))
        return;

    const int track = midiInputTrack.load();
    if (m.isNoteOn())
        previewNoteOn (track, m.getNoteNumber(), m.getFloatVelocity());
    else
        previewNoteOff (track, m.getNoteNumber());

    const int note = m.getNoteNumber();
    const int vel = m.getVelocity();
    const bool on = m.isNoteOn();
    juce::MessageManager::callAsync ([this, note, vel, on]
    {
        if (onMidiNote) onMidiNote (note, vel, on);
    });
}

//==============================================================================
double AudioEngine::samplesPerRow() const
{
    return sampleRate * 60.0 / (juce::jmax (1.0, runtimeBpm.load()) * juce::jmax (1, runtimeLpb.load()));
}

int AudioEngine::resolveInstrument (int track, const Cell& c) const
{
    if (c.ins != 0xFF && c.ins >= 1 && c.ins <= (int) strips.size())
        return c.ins - 1;
    return track;
}

void AudioEngine::startColumnNote (int track, int column, int note, int instTrack, float velocity, int offset)
{
    if (! juce::isPositiveAndBelow (track, (int) voices.size()) || ! juce::isPositiveAndBelow (instTrack, (int) midiBuffers.size()))
        return;

    stopColumnNote (track, column, offset);
    midiBuffers[(size_t) instTrack].addEvent (juce::MidiMessage::noteOn (1, note, juce::jlimit (0.0f, 1.0f, velocity)), offset);
    voices[(size_t) track][(size_t) column] = { note, instTrack };
}

void AudioEngine::stopColumnNote (int track, int column, int offset)
{
    if (! juce::isPositiveAndBelow (track, (int) voices.size()))
        return;

    auto& v = voices[(size_t) track][(size_t) column];
    if (v.note >= 0 && juce::isPositiveAndBelow (v.instTrack, (int) midiBuffers.size()))
        midiBuffers[(size_t) v.instTrack].addEvent (juce::MidiMessage::noteOff (1, v.note), offset);
    v = {};
}

void AudioEngine::allVoicesOff (int offset)
{
    for (int t = 0; t < (int) voices.size(); ++t)
        for (int c = 0; c < kMaxNoteColumns; ++c)
            stopColumnNote (t, c, offset);
    scheduled.clear();
}

void AudioEngine::triggerRow (int64_t rowTime, int64_t blockStart, int numSamples)
{
    if (song.sequence.empty() || song.patterns.empty())
        return;

    const int patIndex = juce::jlimit (0, (int) song.patterns.size() - 1, song.sequence[(size_t) juce::jlimit (0, (int) song.sequence.size() - 1, seqIndex)]);
    const auto& pat = song.patterns[(size_t) patIndex];
    if (row >= pat.numRows)
        row = 0;

    const int offset = juce::jlimit (0, numSamples - 1, (int) (rowTime - blockStart));
    const double spr = samplesPerRow();

    playSeqIndex = seqIndex;
    playRow = row;

    // first pass: global effects (tempo) so delays use the new speed
    for (int t = 0; t < (int) song.tracks.size() && t < (int) pat.tracks.size(); ++t)
        for (int col = 0; col < song.tracks[(size_t) t].noteColumns; ++col)
        {
            const auto& c = pat.cell (t, row, col);
            if (c.fxCmd == 'T' && c.fxVal >= 0x20) runtimeBpm = (double) c.fxVal;
            else if (c.fxCmd == 'L' && c.fxVal >= 1) runtimeLpb = juce::jlimit (1, 32, (int) c.fxVal);
            else if (c.fxCmd == 'B') pendingJump = c.fxVal;
        }

    for (int t = 0; t < (int) song.tracks.size() && t < (int) pat.tracks.size(); ++t)
    {
        for (int col = 0; col < song.tracks[(size_t) t].noteColumns; ++col)
        {
            const auto& c = pat.cell (t, row, col);
            if (c.isEmpty())
                continue;

            const int inst = resolveInstrument (t, c);
            const float vel = c.vol != 0xFF ? juce::jlimit (0.0f, 1.0f, (float) c.vol / 127.0f) : 0.8f;

            int64_t when = rowTime;
            if (c.fxCmd == 'D')
                when += (int64_t) (spr * c.fxVal / 256.0);

            if (c.note >= 0)
            {
                if (when <= rowTime)
                    startColumnNote (t, col, c.note, inst, vel, offset);
                else if (scheduled.size() < scheduled.capacity())
                    scheduled.push_back ({ when, EvType::NoteOn, (int16_t) t, (int8_t) col, c.note, (int16_t) inst, vel });

                if (c.fxCmd == 'R' && (c.fxVal & 0x0F) > 0)
                {
                    const int every = c.fxVal & 0x0F;
                    const float fall = (float) ((c.fxVal >> 4) & 0x0F) / 16.0f;
                    float v = vel;
                    for (int tick = every; tick < kTicksPerLine; tick += every)
                    {
                        v *= (1.0f - fall);
                        if (scheduled.size() < scheduled.capacity())
                            scheduled.push_back ({ rowTime + (int64_t) (spr * tick / kTicksPerLine), EvType::NoteOn,
                                                   (int16_t) t, (int8_t) col, c.note, (int16_t) inst, v });
                    }
                }
            }
            else if (c.note == kNoteOff)
            {
                if (when <= rowTime)
                    stopColumnNote (t, col, offset);
                else if (scheduled.size() < scheduled.capacity())
                    scheduled.push_back ({ when, EvType::NoteOff, (int16_t) t, (int8_t) col, -1, (int16_t) inst, 0.0f });
            }

            if (c.fxCmd == 'C' && scheduled.size() < scheduled.capacity())
                scheduled.push_back ({ rowTime + (int64_t) (spr * c.fxVal / 256.0), EvType::NoteOff, (int16_t) t, (int8_t) col, -1, (int16_t) inst, 0.0f });

            if (c.fxCmd == 'P' && juce::isPositiveAndBelow (inst, (int) midiBuffers.size()))
                midiBuffers[(size_t) inst].addEvent (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, (int) c.fxVal * 64)), offset);

            if (c.fxCmd == 'M' && juce::isPositiveAndBelow (inst, (int) midiBuffers.size()))
                midiBuffers[(size_t) inst].addEvent (juce::MidiMessage::controllerEvent (1, 1, juce::jmin (127, (int) c.fxVal)), offset);
        }
    }
}

void AudioEngine::dispatchScheduled (int64_t blockStart, int numSamples)
{
    const int64_t blockEnd = blockStart + numSamples;
    for (size_t i = 0; i < scheduled.size();)
    {
        auto& e = scheduled[i];
        if (e.time < blockEnd)
        {
            const int off = juce::jlimit (0, numSamples - 1, (int) (e.time - blockStart));
            if (e.type == EvType::NoteOn)
                startColumnNote (e.track, e.column, e.note, e.instTrack, e.velocity, off);
            else
                stopColumnNote (e.track, e.column, off);
            scheduled[i] = scheduled.back();
            scheduled.pop_back();
        }
        else
        {
            ++i;
        }
    }
}

void AudioEngine::runSequencer (int64_t blockStart, int numSamples)
{
    const int64_t blockEnd = blockStart + numSamples;

    while (playing.load() && nextRowTime < (double) blockEnd)
    {
        if (! firstRow)
        {
            // advance
            if (pendingJump >= 0 && ! loopingPattern.load())
            {
                if (pendingJump <= seqIndex) songWrapped = true;
                seqIndex = juce::jlimit (0, (int) song.sequence.size() - 1, pendingJump);
                row = 0;
                pendingJump = -1;
            }
            else
            {
                pendingJump = -1;
                ++row;
                const int patIndex = song.sequence.empty() ? 0 : song.sequence[(size_t) juce::jlimit (0, (int) song.sequence.size() - 1, seqIndex)];
                const int rows = song.patterns.empty() ? 64 : song.patterns[(size_t) juce::jlimit (0, (int) song.patterns.size() - 1, patIndex)].numRows;
                if (row >= rows)
                {
                    row = 0;
                    if (! loopingPattern.load())
                    {
                        ++seqIndex;
                        if (seqIndex >= (int) song.sequence.size())
                        {
                            seqIndex = 0;
                            songWrapped = true;
                        }
                    }
                }
            }
            linesPlayed += 1.0;
        }
        firstRow = false;

        if (exporting && songWrapped)
        {
            playing = false;
            break;
        }

        const auto rowTime = (int64_t) std::ceil (juce::jmax ((double) blockStart, nextRowTime));
        triggerRow (rowTime, blockStart, numSamples);
        lastRowTime = nextRowTime;
        nextRowTime += samplesPerRow();
    }

    if (playing.load())
    {
        const double frac = juce::jlimit (0.0, 1.0, ((double) blockStart - lastRowTime) / samplesPerRow());
        ppqAtBlockStart = (linesPlayed + frac) / (double) juce::jmax (1, runtimeLpb.load());
        samplesPlayed = (int64_t) (ppqAtBlockStart.load() * 60.0 / juce::jmax (1.0, runtimeBpm.load()) * sampleRate);
    }
}

void AudioEngine::renderBlock (int numSamples)
{
    const juce::ScopedLock sl (song.lock);

    for (auto& m : midiBuffers)
        m.clear();

    // transport requests
    const int req = transportRequest.exchange (0);
    if (req == 2)
    {
        allVoicesOff (0);
        for (auto& m : midiBuffers)
        {
            m.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        }
        playing = false;
        runtimeBpm = song.bpm;
        runtimeLpb = song.lpb;
    }
    else if (req == 1)
    {
        allVoicesOff (0);
        seqIndex = juce::jlimit (0, juce::jmax (0, (int) song.sequence.size() - 1), requestSeq.load());
        row = juce::jmax (0, requestRow.load());
        loopingPattern = requestLoop.load();
        firstRow = true;
        pendingJump = -1;
        nextRowTime = (double) sampleCounter;
        lastRowTime = nextRowTime;
        linesPlayed = 0.0;
        songWrapped = false;
        runtimeBpm = song.bpm;
        runtimeLpb = song.lpb;
        playing = true;
    }

    // preview notes
    {
        const juce::GenericScopedTryLock<juce::SpinLock> pl (previewLock);
        if (pl.isLocked())
            std::swap (previewQueue, previewWork);
    }
    for (auto& e : previewWork)
    {
        if (e.track < 0)
        {
            for (auto& m : midiBuffers)
                m.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            continue;
        }
        if (! juce::isPositiveAndBelow (e.track, (int) midiBuffers.size()))
            continue;
        if (e.on)
            midiBuffers[(size_t) e.track].addEvent (juce::MidiMessage::noteOn (1, e.note, e.velocity), 0);
        else
            midiBuffers[(size_t) e.track].addEvent (juce::MidiMessage::noteOff (1, e.note), 0);
    }
    previewWork.clear();

    const int64_t blockStart = sampleCounter;
    if (playing.load())
    {
        dispatchScheduled (blockStart, numSamples);
        runSequencer (blockStart, numSamples);
        dispatchScheduled (blockStart, numSamples);
    }

    masterBuffer.clear (0, numSamples);

    bool anySolo = false;
    for (auto& s : strips)
        anySolo = anySolo || s->solo.load();

    for (size_t t = 0; t < strips.size(); ++t)
    {
        auto& s = *strips[t];
        const bool audible = ! s.mute.load() && (! anySolo || s.solo.load());
        s.process (trackBuffer, midiBuffers[t], numSamples, audible);
        masterBuffer.addFrom (0, 0, trackBuffer, 0, 0, numSamples);
        masterBuffer.addFrom (1, 0, trackBuffer, 1, 0, numSamples);
    }

    juce::MidiBuffer none;
    master.process (masterBuffer, none, numSamples, ! master.mute.load());

    sampleCounter += numSamples;
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                    float* const* out, int numOut,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    juce::ScopedNoDenormals noDenormals;
    int done = 0;
    while (done < numSamples)
    {
        const int n = juce::jmin (blockSize, numSamples - done, masterBuffer.getNumSamples());
        renderBlock (n);
        for (int c = 0; c < numOut; ++c)
        {
            if (out[c] == nullptr) continue;
            if (c < 2)
                juce::FloatVectorOperations::copy (out[c] + done, masterBuffer.getReadPointer (c), n);
            else
                juce::FloatVectorOperations::clear (out[c] + done, n);
        }
        done += n;
    }
}

void AudioEngine::prepareAll (double rate, int block)
{
    const juce::ScopedLock sl (song.lock);
    sampleRate = rate > 0 ? rate : 44100.0;
    blockSize = juce::jlimit (16, 4096, block > 0 ? block : 512);
    trackBuffer.setSize (2, blockSize, false, true, false);
    masterBuffer.setSize (2, blockSize, false, true, false);

    for (auto& s : strips)
        s->prepare (sampleRate, blockSize, this);
    master.prepare (sampleRate, blockSize, this);
    syncVoiceArrays();
    prepared = true;
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepareAll (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void AudioEngine::audioDeviceStopped()
{
}

juce::Optional<juce::AudioPlayHead::PositionInfo> AudioEngine::getPosition() const
{
    PositionInfo info;
    info.setBpm (runtimeBpm.load());
    info.setTimeSignature (TimeSignature { 4, 4 });
    info.setIsPlaying (playing.load());
    info.setIsRecording (false);
    info.setIsLooping (loopingPattern.load());
    const double ppq = ppqAtBlockStart.load();
    info.setPpqPosition (ppq);
    info.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
    info.setTimeInSamples (samplesPlayed.load());
    info.setTimeInSeconds ((double) samplesPlayed.load() / sampleRate);
    return info;
}

//==============================================================================
bool AudioEngine::renderToFile (const juce::File& file, double rate, int bitDepth, std::function<bool (float)> progress, juce::String& error)
{
    const auto oldRate = sampleRate;
    const auto oldBlock = blockSize;

    deviceManager.removeAudioCallback (this);

    rate = rate > 0 ? rate : (sampleRate > 0 ? sampleRate : 44100.0);
    if (! juce::approximatelyEqual (rate, sampleRate) || ! prepared)
        prepareAll (rate, 512);

    for (auto& s : strips) s->setNonRealtime (true);
    master.setNonRealtime (true);

    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
    bool ok = false;

    if (stream == nullptr)
    {
        error = "Cannot write to " + file.getFullPathName();
    }
    else
    {
        std::unique_ptr<juce::AudioFormat> format;
        if (file.hasFileExtension ("aif;aiff"))  format = std::make_unique<juce::AiffAudioFormat>();
        else if (file.hasFileExtension ("flac")) format = std::make_unique<juce::FlacAudioFormat>();
        else                                     format = std::make_unique<juce::WavAudioFormat>();

        auto writer = format->createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                                      .withNumChannels (2)
                                                                                      .withBitsPerSample (bitDepth));
        if (writer == nullptr)
        {
            error = "Could not create the audio file writer";
        }
        else
        {

            int totalRows = 0;
            {
                const juce::ScopedLock sl (song.lock);
                for (auto s : song.sequence)
                    totalRows += song.patterns[(size_t) juce::jlimit (0, (int) song.patterns.size() - 1, s)].numRows;
            }

            exporting = true;
            play (0, 0, false);

            const int64_t maxSamples = (int64_t) (rate * 60.0 * 30.0);
            int64_t rendered = 0;
            int tailBlocks = -1;
            bool cancelled = false;
            int rowsDone = 0;

            while (rendered < maxSamples)
            {
                renderBlock (blockSize);
                writer->writeFromAudioSampleBuffer (masterBuffer, 0, blockSize);
                rendered += blockSize;

                if (! playing.load())
                {
                    if (tailBlocks < 0)
                        tailBlocks = (int) (rate * 2.5 / blockSize);
                    if (--tailBlocks <= 0)
                        break;
                }
                else
                {
                    rowsDone = 0;
                    const juce::ScopedLock sl (song.lock);
                    for (int i = 0; i < playSeqIndex.load() && i < (int) song.sequence.size(); ++i)
                        rowsDone += song.patterns[(size_t) juce::jlimit (0, (int) song.patterns.size() - 1, song.sequence[(size_t) i])].numRows;
                    rowsDone += playRow.load();
                }

                if (progress && ! progress (totalRows > 0 ? juce::jlimit (0.0f, 1.0f, (float) rowsDone / (float) totalRows) : 0.0f))
                {
                    cancelled = true;
                    break;
                }
            }

            exporting = false;
            stop();
            renderBlock (blockSize);
            writer.reset();
            ok = ! cancelled;
            if (cancelled)
            {
                file.deleteFile();
                error = "Export cancelled";
            }
        }
    }

    for (auto& s : strips) s->setNonRealtime (false);
    master.setNonRealtime (false);

    if (! juce::approximatelyEqual (oldRate, sampleRate))
        prepareAll (oldRate, oldBlock);

    deviceManager.addAudioCallback (this);
    return ok;
}

} // namespace artilody
