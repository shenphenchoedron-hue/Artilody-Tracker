#include "BuiltinFX.h"
#include <complex>

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
BuiltinEQ::BuiltinEQ() : BuiltinEffect ("EQ") {}

juce::String BuiltinEQ::typeName (BandType t)
{
    switch (t)
    {
        case BandType::Bell:      return "Bell";
        case BandType::LowShelf:  return "Low Shelf";
        case BandType::HighShelf: return "High Shelf";
        case BandType::LowCut:    return "Low Cut";
        case BandType::HighCut:   return "High Cut";
        case BandType::Notch:     return "Notch";
    }
    return {};
}

juce::String BuiltinEQ::typeId (BandType t)
{
    switch (t)
    {
        case BandType::Bell:      return "bell";
        case BandType::LowShelf:  return "lowshelf";
        case BandType::HighShelf: return "highshelf";
        case BandType::LowCut:    return "lowcut";
        case BandType::HighCut:   return "highcut";
        case BandType::Notch:     return "notch";
    }
    return "bell";
}

BuiltinEQ::BandType BuiltinEQ::typeFromId (const juce::String& s)
{
    for (int i = 0; i <= (int) BandType::Notch; ++i)
        if (typeId ((BandType) i) == s)
            return (BandType) i;
    return BandType::Bell;
}

//------------------------------------------------------------------------------
int BuiltinEQ::addBand (BandType t, float f, float g, float q)
{
    for (int i = 0; i < kMaxBands; ++i)
    {
        auto& b = bands[(size_t) i];
        if (b.active.load()) continue;
        b.type = (int) t;
        b.freq = juce::jlimit (minFreq, maxFreq, f);
        b.gain = juce::jlimit (minGain, maxGain, g);
        b.q    = juce::jlimit (minQ, maxQ, q);
        b.generation = b.generation.load() + 1;
        b.active = true;
        return i;
    }
    return -1;
}

void BuiltinEQ::removeBand (int i)
{
    if (juce::isPositiveAndBelow (i, kMaxBands))
        bands[(size_t) i].active = false;
}

void BuiltinEQ::removeAllBands()
{
    for (int i = 0; i < kMaxBands; ++i)
        removeBand (i);
}

void BuiltinEQ::setBand (int i, const BandSettings& s)
{
    if (! juce::isPositiveAndBelow (i, kMaxBands)) return;
    auto& b = bands[(size_t) i];
    if (! b.active.load()) return;   // only edits existing bands - use addBand / removeBand to (de)activate
    const bool typeChanged = b.type.load() != (int) s.type;
    b.type = (int) s.type;
    b.freq = juce::jlimit (minFreq, maxFreq, s.freq);
    b.gain = juce::jlimit (minGain, maxGain, s.gain);
    b.q    = juce::jlimit (minQ, maxQ, s.q);
    if (typeChanged)
        b.generation = b.generation.load() + 1;
}

BuiltinEQ::BandSettings BuiltinEQ::getBand (int i) const
{
    BandSettings s;
    if (! juce::isPositiveAndBelow (i, kMaxBands)) return s;
    auto& b = bands[(size_t) i];
    s.active = b.active.load();
    s.type = (BandType) b.type.load();
    s.freq = b.freq.load();
    s.gain = b.gain.load();
    s.q = b.q.load();
    return s;
}

int BuiltinEQ::getNumActiveBands() const
{
    int n = 0;
    for (auto& b : bands)
        n += b.active.load() ? 1 : 0;
    return n;
}

//------------------------------------------------------------------------------
BuiltinEQ::Coeffs BuiltinEQ::makeCoeffs (BandType t, double sr, double f, double gainDb, double q)
{
    // RBJ "Audio EQ Cookbook" biquads, normalised by a0.
    f = juce::jlimit (10.0, sr * 0.49, f);
    q = juce::jmax (0.05, q);
    const double w0 = juce::MathConstants<double>::twoPi * f / sr;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double alpha = sw / (2.0 * q);
    const double A = std::pow (10.0, gainDb / 40.0);
    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

    switch (t)
    {
        case BandType::Bell:
            b0 = 1 + alpha * A;  b1 = -2 * cw;  b2 = 1 - alpha * A;
            a0 = 1 + alpha / A;  a1 = -2 * cw;  a2 = 1 - alpha / A;
            break;
        case BandType::LowShelf:
        {
            const double sa = 2 * std::sqrt (A) * alpha;
            b0 =      A * ((A + 1) - (A - 1) * cw + sa);
            b1 =  2 * A * ((A - 1) - (A + 1) * cw);
            b2 =      A * ((A + 1) - (A - 1) * cw - sa);
            a0 =           (A + 1) + (A - 1) * cw + sa;
            a1 =     -2 * ((A - 1) + (A + 1) * cw);
            a2 =           (A + 1) + (A - 1) * cw - sa;
            break;
        }
        case BandType::HighShelf:
        {
            const double sa = 2 * std::sqrt (A) * alpha;
            b0 =      A * ((A + 1) + (A - 1) * cw + sa);
            b1 = -2 * A * ((A - 1) + (A + 1) * cw);
            b2 =      A * ((A + 1) + (A - 1) * cw - sa);
            a0 =           (A + 1) - (A - 1) * cw + sa;
            a1 =      2 * ((A - 1) - (A + 1) * cw);
            a2 =           (A + 1) - (A - 1) * cw - sa;
            break;
        }
        case BandType::LowCut:
            b0 = (1 + cw) / 2;  b1 = -(1 + cw);  b2 = (1 + cw) / 2;
            a0 = 1 + alpha;     a1 = -2 * cw;    a2 = 1 - alpha;
            break;
        case BandType::HighCut:
            b0 = (1 - cw) / 2;  b1 = 1 - cw;     b2 = (1 - cw) / 2;
            a0 = 1 + alpha;     a1 = -2 * cw;    a2 = 1 - alpha;
            break;
        case BandType::Notch:
            b0 = 1;             b1 = -2 * cw;    b2 = 1;
            a0 = 1 + alpha;     a1 = -2 * cw;    a2 = 1 - alpha;
            break;
    }

    Coeffs c;
    c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0;
    return c;
}

double BuiltinEQ::magnitudeOf (const Coeffs& c, double f, double sr)
{
    const double w = juce::MathConstants<double>::twoPi * f / sr;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    const auto num = c.b0 + c.b1 * z1 + c.b2 * z2;
    const auto den = 1.0 + c.a1 * z1 + c.a2 * z2;
    return std::abs (num / den);
}

float BuiltinEQ::getMagnitudeDb (float f, int band) const
{
    const double sr = sampleRate > 0 ? sampleRate : 44100.0;
    double mag = 1.0;
    for (int i = 0; i < kMaxBands; ++i)
    {
        if (band >= 0 && i != band) continue;
        auto s = getBand (i);
        if (! s.active) continue;
        mag *= magnitudeOf (makeCoeffs (s.type, sr, s.freq, s.gain, s.q), f, sr);
    }
    return juce::Decibels::gainToDecibels ((float) mag, -100.0f);
}

//------------------------------------------------------------------------------
void BuiltinEQ::prepare (double sr, int)
{
    sampleRate = sr;
    reset();
}

void BuiltinEQ::reset()
{
    for (auto& st : state)
    {
        st.generation = 0xffffffff;   // forces a snap to the targets on the next block
        st.running = false;
        st.z1[0] = st.z1[1] = st.z2[0] = st.z2[1] = 0.0;
    }
}

void BuiltinEQ::process (float* left, float* right, int n)
{
    constexpr int sub = 16;   // coefficients are updated every 16 samples while a band moves
    const double k = 1.0 - std::exp (-(double) sub / (sampleRate * 0.025));   // ~25 ms glide

    for (int i = 0; i < kMaxBands; ++i)
    {
        auto& b = bands[(size_t) i];
        auto& st = state[(size_t) i];
        const bool active = b.active.load();
        const auto gen = b.generation.load();

        if (active && (gen != st.generation || ! st.running))
        {
            // new band (or new type): start exactly at the target, with clean filter memory
            st.generation = gen;
            st.running = true;
            st.type = b.type.load();
            st.freq = b.freq.load();
            st.gain = b.gain.load();
            st.q = b.q.load();
            st.c = makeCoeffs ((BandType) st.type, sampleRate, st.freq, st.gain, st.q);
            st.z1[0] = st.z1[1] = st.z2[0] = st.z2[1] = 0.0;
        }
        else if (! active && st.running)
        {
            // removed: glide bells/shelves to 0 dB before stopping, stop other types at once
            if (! typeHasGain ((BandType) st.type) || std::abs (st.gain) < 0.05)
            {
                st.running = false;
                continue;
            }
        }

        if (! st.running) continue;

        const double tFreq = std::log (active ? (double) b.freq.load() : st.freq);
        const double tGain = active ? (double) b.gain.load() : 0.0;
        const double tQ    = active ? (double) b.q.load() : st.q;

        for (int start = 0; start < n; start += sub)
        {
            const int len = juce::jmin (sub, n - start);

            const double lf = std::log (st.freq);
            const bool moving = std::abs (lf - tFreq) > 1.0e-4 || std::abs (st.gain - tGain) > 1.0e-3 || std::abs (st.q - tQ) > 1.0e-4;
            if (moving)
            {
                st.freq = std::exp (lf + (tFreq - lf) * k);
                st.gain += (tGain - st.gain) * k;
                st.q    += (tQ - st.q) * k;
                st.c = makeCoeffs ((BandType) st.type, sampleRate, st.freq, st.gain, st.q);
            }

            const auto c = st.c;
            float* ch[2] = { left + start, right + start };
            for (int chn = 0; chn < 2; ++chn)
            {
                double z1 = st.z1[chn], z2 = st.z2[chn];
                auto* d = ch[chn];
                for (int s = 0; s < len; ++s)
                {
                    // transposed direct form II
                    const double x = d[s];
                    const double y = c.b0 * x + z1;
                    z1 = c.b1 * x - c.a1 * y + z2;
                    z2 = c.b2 * x - c.a2 * y;
                    d[s] = (float) y;
                }
                // guard against denormals / blow-ups
                st.z1[chn] = std::abs (z1) < 1.0e-20 ? 0.0 : z1;
                st.z2[chn] = std::abs (z2) < 1.0e-20 ? 0.0 : z2;
            }
        }
    }
}

//------------------------------------------------------------------------------
juce::ValueTree BuiltinEQ::toValueTree() const
{
    juce::ValueTree v ("FX");
    v.setProperty ("name", name, nullptr);
    v.setProperty ("enabled", enabled.load(), nullptr);
    for (int i = 0; i < kMaxBands; ++i)
    {
        auto s = getBand (i);
        if (! s.active) continue;
        juce::ValueTree bv ("BAND");
        bv.setProperty ("type", typeId (s.type), nullptr);
        bv.setProperty ("freq", s.freq, nullptr);
        bv.setProperty ("gain", s.gain, nullptr);
        bv.setProperty ("q", s.q, nullptr);
        v.appendChild (bv, nullptr);
    }
    return v;
}

void BuiltinEQ::fromValueTree (const juce::ValueTree& v)
{
    if (! v.isValid()) return;
    enabled = (bool) v.getProperty ("enabled", false);
    removeAllBands();

    bool anyBand = false;
    for (auto bv : v)
    {
        if (! bv.hasType ("BAND")) continue;
        anyBand = true;
        addBand (typeFromId (bv.getProperty ("type", "bell").toString()),
                 (float) bv.getProperty ("freq", 1000.0f), (float) bv.getProperty ("gain", 0.0f), (float) bv.getProperty ("q", 1.0f));
    }

    // Projects from 1.0.0 - 1.0.5 used a fixed low cut + low shelf + mid bell + high shelf EQ.
    if (! anyBand && v.hasProperty ("lowcut"))
    {
        const float lowcut = v.getProperty ("lowcut", 20.0f);
        const float lowgain = v.getProperty ("lowgain", 0.0f), midgain = v.getProperty ("midgain", 0.0f), higain = v.getProperty ("higain", 0.0f);
        if (lowcut > 21.0f)              addBand (BandType::LowCut, lowcut, 0.0f, 0.7071f);
        if (std::abs (lowgain) > 0.01f)  addBand (BandType::LowShelf, v.getProperty ("lowfreq", 120.0f), lowgain, 0.7071f);
        if (std::abs (midgain) > 0.01f)  addBand (BandType::Bell, v.getProperty ("midfreq", 1000.0f), midgain, v.getProperty ("midq", 0.9f));
        if (std::abs (higain) > 0.01f)   addBand (BandType::HighShelf, v.getProperty ("hifreq", 8000.0f), higain, 0.7071f);
    }
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
