#include "EQEditor.h"

namespace artilody
{

using BandType = BuiltinEQ::BandType;

static constexpr float kDisplayDb = 24.0f;   // graph shows -24 .. +24 dB

static juce::String formatFreq (double f)
{
    return f >= 1000.0 ? juce::String (f / 1000.0, f >= 10000.0 ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";
}

static juce::String formatGain (double g)
{
    return (g > 0.05 ? "+" : "") + juce::String (g, 1) + " dB";
}

juce::Colour EQEditor::bandColour (int band)
{
    static const juce::Colour colours[] = {
        juce::Colour (0xffff7a59), juce::Colour (0xffffb020), juce::Colour (0xff29c79c), juce::Colour (0xff27c2ef),
        juce::Colour (0xff2f7bf0), juce::Colour (0xff9b6bf2), juce::Colour (0xfff06abf), juce::Colour (0xffff5a4e),
    };
    return colours[(size_t) juce::jlimit (0, 7, band)];
}

//==============================================================================
EQEditor::EQEditor (BuiltinEQ& e, juce::Colour a, std::function<void()> cb)
    : eq (e), accent (a), onChange (std::move (cb))
{
    enable.setToggleState (eq.enabled.load(), juce::dontSendNotification);
    enable.onClick = [this]
    {
        eq.enabled = enable.getToggleState();
        repaint();
        if (onChange) onChange();
    };
    addAndMakeVisible (enable);

    resetButton.setTooltip ("Remove all bands");
    resetButton.onClick = [this] { eq.removeAllBands(); selectBand (-1); changed(); };
    addAndMakeVisible (resetButton);

    for (int t = 0; t <= (int) BandType::Notch; ++t)
        typeBox.addItem (BuiltinEQ::typeName ((BandType) t), t + 1);
    typeBox.onChange = [this]
    {
        if (selected < 0) return;
        auto s = eq.getBand (selected);
        s.type = (BandType) (typeBox.getSelectedId() - 1);
        if (! BuiltinEQ::typeHasGain (s.type)) s.gain = 0.0f;
        eq.setBand (selected, s);
        changed();
    };
    addAndMakeVisible (typeBox);

    auto setupKnob = [this] (juce::Slider& k, double mn, double mx, double mid, std::function<juce::String (double)> fmt)
    {
        k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
        k.setRange (mn, mx, 0.0);
        if (mid > mn && mid < mx) k.setSkewFactorFromMidPoint (mid);
        k.setColour (juce::Slider::rotarySliderFillColourId, accent);
        k.setColour (juce::Slider::textBoxTextColourId, Theme::text);
        k.textFromValueFunction = std::move (fmt);
        k.setTextBoxIsEditable (true);
        k.updateText();
        addAndMakeVisible (k);
    };

    setupKnob (freqSlider, BuiltinEQ::minFreq, BuiltinEQ::maxFreq, 1000.0, [] (double v) { return formatFreq (v); });
    freqSlider.valueFromTextFunction = [] (const juce::String& t)
    {
        auto s = t.trim().toLowerCase();
        double v = s.getDoubleValue();
        if (s.contains ("k")) v *= 1000.0;
        return v;
    };
    setupKnob (gainSlider, BuiltinEQ::minGain, BuiltinEQ::maxGain, 0.0, [] (double v) { return formatGain (v); });
    gainSlider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
    setupKnob (qSlider, BuiltinEQ::minQ, BuiltinEQ::maxQ, 1.0, [] (double v) { return "Q " + juce::String (v, 2); });
    qSlider.valueFromTextFunction = [] (const juce::String& t) { return t.retainCharacters ("0123456789.").getDoubleValue(); };

    for (auto* k : { &freqSlider, &gainSlider, &qSlider })
        k->updateText();   // apply the custom formatting to the initial text

    freqSlider.setDoubleClickReturnValue (true, 1000.0);
    gainSlider.setDoubleClickReturnValue (true, 0.0);
    qSlider.setDoubleClickReturnValue (true, 1.0);

    auto knobChanged = [this]
    {
        if (selected < 0) return;
        auto s = eq.getBand (selected);
        s.freq = (float) freqSlider.getValue();
        s.gain = BuiltinEQ::typeHasGain (s.type) ? (float) gainSlider.getValue() : 0.0f;
        s.q = (float) qSlider.getValue();
        eq.setBand (selected, s);
        changed();
    };
    freqSlider.onValueChange = knobChanged;
    gainSlider.onValueChange = knobChanged;
    qSlider.onValueChange = knobChanged;

    deleteButton.onClick = [this]
    {
        if (selected < 0) return;
        eq.removeBand (selected);
        selectBand (-1);
        changed();
    };
    addAndMakeVisible (deleteButton);

    hint.setText ("Click on the line (or double-click anywhere) to add a band, then drag it. Shift = fine adjust, wheel = Q, double-click a band to delete it, right-click for its type.",
                  juce::dontSendNotification);
    hint.setFont (Theme::uiFont (12.5f));
    hint.setColour (juce::Label::textColourId, Theme::textDim);
    hint.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (hint);

    // select the first existing band, if any
    for (int i = 0; i < BuiltinEQ::kMaxBands; ++i)
        if (eq.getBand (i).active) { selected = i; break; }

    setSize (700, 500);
    updateControls();
}

//==============================================================================
void EQEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto top = r.removeFromTop (28);
    resetButton.setBounds (top.removeFromRight (70).reduced (0, 2));
    top.removeFromRight (8);
    enable.setBounds (top.removeFromRight (100));
    r.removeFromTop (8);

    controlsArea = r.removeFromBottom (108);
    r.removeFromBottom (10);
    graphArea = r;
    plot = graphArea.toFloat().reduced (36.0f, 14.0f).withTrimmedBottom (8.0f);

    auto c = controlsArea.reduced (12, 8);
    hint.setBounds (c);
    auto typeArea = c.removeFromLeft (150);
    typeBox.setBounds (typeArea.withSizeKeepingCentre (140, 28).translated (0, 6));
    c.removeFromLeft (16);
    deleteButton.setBounds (c.removeFromRight (110).withSizeKeepingCentre (110, 28).translated (0, 6));
    c.removeFromRight (16);
    const int w = c.getWidth() / 3;
    for (auto* k : { &freqSlider, &gainSlider, &qSlider })
        k->setBounds (c.removeFromLeft (w).withTrimmedTop (16).withSizeKeepingCentre (juce::jmin (w, 100), 76));
}

//==============================================================================
float EQEditor::freqToX (float f) const
{
    return plot.getX() + plot.getWidth() * (std::log (f / BuiltinEQ::minFreq) / std::log (BuiltinEQ::maxFreq / BuiltinEQ::minFreq));
}

float EQEditor::xToFreq (float x) const
{
    const float t = (x - plot.getX()) / plot.getWidth();
    return juce::jlimit (BuiltinEQ::minFreq, BuiltinEQ::maxFreq, BuiltinEQ::minFreq * std::pow (BuiltinEQ::maxFreq / BuiltinEQ::minFreq, t));
}

float EQEditor::dbToY (float db) const
{
    return plot.getCentreY() - db / kDisplayDb * plot.getHeight() * 0.5f;
}

float EQEditor::yToDb (float y) const
{
    return (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * kDisplayDb;
}

juce::Point<float> EQEditor::handlePosition (int band) const
{
    auto s = eq.getBand (band);
    const float y = BuiltinEQ::typeHasGain (s.type) ? dbToY (juce::jlimit (-kDisplayDb, kDisplayDb, s.gain)) : dbToY (0.0f);
    return { freqToX (s.freq), y };
}

int EQEditor::bandAt (juce::Point<float> p) const
{
    int best = -1;
    float bestDist = 12.0f;
    for (int i = 0; i < BuiltinEQ::kMaxBands; ++i)
    {
        if (! eq.getBand (i).active) continue;
        const float d = handlePosition (i).getDistanceFrom (p);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

bool EQEditor::isNearCurve (juce::Point<float> p) const
{
    if (! plot.contains (p)) return false;
    const float db = juce::jlimit (-kDisplayDb, kDisplayDb, eq.getMagnitudeDb (xToFreq (p.x)));
    return std::abs (dbToY (db) - p.y) < 10.0f;
}

//==============================================================================
void EQEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (14);
    auto top = r.removeFromTop (28);
    g.setColour (Theme::text);
    g.setFont (Theme::uiFont (17.0f, 700));
    g.drawText ("Parametric EQ", top, juce::Justification::centredLeft);
    g.setColour (Theme::textDim);
    g.setFont (Theme::uiFont (12.5f));
    const int n = eq.getNumActiveBands();
    g.drawText (juce::String (n) + " / " + juce::String (BuiltinEQ::kMaxBands) + " bands", top.withTrimmedLeft (140), juce::Justification::centredLeft);

    // --- graph background
    auto ga = graphArea.toFloat();
    g.setColour (Theme::gridBg);
    g.fillRoundedRectangle (ga, 10.0f);

    g.setFont (Theme::uiFont (10.5f, 600));
    for (float db : { -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f })
    {
        const float y = dbToY (db);
        g.setColour (db == 0.0f ? Theme::gridDim.brighter (0.3f) : Theme::gridLine);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (Theme::gridDim.brighter (0.4f));
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (ga.getX() + 2, y - 7, 30, 14), juce::Justification::centredRight);
    }
    for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        const float x = freqToX (f);
        g.setColour (Theme::gridLine);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (Theme::gridDim.brighter (0.4f));
        const auto label = f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f);
        g.drawText (label, juce::Rectangle<float> (x - 20, plot.getBottom() + 2, 40, 14), juce::Justification::centred);
    }
    for (float f : { 30.0f, 40.0f, 60.0f, 70.0f, 80.0f, 90.0f, 300.0f, 400.0f, 600.0f, 700.0f, 800.0f, 900.0f, 3000.0f, 4000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f })
    {
        g.setColour (Theme::gridLine.withAlpha (0.45f));
        g.drawVerticalLine ((int) freqToX (f), plot.getY(), plot.getBottom());
    }

    const bool on = eq.enabled.load();
    updateCurveCache();

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (plot.toNearestInt().expanded (0, 2));

        // individual bands (soft fills)
        for (int i = 0; i < BuiltinEQ::kMaxBands; ++i)
        {
            if (! cache.key[(size_t) i].active) continue;
            const auto& p = cache.bandPaths[(size_t) i];
            juce::Path fill (p);
            fill.lineTo (plot.getRight(), dbToY (0.0f));
            fill.lineTo (plot.getX(), dbToY (0.0f));
            fill.closeSubPath();
            const auto c = bandColour (i);
            g.setColour (c.withAlpha (i == selected ? 0.22f : 0.10f));
            g.fillPath (fill);
            if (i == selected)
            {
                g.setColour (c.withAlpha (0.7f));
                g.strokePath (p, juce::PathStrokeType (1.2f));
            }
        }

        // total response
        const auto& total = cache.total;
        g.setColour ((on ? accent : Theme::gridDim).withAlpha (0.25f));
        g.strokePath (total, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (on ? accent.brighter (0.25f) : Theme::gridDim.brighter (0.3f));
        g.strokePath (total, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // hover marker for "click on the line to add"
    if (hoverInPlot && hovered < 0 && dragging < 0 && nearCurve && n < BuiltinEQ::kMaxBands)
    {
        const juce::Point<float> pt = markerPos;
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawEllipse (juce::Rectangle<float> (12, 12).withCentre (pt), 1.5f);
        g.drawLine (pt.x - 3, pt.y, pt.x + 3, pt.y, 1.5f);
        g.drawLine (pt.x, pt.y - 3, pt.x, pt.y + 3, 1.5f);
    }

    // handles
    for (int i = 0; i < BuiltinEQ::kMaxBands; ++i)
    {
        if (! eq.getBand (i).active) continue;
        const auto pos = handlePosition (i);
        const auto c = bandColour (i);
        const bool big = i == selected || i == hovered || i == dragging;
        auto circle = juce::Rectangle<float> (big ? 20.0f : 16.0f, big ? 20.0f : 16.0f).withCentre (pos);
        if (big)
        {
            g.setColour (c.withAlpha (0.3f));
            g.fillEllipse (circle.expanded (5.0f));
        }
        g.setColour (c);
        g.fillEllipse (circle);
        g.setColour (i == selected ? juce::Colours::white : Theme::gridBg);
        g.drawEllipse (circle, i == selected ? 2.0f : 1.5f);
        g.setColour (juce::Colours::white);
        g.setFont (Theme::uiFont (10.5f, 700));
        g.drawText (juce::String (i + 1), circle, juce::Justification::centred);
    }

    // readout for the hovered / dragged band
    const int info = dragging >= 0 ? dragging : hovered;
    if (info >= 0)
    {
        auto s = eq.getBand (info);
        juce::String t = BuiltinEQ::typeName (s.type) + "   " + formatFreq (s.freq);
        if (BuiltinEQ::typeHasGain (s.type)) t << "   " << formatGain (s.gain);
        t << "   Q " << juce::String (s.q, 2);
        const auto font = Theme::uiFont (12.0f, 600);
        const float w = juce::GlyphArrangement::getStringWidth (font, t) + 18.0f;
        auto pos = handlePosition (info);
        auto box = juce::Rectangle<float> (w, 24.0f).withCentre ({ pos.x, pos.y - 30.0f });
        if (box.getY() < plot.getY()) box.setY (pos.y + 18.0f);
        box = box.constrainedWithin (ga.reduced (4.0f));
        g.setColour (juce::Colour (0xf0101722));
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (bandColour (info));
        g.drawRoundedRectangle (box, 6.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (font);
        g.drawText (t, box, juce::Justification::centred);
    }

    // --- controls card
    auto ca = controlsArea.toFloat();
    g.setColour (Theme::panelAlt);
    g.fillRoundedRectangle (ca, 10.0f);
    g.setColour (Theme::border);
    g.drawRoundedRectangle (ca.reduced (0.5f), 10.0f, 1.0f);

    if (selected >= 0)
    {
        g.setFont (Theme::uiFont (11.5f, 700));
        g.setColour (bandColour (selected));
        g.fillEllipse (juce::Rectangle<float> (10, 10).withCentre ({ (float) typeBox.getX() + 6, (float) typeBox.getY() - 12 }));
        g.setColour (Theme::textDim);
        g.drawText ("BAND " + juce::String (selected + 1), typeBox.getX() + 16, typeBox.getY() - 20, 120, 16, juce::Justification::centredLeft);
        const char* names[] = { "FREQUENCY", "GAIN", "Q / WIDTH" };
        int i = 0;
        for (auto* k : { &freqSlider, &gainSlider, &qSlider })
            g.drawText (names[i++], k->getX() - 20, k->getY() - 16, k->getWidth() + 40, 14, juce::Justification::centred);
    }
}

//==============================================================================
void EQEditor::updateCurveCache()
{
    std::array<BuiltinEQ::BandSettings, BuiltinEQ::kMaxBands> key;
    for (int i = 0; i < BuiltinEQ::kMaxBands; ++i)
        key[(size_t) i] = eq.getBand (i);

    auto same = [] (const BuiltinEQ::BandSettings& a, const BuiltinEQ::BandSettings& b)
    {
        return a.active == b.active && (! a.active || (a.type == b.type && a.freq == b.freq && a.gain == b.gain && a.q == b.q));
    };

    bool upToDate = cache.valid && cache.plot == plot && cache.sampleRate == eq.getSampleRate();
    for (int i = 0; upToDate && i < BuiltinEQ::kMaxBands; ++i)
        upToDate = same (key[(size_t) i], cache.key[(size_t) i]);
    if (upToDate) return;

    cache.valid = true;
    cache.plot = plot;
    cache.sampleRate = eq.getSampleRate();
    cache.key = key;

    // coefficients once per band, then one pass over the x axis for all curves
    const double sr = cache.sampleRate > 0 ? cache.sampleRate : 44100.0;
    std::array<BuiltinEQ::Coeffs, BuiltinEQ::kMaxBands> coeffs;
    for (int i = 0; i < BuiltinEQ::kMaxBands; ++i)
    {
        cache.bandPaths[(size_t) i].clear();
        if (key[(size_t) i].active)
            coeffs[(size_t) i] = BuiltinEQ::makeCoeffs (key[(size_t) i].type, sr, key[(size_t) i].freq, key[(size_t) i].gain, key[(size_t) i].q);
    }
    cache.total.clear();

    auto toY = [this] (double mag)
    {
        const float db = juce::jlimit (-kDisplayDb - 2.0f, kDisplayDb + 2.0f, juce::Decibels::gainToDecibels ((float) mag, -100.0f));
        return dbToY (db);
    };

    const int steps = juce::jmax (100, (int) plot.getWidth() / 2);
    for (int s = 0; s <= steps; ++s)
    {
        const float x = plot.getX() + plot.getWidth() * (float) s / (float) steps;
        const double f = xToFreq (x);
        double total = 1.0;
        for (int i = 0; i < BuiltinEQ::kMaxBands; ++i)
        {
            if (! key[(size_t) i].active) continue;
            const double m = BuiltinEQ::magnitudeOf (coeffs[(size_t) i], f, sr);
            total *= m;
            auto& p = cache.bandPaths[(size_t) i];
            if (s == 0) p.startNewSubPath (x, toY (m)); else p.lineTo (x, toY (m));
        }
        if (s == 0) cache.total.startNewSubPath (x, toY (total)); else cache.total.lineTo (x, toY (total));
    }
}

void EQEditor::selectBand (int band)
{
    selected = band;
    updateControls();
    repaint();
}

void EQEditor::updateControls()
{
    const bool has = selected >= 0 && eq.getBand (selected).active;
    if (! has) selected = -1;

    for (juce::Component* c : { (juce::Component*) &typeBox, (juce::Component*) &freqSlider, (juce::Component*) &gainSlider,
                                (juce::Component*) &qSlider, (juce::Component*) &deleteButton })
        c->setVisible (has);
    hint.setVisible (! has);

    if (has)
    {
        auto s = eq.getBand (selected);
        typeBox.setSelectedId ((int) s.type + 1, juce::dontSendNotification);
        freqSlider.setValue (s.freq, juce::dontSendNotification);
        gainSlider.setValue (s.gain, juce::dontSendNotification);
        gainSlider.setEnabled (BuiltinEQ::typeHasGain (s.type));
        qSlider.setValue (s.q, juce::dontSendNotification);
        freqSlider.setColour (juce::Slider::rotarySliderFillColourId, bandColour (selected));
        gainSlider.setColour (juce::Slider::rotarySliderFillColourId, bandColour (selected));
        qSlider.setColour (juce::Slider::rotarySliderFillColourId, bandColour (selected));
    }
}

void EQEditor::enableIfEdited()
{
    if (eq.getNumActiveBands() > 0 && ! eq.enabled.load())
    {
        eq.enabled = true;
        enable.setToggleState (true, juce::dontSendNotification);
    }
}

void EQEditor::changed()
{
    enableIfEdited();
    updateControls();
    repaint();
    if (onChange) onChange();
}

void EQEditor::addBandAt (juce::Point<float> p)
{
    const float f = xToFreq (p.x);
    const int b = eq.addBand (BandType::Bell, f, juce::jlimit (BuiltinEQ::minGain, BuiltinEQ::maxGain, yToDb (p.y)), 1.0f);
    if (b >= 0)
    {
        selectBand (b);
        changed();
    }
}

void EQEditor::showBandMenu (int band)
{
    auto s = eq.getBand (band);
    juce::PopupMenu m;
    m.addSectionHeader ("Band " + juce::String (band + 1));
    for (int t = 0; t <= (int) BandType::Notch; ++t)
        m.addItem (t + 1, BuiltinEQ::typeName ((BandType) t), true, (int) s.type == t);
    m.addSeparator();
    m.addItem (100, "Reset gain to 0 dB", BuiltinEQ::typeHasGain (s.type));
    m.addItem (101, "Reset Q");
    m.addItem (102, "Delete band");

    juce::Component::SafePointer<EQEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, band] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& theEq = safe->eq;
        auto st = theEq.getBand (band);
        if (r >= 1 && r <= 6)
        {
            st.type = (BandType) (r - 1);
            if (! BuiltinEQ::typeHasGain (st.type)) st.gain = 0.0f;
            theEq.setBand (band, st);
        }
        else if (r == 100) { st.gain = 0.0f; theEq.setBand (band, st); }
        else if (r == 101) { st.q = BuiltinEQ::typeHasGain (st.type) ? 1.0f : 0.7071f; theEq.setBand (band, st); }
        else if (r == 102) { theEq.removeBand (band); safe->selected = -1; }
        safe->changed();
    });
}

//==============================================================================
void EQEditor::mouseMove (const juce::MouseEvent& e)
{
    hoverPos = e.position;
    hoverInPlot = plot.contains (e.position);
    const int h = bandAt (e.position);

    const bool near = h < 0 && isNearCurve (e.position);
    juce::Point<float> marker;
    if (near)
        marker = { e.position.x, dbToY (juce::jlimit (-kDisplayDb, kDisplayDb, eq.getMagnitudeDb (xToFreq (e.position.x)))) };

    if (h != hovered)
    {
        hovered = h;
        repaint (graphArea);   // the readout box appears / disappears
    }
    else if (near != nearCurve || (near && marker != markerPos))
    {
        // only the small "+" marker moves - repaint just around its old and new position
        if (nearCurve) repaint (juce::Rectangle<float> (24, 24).withCentre (markerPos).getSmallestIntegerContainer());
        if (near)      repaint (juce::Rectangle<float> (24, 24).withCentre (marker).getSmallestIntegerContainer());
    }
    nearCurve = near;
    markerPos = marker;

    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor
                           : (near ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor));
}

void EQEditor::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    hoverInPlot = false;
    nearCurve = false;
    repaint (graphArea);
}

void EQEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! graphArea.contains (e.getPosition())) return;

    int b = bandAt (e.position);
    if (e.mods.isPopupMenu())
    {
        if (b >= 0) { selectBand (b); showBandMenu (b); }
        return;
    }

    // clicking on the line inserts a new band at that frequency and starts dragging it. It starts at
    // 0 dB so the curve (and the sound) does not change until the band is dragged.
    if (b < 0 && isNearCurve (e.position) && eq.getNumActiveBands() < BuiltinEQ::kMaxBands)
    {
        b = eq.addBand (BandType::Bell, xToFreq (e.position.x), 0.0f, 1.0f);
        if (b >= 0)
        {
            justAdded = b;
            justAddedTime = juce::Time::getMillisecondCounter();
            changed();
        }
    }

    if (b >= 0)
    {
        dragging = b;
        dragStartMouse = e.position;
        dragStartBand = eq.getBand (b);
        selectBand (b);
    }
}

void EQEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    if (! eq.getBand (dragging).active) { dragging = -1; return; }   // deleted by a double-click

    const float scale = e.mods.isShiftDown() ? 0.1f : 1.0f;   // Shift = fine adjustment
    const auto delta = (e.position - dragStartMouse) * scale;
    auto s = dragStartBand;

    s.freq = xToFreq (freqToX (dragStartBand.freq) + delta.x);
    if (BuiltinEQ::typeHasGain (s.type))
        s.gain = juce::jlimit (BuiltinEQ::minGain, BuiltinEQ::maxGain, dragStartBand.gain - delta.y / (plot.getHeight() * 0.5f) * kDisplayDb);
    else
        s.q = juce::jlimit (BuiltinEQ::minQ, BuiltinEQ::maxQ, dragStartBand.q * std::pow (2.0f, -delta.y / 60.0f));   // cuts / notch: up = narrower

    eq.setBand (dragging, s);
    hoverPos = e.position;
    enableIfEdited();
    updateControls();
    repaint();
    if (onChange) onChange();
}

void EQEditor::mouseUp (const juce::MouseEvent&)
{
    dragging = -1;
    repaint();
}

void EQEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! plot.expanded (10.0f).contains (e.position)) return;

    const int b = bandAt (e.position);
    dragging = -1;   // the second mouseDown of the double-click started a drag - cancel it
    hovered = -1;
    if (b >= 0 && b == justAdded && juce::Time::getMillisecondCounter() - justAddedTime < 800)
        return;   // this double-click created the band on the line - keep it
    if (b >= 0)
    {
        eq.removeBand (b);
        selectBand (-1);
        changed();
        return;
    }
    addBandAt (e.position);
}

void EQEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int b = bandAt (e.position);
    if (b < 0) b = selected;
    if (b < 0 || ! graphArea.contains (e.getPosition())) return;

    auto s = eq.getBand (b);
    const float step = e.mods.isShiftDown() ? 0.02f : 0.12f;
    s.q = juce::jlimit (BuiltinEQ::minQ, BuiltinEQ::maxQ, s.q * std::pow (2.0f, (w.deltaY >= 0 ? 1.0f : -1.0f) * step * (w.isReversed ? -1.0f : 1.0f)));
    eq.setBand (b, s);
    enableIfEdited();
    selectBand (b);
    if (onChange) onChange();
}

} // namespace artilody
