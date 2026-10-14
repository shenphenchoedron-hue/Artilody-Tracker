#include "BuiltinFX.h"

namespace artilody
{

using AC = juce::dsp::IIR::ArrayCoefficients<float>;

juce::ValueTree BuiltinEffect::toValueTree() const
{
    juce::ValueTree v ("FX");
    v.setProperty ("name", name, nullptr);
    v.setProperty ("enabled", enabled.load(), nullptr);
    for (auto& p : params)
        v.setProperty (p->id, p->value.load(), nullptr);
    return v;
}

void BuiltinEffect::fromValueTree (const juce::ValueTree& v)
{
    if (! v.isValid()) return;
    enabled = (bool) v.getProperty ("enabled", false);
    for (auto& p : params)
        p->value = juce::jlimit (p->minValue, p->maxValue, (float) v.getProperty (p->id, p->defaultValue));
    dirty = true;
}

//==============================================================================
BuiltinEQ::BuiltinEQ() : BuiltinEffect ("EQ")
{
    addParam ("lowcut",  "Low Cut",   "Hz", 20.0f, 1000.0f, 20.0f, 120.0f);
    addParam ("lowfreq", "Low Freq",  "Hz", 30.0f, 600.0f, 120.0f, 150.0f);
    addParam ("lowgain", "Low Gain",  "dB", -18.0f, 18.0f, 0.0f, 0.0f);
    addParam ("midfreq", "Mid Freq",  "Hz", 200.0f, 8000.0f, 1000.0f, 1200.0f);
    addParam ("midgain", "Mid Gain",  "dB", -18.0f, 18.0f, 0.0f, 0.0f);
    addParam ("midq",    "Mid Q",     "",   0.2f, 6.0f, 0.9f, 1.2f);
    addParam ("hifreq",  "High Freq", "Hz", 2000.0f, 16000.0f, 8000.0f, 6000.0f);
    addParam ("higain",  "High Gain", "dB", -18.0f, 18.0f, 0.0f, 0.0f);
}

void BuiltinEQ::prepare (double sr, int)
{
    sampleRate = sr;
    for (auto* arr : { cut, low, mid, high })
        for (int c = 0; c < 2; ++c)
        {
            arr[c].coefficients = new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
            arr[c].reset();
        }
    dirty = true;
    updateCoefficients();
}

void BuiltinEQ::updateCoefficients()
{
    const auto sr = sampleRate;
    const auto nyq = (float) sr * 0.45f;
    cutC  = AC::makeHighPass (sr, juce::jmin (get (LowCut), nyq), 0.7071f);
    lowC  = AC::makeLowShelf (sr, juce::jmin (get (LowFreq), nyq), 0.7071f, juce::Decibels::decibelsToGain (get (LowGain)));
    midC  = AC::makePeakFilter (sr, juce::jmin (get (MidFreq), nyq), get (MidQ), juce::Decibels::decibelsToGain (get (MidGain)));
    highC = AC::makeHighShelf (sr, juce::jmin (get (HighFreq), nyq), 0.7071f, juce::Decibels::decibelsToGain (get (HighGain)));

    for (int c = 0; c < 2; ++c)
    {
        *cut[c].coefficients = cutC;
        *low[c].coefficients = lowC;
        *mid[c].coefficients = midC;
        *high[c].coefficients = highC;
    }
}

void BuiltinEQ::process (float* left, float* right, int n)
{
    if (dirty.exchange (false))
        updateCoefficients();

    const bool useCut = get (LowCut) > 21.0f;
    const bool useLow = std::abs (get (LowGain)) > 0.01f;
    const bool useMid = std::abs (get (MidGain)) > 0.01f;
    const bool useHigh = std::abs (get (HighGain)) > 0.01f;

    float* ch[2] = { left, right };
    for (int c = 0; c < 2; ++c)
    {
        auto* d = ch[c];
        for (int i = 0; i < n; ++i)
        {
            float x = d[i];
            if (useCut)  x = cut[c].processSample (x);
            if (useLow)  x = low[c].processSample (x);
            if (useMid)  x = mid[c].processSample (x);
            if (useHigh) x = high[c].processSample (x);
            d[i] = x;
        }
    }
}

void BuiltinEQ::reset()
{
    for (auto* arr : { cut, low, mid, high })
        for (int c = 0; c < 2; ++c)
            arr[c].reset();
}

float BuiltinEQ::getMagnitudeDb (float f) const
{
    const auto sr = sampleRate > 0 ? sampleRate : 44100.0;
    const auto nyq = (float) sr * 0.45f;
    double mag = 1.0;

    auto apply = [&] (const std::array<float, 6>& a)
    {
        juce::dsp::IIR::Coefficients<float> c (a[0], a[1], a[2], a[3], a[4], a[5]);
        mag *= c.getMagnitudeForFrequency (f, sr);
    };

    if (get (LowCut) > 21.0f)              apply (AC::makeHighPass (sr, juce::jmin (get (LowCut), nyq), 0.7071f));
    if (std::abs (get (LowGain)) > 0.01f)  apply (AC::makeLowShelf (sr, juce::jmin (get (LowFreq), nyq), 0.7071f, juce::Decibels::decibelsToGain (get (LowGain))));
    if (std::abs (get (MidGain)) > 0.01f)  apply (AC::makePeakFilter (sr, juce::jmin (get (MidFreq), nyq), get (MidQ), juce::Decibels::decibelsToGain (get (MidGain))));
    if (std::abs (get (HighGain)) > 0.01f) apply (AC::makeHighShelf (sr, juce::jmin (get (HighFreq), nyq), 0.7071f, juce::Decibels::decibelsToGain (get (HighGain))));
    return juce::Decibels::gainToDecibels ((float) mag, -60.0f);
}

//==============================================================================
BuiltinCompressor::BuiltinCompressor() : BuiltinEffect ("Compressor")
{
    addParam ("threshold", "Threshold", "dB", -60.0f, 0.0f, -18.0f, -18.0f);
    addParam ("ratio",     "Ratio",     ":1", 1.0f, 20.0f, 4.0f, 4.0f);
    addParam ("attack",    "Attack",    "ms", 0.1f, 200.0f, 10.0f, 15.0f);
    addParam ("release",   "Release",   "ms", 10.0f, 1500.0f, 150.0f, 200.0f);
    addParam ("knee",      "Knee",      "dB", 0.0f, 24.0f, 6.0f, 6.0f);
    addParam ("makeup",    "Makeup",    "dB", 0.0f, 24.0f, 0.0f, 6.0f);
}

void BuiltinCompressor::prepare (double sr, int)
{
    sampleRate = sr;
    reset();
}

void BuiltinCompressor::reset()
{
    envelopeDb = 0.0f;
    gainReductionDb = 0.0f;
}

void BuiltinCompressor::process (float* left, float* right, int n)
{
    const float thr = get (Threshold);
    const float ratio = juce::jmax (1.0f, get (Ratio));
    const float knee = get (Knee);
    const float makeup = juce::Decibels::decibelsToGain (get (Makeup));
    attackCoef  = std::exp (-1.0f / (0.001f * get (Attack)  * (float) sampleRate));
    releaseCoef = std::exp (-1.0f / (0.001f * get (Release) * (float) sampleRate));

    float maxGr = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float peak = juce::jmax (std::abs (left[i]), std::abs (right[i]));
        const float inDb = peak > 1.0e-6f ? 20.0f * std::log10 (peak) : -120.0f;

        // static curve with soft knee
        const float over = inDb - thr;
        float grTarget = 0.0f;
        if (knee > 0.0f && std::abs (over) <= knee * 0.5f)
        {
            const float x = over + knee * 0.5f;
            grTarget = (1.0f - 1.0f / ratio) * x * x / (2.0f * knee);
        }
        else if (over > 0.0f)
        {
            grTarget = over * (1.0f - 1.0f / ratio);
        }

        const float coef = grTarget > envelopeDb ? attackCoef : releaseCoef;
        envelopeDb = grTarget + coef * (envelopeDb - grTarget);

        const float g = std::pow (10.0f, -envelopeDb / 20.0f) * makeup;
        left[i] *= g;
        right[i] *= g;
        maxGr = juce::jmax (maxGr, envelopeDb);
    }
    gainReductionDb = maxGr;
}

juce::String BuiltinCompressor::getStatusText() const
{
    return "GR " + juce::String (-gainReductionDb.load(), 1) + " dB";
}

//==============================================================================
BuiltinReverb::BuiltinReverb() : BuiltinEffect ("Reverb")
{
    addParam ("size",    "Size",    "", 0.0f, 1.0f, 0.55f, 0.5f);
    addParam ("damping", "Damping", "", 0.0f, 1.0f, 0.45f, 0.5f);
    addParam ("width",   "Width",   "", 0.0f, 1.0f, 1.0f, 0.5f);
    addParam ("mix",     "Mix",     "", 0.0f, 1.0f, 0.25f, 0.5f);
}

void BuiltinReverb::prepare (double sr, int)
{
    sampleRate = sr;
    reverb.setSampleRate (sr);
    reverb.reset();
    dirty = true;
}

void BuiltinReverb::process (float* left, float* right, int n)
{
    if (dirty.exchange (false))
    {
        juce::Reverb::Parameters p;
        const float mix = get (Mix);
        p.roomSize = get (Size);
        p.damping = get (Damping);
        p.width = get (Width);
        p.wetLevel = mix * 0.5f;
        p.dryLevel = 0.5f * (1.0f - mix * 0.5f);
        p.freezeMode = 0.0f;
        reverb.setParameters (p);
    }
    reverb.processStereo (left, right, n);
}

void BuiltinReverb::reset()
{
    reverb.reset();
}

} // namespace artilody
