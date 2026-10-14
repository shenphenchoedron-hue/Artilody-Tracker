#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
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

    virtual juce::ValueTree toValueTree() const;
    virtual void fromValueTree (const juce::ValueTree&);

    /** Puts every parameter back to its default (does not change 'enabled'). */
    virtual void resetToDefaults()
    {
        for (int i = 0; i < numParams(); ++i)
            set (i, params[(size_t) i]->defaultValue);
    }

protected:
    void addParam (const char* id, const char* n, const char* suffix, float mn, float mx, float def, float centre)
    {
        params.push_back (std::make_unique<FxParam> (id, n, suffix, mn, mx, def, centre));
    }

    std::atomic<bool> dirty { true };
    double sampleRate = 44100.0;
};

//==============================================================================
/** Parametric EQ with up to kMaxBands freely placed IIR biquad bands (RBJ cookbook filters).

    Bands are edited on the message thread through atomics; the audio thread smooths frequency,
    gain and Q towards their targets and recomputes the biquad coefficients every few samples, so
    dragging a band while audio plays does not click or zipper.
*/
class BuiltinEQ final : public BuiltinEffect
{
public:
    BuiltinEQ();

    static constexpr int kMaxBands = 8;
    static constexpr float minFreq = 20.0f, maxFreq = 20000.0f;
    static constexpr float minGain = -24.0f, maxGain = 24.0f;
    static constexpr float minQ = 0.1f, maxQ = 18.0f;

    enum class BandType { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch };
    static juce::String typeName (BandType);
    static juce::String typeId (BandType);
    static BandType typeFromId (const juce::String&);
    /** True if the band type has a gain parameter (bell and shelves). */
    static bool typeHasGain (BandType t) { return t == BandType::Bell || t == BandType::LowShelf || t == BandType::HighShelf; }

    struct BandSettings
    {
        bool active = false;
        BandType type = BandType::Bell;
        float freq = 1000.0f, gain = 0.0f, q = 1.0f;
    };

    // message thread API ------------------------------------------------------
    int  addBand (BandType, float freq, float gain, float q);   ///< returns band index or -1 if full
    void removeBand (int index);
    void removeAllBands();
    void setBand (int index, const BandSettings&);             ///< edits an active band (ignores BandSettings::active)
    BandSettings getBand (int index) const;
    int  getNumActiveBands() const;
    double getSampleRate() const noexcept { return sampleRate; }

    /** Magnitude response in dB at frequency f; band = -1 for all bands (uses target values). */
    float getMagnitudeDb (float frequency, int band = -1) const;

    // BuiltinEffect ------------------------------------------------------------
    void prepare (double sampleRate, int maxBlock) override;
    void process (float* left, float* right, int numSamples) override;
    void reset() override;
    void resetToDefaults() override { removeAllBands(); }
    juce::ValueTree toValueTree() const override;
    void fromValueTree (const juce::ValueTree&) override;

    struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    static Coeffs makeCoeffs (BandType, double sampleRate, double freq, double gainDb, double q);
    static double magnitudeOf (const Coeffs&, double freq, double sampleRate);

private:
    struct Band
    {
        std::atomic<bool> active { false };
        std::atomic<int> type { 0 };
        std::atomic<float> freq { 1000.0f }, gain { 0.0f }, q { 1.0f };
        std::atomic<uint32_t> generation { 0 };   // bumped when a slot is (re)used or the type changes
    };

    struct BandState   // audio thread only
    {
        uint32_t generation = 0xffffffff;
        bool running = false;
        int type = 0;
        double freq = 1000, gain = 0, q = 1;  // smoothed values
        Coeffs c;
        double z1[2] {}, z2[2] {};
    };

    std::array<Band, kMaxBands> bands;
    std::array<BandState, kMaxBands> state;
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
