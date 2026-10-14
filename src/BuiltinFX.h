#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <memory>
#include <vector>

namespace artilody
{

struct FxParam
{
    FxParam (juce::String i, juce::String n, juce::String suf, float mn, float mx, float d, float centre)
        : id (std::move (i)), name (std::move (n)), suffix (std::move (suf)), minValue (mn), maxValue (mx), defaultValue (d), skewCentre (centre), value (d) {}

    juce::String id, name, suffix;
    float minValue, maxValue, defaultValue, skewCentre;
    std::atomic<float> value;
};

/** Base class for the three built-in channel strip effects. */
class BuiltinEffect
{
public:
    explicit BuiltinEffect (juce::String effectName) : name (std::move (effectName)) {}
    virtual ~BuiltinEffect() = default;

    const juce::String name;
    std::atomic<bool> enabled { false };
    std::vector<std::unique_ptr<FxParam>> params;

    float get (int i) const noexcept       { return params[(size_t) i]->value.load(); }
    void set (int i, float v) noexcept     { params[(size_t) i]->value = v; dirty = true; }
    int numParams() const noexcept         { return (int) params.size(); }

    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void process (float* left, float* right, int numSamples) = 0;
    virtual void reset() = 0;

    /** A short readout for the UI, e.g. gain reduction. */
    virtual juce::String getStatusText() const { return {}; }

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);

protected:
    void addParam (const char* id, const char* n, const char* suffix, float mn, float mx, float def, float centre)
    {
        params.push_back (std::make_unique<FxParam> (id, n, suffix, mn, mx, def, centre));
    }

    std::atomic<bool> dirty { true };
    double sampleRate = 44100.0;
};

//==============================================================================
class BuiltinEQ final : public BuiltinEffect
{
public:
    BuiltinEQ();
    enum { LowCut, LowFreq, LowGain, MidFreq, MidGain, MidQ, HighFreq, HighGain };

    void prepare (double sampleRate, int maxBlock) override;
    void process (float* left, float* right, int numSamples) override;
    void reset() override;

    /** Magnitude response in dB at frequency f (UI thread, approximate). */
    float getMagnitudeDb (float frequency) const;

private:
    void updateCoefficients();
    using Filter = juce::dsp::IIR::Filter<float>;
    Filter cut[2], low[2], mid[2], high[2];
    std::array<float, 6> cutC {}, lowC {}, midC {}, highC {};
};

//==============================================================================
class BuiltinCompressor final : public BuiltinEffect
{
public:
    BuiltinCompressor();
    enum { Threshold, Ratio, Attack, Release, Knee, Makeup };

    void prepare (double sampleRate, int maxBlock) override;
    void process (float* left, float* right, int numSamples) override;
    void reset() override;
    juce::String getStatusText() const override;

    std::atomic<float> gainReductionDb { 0.0f };

private:
    float envelopeDb = 0.0f;
    float attackCoef = 0.0f, releaseCoef = 0.0f;
};

//==============================================================================
class BuiltinReverb final : public BuiltinEffect
{
public:
    BuiltinReverb();
    enum { Size, Damping, Width, Mix };

    void prepare (double sampleRate, int maxBlock) override;
    void process (float* left, float* right, int numSamples) override;
    void reset() override;

private:
    juce::Reverb reverb;
};

} // namespace artilody
