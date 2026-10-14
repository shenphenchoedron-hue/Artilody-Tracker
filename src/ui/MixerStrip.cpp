#include "MixerStrip.h"
#include "../PluginHost.h"

namespace artilody
{

//==============================================================================
void LevelMeter::setLevels (float l, float r)
{
    auto toPos = [] (float v) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (v, -60.0f) + 60.0f) / 66.0f); };
    const float nl = toPos (l), nr = toPos (r);
    levelL = juce::jmax (nl, levelL - 0.025f);
    levelR = juce::jmax (nr, levelR - 0.025f);
    holdL = juce::jmax (levelL, holdL - 0.004f);
    holdR = juce::jmax (levelR, holdR - 0.004f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto drawBar = [&] (juce::Rectangle<float> b, float level, float hold)
    {
        g.setColour (juce::Colour (0xffe6ebf2));
        g.fillRoundedRectangle (b, 1.5f);
        if (horizontal)
        {
            juce::ColourGradient grad (juce::Colour (0xff1fc4b4), b.getX(), 0, juce::Colour (0xffff4d5e), b.getRight(), 0, false);
            grad.addColour (0.7, juce::Colour (0xffb8e04a));
            grad.addColour (0.85, juce::Colour (0xffffc23d));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (b.withWidth (b.getWidth() * level), 1.5f);
            g.setColour (Theme::text.withAlpha (0.5f));
            g.fillRect (b.getX() + b.getWidth() * hold - 1.0f, b.getY(), 1.5f, b.getHeight());
        }
        else
        {
            juce::ColourGradient grad (juce::Colour (0xff1fc4b4), 0, b.getBottom(), juce::Colour (0xffff4d5e), 0, b.getY(), false);
            grad.addColour (0.7, juce::Colour (0xffb8e04a));
            grad.addColour (0.85, juce::Colour (0xffffc23d));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (b.withTop (b.getBottom() - b.getHeight() * level), 1.5f);
            g.setColour (Theme::text.withAlpha (0.5f));
            g.fillRect (b.getX(), b.getBottom() - b.getHeight() * hold, b.getWidth(), 1.5f);
        }
    };

    if (horizontal)
    {
        auto top = r.removeFromTop (r.getHeight() * 0.5f).reduced (0, 0.5f);
        drawBar (top, levelL, holdL);
        drawBar (r.reduced (0, 0.5f), levelR, holdR);
    }
    else
    {
        auto left = r.removeFromLeft (r.getWidth() * 0.5f).reduced (0.5f, 0);
        drawBar (left, levelL, holdL);
        drawBar (r.reduced (0.5f, 0), levelR, holdR);
    }
}

//==============================================================================
BuiltinEditor::BuiltinEditor (BuiltinEffect& e, juce::Colour a) : fx (e), accent (a)
{
    enable.setToggleState (fx.enabled.load(), juce::dontSendNotification);
    enable.onClick = [this] { fx.enabled = enable.getToggleState(); };
    addAndMakeVisible (enable);

    resetButton.onClick = [this]
    {
        for (int i = 0; i < fx.numParams(); ++i)
        {
            fx.set (i, fx.params[(size_t) i]->defaultValue);
            knobs[i]->setValue (fx.get (i), juce::dontSendNotification);
        }
        repaint();
    };
    addAndMakeVisible (resetButton);

    for (int i = 0; i < fx.numParams(); ++i)
    {
        auto& p = *fx.params[(size_t) i];
        auto* k = knobs.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow));
        k->setRange (p.minValue, p.maxValue, 0.0);
        if (p.skewCentre > p.minValue && p.skewCentre < p.maxValue && ! (p.minValue < 0 && p.maxValue > 0))
            k->setSkewFactorFromMidPoint (p.skewCentre);
        k->setValue (p.value.load(), juce::dontSendNotification);
        k->setDoubleClickReturnValue (true, p.defaultValue);
        k->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        k->setColour (juce::Slider::rotarySliderFillColourId, accent);
        k->setColour (juce::Slider::textBoxTextColourId, Theme::textDim);
        const auto suffix = p.suffix;
        k->textFromValueFunction = [suffix] (double v)
        {
            if (suffix == "Hz") return v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
            if (suffix.isEmpty()) return juce::String (v, 2);
            if (suffix == ":1") return juce::String (v, 1) + ":1";
            return juce::String (v, 1) + " " + suffix;
        };
        k->onValueChange = [this, i, k]
        {
            fx.set (i, (float) k->getValue());
            if (! fx.enabled.load())
            {
                fx.enabled = true;
                enable.setToggleState (true, juce::dontSendNotification);
            }
            repaint();
        };
        k->updateText();
        addAndMakeVisible (k);
    }

    const int cols = 4;
    const int rows = (fx.numParams() + cols - 1) / cols;
    const bool hasCurve = dynamic_cast<BuiltinEQ*> (&fx) != nullptr;
    setSize (cols * 86 + 24, 50 + rows * 92 + (hasCurve ? 120 : 0) + (dynamic_cast<BuiltinCompressor*> (&fx) != nullptr ? 26 : 0));
    startTimerHz (20);
}

void BuiltinEditor::timerCallback()
{
    if (dynamic_cast<BuiltinCompressor*> (&fx) != nullptr)
        repaint();
}

void BuiltinEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (28);
    resetButton.setBounds (top.removeFromRight (64).reduced (0, 2));
    enable.setBounds (top.removeFromRight (110));
    r.removeFromTop (6);

    if (dynamic_cast<BuiltinEQ*> (&fx) != nullptr)
    {
        curveArea = r.removeFromTop (110);
        r.removeFromTop (8);
    }
    else
    {
        curveArea = {};
    }
    if (dynamic_cast<BuiltinCompressor*> (&fx) != nullptr)
    {
        curveArea = r.removeFromTop (20);
        r.removeFromTop (6);
    }

    const int cols = 4;
    for (int i = 0; i < knobs.size(); ++i)
    {
        const int c = i % cols, row = i / cols;
        knobs[i]->setBounds (r.getX() + c * 86, r.getY() + row * 92 + 16, 80, 72);
    }
}

void BuiltinEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (12);
    g.setColour (Theme::text);
    g.setFont (Theme::uiFont (16.0f, 700));
    g.drawText (fx.name, r.removeFromTop (28), juce::Justification::centredLeft);

    g.setFont (Theme::uiFont (11.5f, 600));
    g.setColour (Theme::textDim);
    for (int i = 0; i < knobs.size(); ++i)
    {
        auto kb = knobs[i]->getBounds();
        g.drawText (fx.params[(size_t) i]->name, kb.getX(), kb.getY() - 16, kb.getWidth(), 14, juce::Justification::centred);
    }

    if (auto* eq = dynamic_cast<BuiltinEQ*> (&fx); eq != nullptr && ! curveArea.isEmpty())
    {
        auto c = curveArea.toFloat();
        g.setColour (Theme::gridBg);
        g.fillRoundedRectangle (c, 8.0f);
        g.setColour (Theme::gridLine);
        for (float db : { -12.0f, 0.0f, 12.0f })
        {
            const float y = c.getCentreY() - db / 18.0f * c.getHeight() * 0.45f;
            g.drawHorizontalLine ((int) y, c.getX() + 4, c.getRight() - 4);
        }
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = c.getX() + c.getWidth() * (std::log10 (f / 20.0f) / 3.0f);
            g.drawVerticalLine ((int) x, c.getY() + 4, c.getBottom() - 4);
        }
        juce::Path p;
        for (int i = 0; i <= 120; ++i)
        {
            const float t = (float) i / 120.0f;
            const float f = 20.0f * std::pow (1000.0f, t);
            const float db = juce::jlimit (-18.0f, 18.0f, eq->getMagnitudeDb (f));
            const float x = c.getX() + c.getWidth() * t;
            const float y = c.getCentreY() - db / 18.0f * c.getHeight() * 0.45f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (fx.enabled.load() ? accent : Theme::gridDim);
        g.strokePath (p, juce::PathStrokeType (2.0f));
    }

    if (auto* comp = dynamic_cast<BuiltinCompressor*> (&fx); comp != nullptr && ! curveArea.isEmpty())
    {
        auto c = curveArea.toFloat();
        g.setColour (juce::Colour (0xffe6ebf2));
        g.fillRoundedRectangle (c, 4.0f);
        const float gr = juce::jlimit (0.0f, 24.0f, comp->gainReductionDb.load());
        g.setColour (accent);
        g.fillRoundedRectangle (c.withWidth (c.getWidth() * gr / 24.0f), 4.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::uiFont (11.5f, 600));
        g.drawText ("Gain reduction  " + juce::String (-gr, 1) + " dB", c.reduced (6, 0), juce::Justification::centredLeft);
    }
}

//==============================================================================
class MixerStrip::InstrumentButton final : public juce::Component
{
public:
    InstrumentButton (MixerStrip& o) : owner (o) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (isMouseOver())
        {
            g.setColour (Theme::accentBlue.withAlpha (0.06f));
            g.fillRoundedRectangle (r, 7.0f);
        }
        auto icon = r.removeFromLeft (r.getHeight()).reduced (3.0f);
        auto& s = owner.strip;

        juce::String name, type;
        if (s.isMaster)
        {
            name = "Main Out";
            auto* dev = owner.ctx.engine.deviceManager.getCurrentAudioDevice();
            type = dev != nullptr ? dev->getName() : juce::String ("No device");
            g.setColour (Theme::text);
            g.fillRoundedRectangle (icon, 5.0f);
            Theme::drawIcon (g, Theme::Icon::Mixer, icon.reduced (5.0f), juce::Colours::white);
        }
        else
        {
            name = s.getInstrumentName();
            type = s.getInstrumentTypeName();
            if (s.getInstrument() == nullptr)
            {
                g.setColour (Theme::panelAlt);
                g.fillRoundedRectangle (icon, 5.0f);
                g.setColour (Theme::border);
                g.drawRoundedRectangle (icon, 5.0f, 1.0f);
                Theme::drawIcon (g, Theme::Icon::Plus, icon.reduced (6.0f), Theme::textDim);
            }
            else
            {
                Theme::drawInstrumentIcon (g, icon, name, owner.colour());
            }
        }

        auto chevron = r.removeFromRight (16.0f);
        Theme::drawIcon (g, Theme::Icon::Chevron, chevron.withSizeKeepingCentre (14, 14), Theme::text);

        r.removeFromLeft (6.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::uiFont (13.0f, 700));
        g.drawFittedText (name, r.removeFromTop (r.getHeight() * 0.52f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.8f);
        g.setColour (Theme::textDim);
        g.setFont (Theme::uiFont (11.0f, 400));
        g.drawFittedText (type, r.toNearestInt(), juce::Justification::topLeft, 1, 0.8f);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasClicked()) return;
        if (owner.strip.isMaster)
        {
            if (owner.ctx.onShowAudioSettings) owner.ctx.onShowAudioSettings();
            return;
        }
        if (e.getNumberOfClicks() > 1 || (e.mods.isAltDown()))
        {
            if (auto* p = owner.strip.getInstrumentPlugin())
            {
                PluginHost::get().showEditor (p, owner.title() + " - " + p->getName());
                return;
            }
        }
        owner.showInstrumentMenu();
    }

private:
    MixerStrip& owner;
};

//==============================================================================
class MixerStrip::FxPanel final : public juce::Component
{
public:
    static constexpr int rowH = 22;

    FxPanel (MixerStrip& o) : owner (o)
    {
        collapseButton.drawBackground = false;
        collapseButton.iconColour = Theme::textDim;
        collapseButton.onClick = [this]
        {
            owner.strip.fxCollapsed = ! owner.strip.fxCollapsed;
            owner.ctx.state.dirty = true;
            rebuild();
        };
        addAndMakeVisible (collapseButton);

        addButton.drawBackground = false;
        addButton.iconColour = Theme::textDim;
        addButton.setTooltip ("Add VST3 / AU / CLAP effect");
        addButton.onClick = [this] { owner.showAddFxMenu(); };
        addAndMakeVisible (addButton);
        rebuild();
    }

    struct PluginRow final : public juce::Component
    {
        PluginRow (FxPanel& p, int i) : panel (p), index (i)
        {
            for (auto* b : { &power, &up, &down, &remove })
            {
                b->drawBackground = false;
                b->radius = 4.0f;
                addChildComponent (b);
            }
            power.setClickingTogglesState (false);
            power.onClick = [this]
            {
                if (auto* s = slot()) { s->bypassed = ! s->bypassed.load(); repaint(); }
            };
            power.setVisible (true);
            up.onClick = [this] { panel.owner.strip.moveFx (index, index - 1); panel.rebuild(); };
            down.onClick = [this] { panel.owner.strip.moveFx (index, index + 1); panel.rebuild(); };
            remove.onClick = [this] { panel.removeFx (index); };
            up.setTooltip ("Move up");
            down.setTooltip ("Move down");
            remove.setTooltip ("Remove");
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        PluginSlot* slot() const { return panel.owner.strip.getFx (index); }

        void resized() override
        {
            auto r = getLocalBounds();
            power.setBounds (r.removeFromLeft (18).reduced (1));
            remove.setBounds (r.removeFromRight (15).reduced (0, 3));
            down.setBounds (r.removeFromRight (15).reduced (0, 3));
            up.setBounds (r.removeFromRight (15).reduced (0, 3));
        }

        void paint (juce::Graphics& g) override
        {
            auto* s = slot();
            if (s == nullptr) return;
            const bool on = ! s->bypassed.load();
            auto r = getLocalBounds().toFloat();
            g.setColour (isMouseOver (true) ? Theme::selection : Theme::panelAlt);
            g.fillRoundedRectangle (r.reduced (0, 1), 5.0f);
            power.iconColour = on ? panel.owner.colour() : Theme::textFaint;
            power.icon = Theme::Icon::Play;
            g.setColour (on ? Theme::text : Theme::textFaint);
            g.setFont (Theme::uiFont (12.0f, 400));
            auto tr = getLocalBounds().withTrimmedLeft (19).withTrimmedRight (isMouseOver (true) ? 46 : 2);
            g.drawFittedText (s->plugin->getName(), tr, juce::Justification::centredLeft, 1, 0.85f);
        }

        void mouseEnter (const juce::MouseEvent&) override { showButtons (true); }
        void mouseExit (const juce::MouseEvent&) override  { showButtons (isMouseOver (true)); }
        void showButtons (bool b)
        {
            up.setVisible (b && index > 0);
            down.setVisible (b && index < panel.owner.strip.getNumFx() - 1);
            remove.setVisible (b);
            repaint();
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mouseWasClicked()) return;
            auto* s = slot();
            if (s == nullptr) return;
            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu m;
                m.addItem (1, "Open editor");
                m.addItem (2, "Bypass", true, s->bypassed.load());
                m.addItem (3, "Move up", index > 0);
                m.addItem (4, "Move down", index < panel.owner.strip.getNumFx() - 1);
                m.addSeparator();
                m.addItem (5, "Remove");
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe = juce::Component::SafePointer<PluginRow> (this)] (int r)
                {
                    if (safe == nullptr) return;
                    auto& pnl = safe->panel;
                    const int i = safe->index;
                    if (r == 1) pnl.openEditor (i);
                    else if (r == 2) { if (auto* sl = safe->slot()) sl->bypassed = ! sl->bypassed.load(); safe->repaint(); }
                    else if (r == 3) { pnl.owner.strip.moveFx (i, i - 1); pnl.rebuild(); }
                    else if (r == 4) { pnl.owner.strip.moveFx (i, i + 1); pnl.rebuild(); }
                    else if (r == 5) pnl.removeFx (i);
                });
                return;
            }
            panel.openEditor (index);
        }

        FxPanel& panel;
        int index;
        IconButton power { "bypass", Theme::Icon::Play }, up { "up", Theme::Icon::ChevronUp },
                   down { "down", Theme::Icon::ChevronDown }, remove { "remove", Theme::Icon::Close };
    };

    struct BuiltinRow final : public juce::Component
    {
        BuiltinRow (FxPanel& p, int i) : panel (p), index (i)
        {
            power.drawBackground = false;
            power.onClick = [this]
            {
                auto* b = fx();
                b->enabled = ! b->enabled.load();
                panel.owner.ctx.state.dirty = true;
                repaint();
            };
            power.setTooltip ("Enable / disable");
            addAndMakeVisible (power);
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        BuiltinEffect* fx() const { return panel.owner.strip.getBuiltin (index); }

        void resized() override { power.setBounds (getLocalBounds().removeFromLeft (18).reduced (1)); }

        void paint (juce::Graphics& g) override
        {
            auto* b = fx();
            const bool on = b->enabled.load();
            auto r = getLocalBounds().toFloat();
            g.setColour (on ? panel.owner.colour().withAlpha (0.12f) : (isMouseOver() ? Theme::selection : Theme::panelAlt));
            g.fillRoundedRectangle (r.reduced (0, 1), 5.0f);
            power.icon = Theme::Icon::Power;
            power.iconColour = on ? panel.owner.colour().darker (0.2f) : Theme::textFaint;
            g.setColour (on ? Theme::text : Theme::textDim);
            g.setFont (Theme::uiFont (12.0f, on ? 600 : 400));
            const juce::String name = index == 1 ? "Comp" : b->name;
            g.drawFittedText (name, getLocalBounds().withTrimmedLeft (19).withTrimmedRight (2), juce::Justification::centredLeft, 1, 0.85f);
        }

        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { repaint(); }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mouseWasClicked()) return;
            auto content = std::make_unique<BuiltinEditor> (*fx(), panel.owner.colour());
            juce::CallOutBox::launchAsynchronously (std::move (content), getScreenBounds(), nullptr);
            panel.owner.ctx.state.dirty = true;
        }

        FxPanel& panel;
        int index;
        IconButton power { "power", Theme::Icon::Power };
    };

    void openEditor (int index)
    {
        if (auto* s = owner.strip.getFx (index))
            PluginHost::get().showEditor (s->plugin.get(), owner.title() + " - " + s->plugin->getName());
    }

    void removeFx (int index)
    {
        if (auto* s = owner.strip.getFx (index))
        {
            PluginHost::get().closeEditor (s->plugin.get());
            auto removed = owner.strip.removeFx (index);
            removed.reset();
            owner.ctx.state.dirty = true;
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<FxPanel> (this)] { if (safe) safe->rebuild(); });
        }
    }

    void rebuild()
    {
        pluginRows.clear();
        if (! owner.strip.fxCollapsed)
            for (int i = 0; i < owner.strip.getNumFx(); ++i)
                addAndMakeVisible (pluginRows.add (new PluginRow (*this, i)));

        if (builtinRows.isEmpty())
            for (int i = 0; i < 3; ++i)
                addAndMakeVisible (builtinRows.add (new BuiltinRow (*this, i)));

        collapseButton.icon = owner.strip.fxCollapsed ? Theme::Icon::ChevronDown : Theme::Icon::ChevronUp;
        collapseButton.setTooltip (owner.strip.fxCollapsed ? "Expand plug-in list" : "Collapse plug-in list");
        setSize (getWidth() > 0 ? getWidth() : 90, getIdealHeight());
        resized();
        repaint();
    }

    int getIdealHeight() const
    {
        int h = 20; // header
        if (! owner.strip.fxCollapsed)
            h += owner.strip.getNumFx() * rowH;
        h += 6 + 3 * rowH;
        return h;
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto header = r.removeFromTop (20);
        addButton.setBounds (header.removeFromRight (18).reduced (1));
        collapseButton.setBounds (header.removeFromRight (18).reduced (1));
        for (auto* row : pluginRows)
            row->setBounds (r.removeFromTop (rowH));
        r.removeFromTop (6);
        for (auto* row : builtinRows)
            row->setBounds (r.removeFromTop (rowH));
    }

    void paint (juce::Graphics& g) override
    {
        auto header = getLocalBounds().removeFromTop (20);
        g.setColour (Theme::textDim);
        g.setFont (Theme::uiFont (11.0f, 700));
        const int n = owner.strip.getNumFx();
        juce::String t = "PLUG-INS";
        if (n > 0) t << "  " << n;
        g.drawText (t, header.withTrimmedLeft (2), juce::Justification::centredLeft);

        if (! owner.strip.fxCollapsed && n == 0)
        {
            // no rows: nothing
        }
        const int sepY = 20 + (owner.strip.fxCollapsed ? 0 : n * rowH) + 3;
        g.setColour (Theme::border);
        g.drawHorizontalLine (sepY, 2.0f, (float) getWidth() - 2.0f);
    }

    MixerStrip& owner;
    IconButton collapseButton { "collapse", Theme::Icon::ChevronUp }, addButton { "add", Theme::Icon::Plus };
    juce::OwnedArray<PluginRow> pluginRows;
    juce::OwnedArray<BuiltinRow> builtinRows;
};

//==============================================================================
MixerStrip::MixerStrip (AppContext& c, StripProcessor& s, int t) : ctx (c), strip (s), track (t)
{
    menuButton.drawBackground = false;
    menuButton.iconColour = Theme::text;
    menuButton.onClick = [this] { showTrackMenu(); };
    addAndMakeVisible (menuButton);

    instrumentButton = std::make_unique<InstrumentButton> (*this);
    addAndMakeVisible (*instrumentButton);

    fxPanel = std::make_unique<FxPanel> (*this);
    fxViewport.setViewedComponent (fxPanel.get(), false);
    fxViewport.setScrollBarsShown (true, false);
    fxViewport.setScrollBarThickness (6);
    addAndMakeVisible (fxViewport);

    auto setupFader = [this] (juce::Slider& f)
    {
        f.setRange (-60.0, 6.0, 0.1);
        f.setSkewFactorFromMidPoint (-12.0);
        f.setValue (strip.volumeDb.load(), juce::dontSendNotification);
        f.setDoubleClickReturnValue (true, 0.0);
        f.setColour (juce::Slider::thumbColourId, colour());
        f.onValueChange = [this, &f]
        {
            strip.volumeDb = (float) f.getValue();
            if (&f == &fader) compactFader.setValue (f.getValue(), juce::dontSendNotification);
            else fader.setValue (f.getValue(), juce::dontSendNotification);
            ctx.state.dirty = true;
            repaint();
        };
        f.setTooltip ("Volume");
    };
    setupFader (fader);
    setupFader (compactFader);
    addAndMakeVisible (fader);
    addChildComponent (compactFader);

    panKnob.setRange (-1.0, 1.0, 0.01);
    panKnob.setValue (strip.pan.load(), juce::dontSendNotification);
    panKnob.setDoubleClickReturnValue (true, 0.0);
    panKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    panKnob.setColour (juce::Slider::rotarySliderFillColourId, colour());
    panKnob.onValueChange = [this] { strip.pan = (float) panKnob.getValue(); ctx.state.dirty = true; };
    panKnob.setTooltip ("Panning");
    addAndMakeVisible (panKnob);

    addAndMakeVisible (meter);
    compactMeter.horizontal = true;
    addChildComponent (compactMeter);

    for (auto* b : { &muteButton, &soloButton })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffff8a3d));
    soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffffc23d));
    soloButton.setColour (juce::TextButton::textColourOnId, Theme::text);
    muteButton.setTooltip ("Mute");
    soloButton.setTooltip ("Solo");
    muteButton.onClick = [this] { strip.mute = muteButton.getToggleState(); ctx.state.dirty = true; };
    soloButton.onClick = [this] { strip.solo = soloButton.getToggleState(); ctx.state.dirty = true; };
    if (strip.isMaster)
        soloButton.setVisible (false);

    refresh();
    startTimerHz (30);
}

MixerStrip::~MixerStrip()
{
    stopTimer();
    fxViewport.setViewedComponent (nullptr, false);
}

juce::Colour MixerStrip::colour() const
{
    if (strip.isMaster || ! juce::isPositiveAndBelow (track, ctx.engine.song.numTracks()))
        return Theme::accentBlue;
    return ctx.engine.song.tracks[(size_t) track].colour;
}

juce::String MixerStrip::title() const
{
    if (strip.isMaster || ! juce::isPositiveAndBelow (track, ctx.engine.song.numTracks()))
        return "Master";
    return ctx.engine.song.tracks[(size_t) track].name;
}

void MixerStrip::refresh()
{
    fader.setValue (strip.volumeDb.load(), juce::dontSendNotification);
    compactFader.setValue (strip.volumeDb.load(), juce::dontSendNotification);
    panKnob.setValue (strip.pan.load(), juce::dontSendNotification);
    muteButton.setToggleState (strip.mute.load(), juce::dontSendNotification);
    soloButton.setToggleState (strip.solo.load(), juce::dontSendNotification);
    fader.setColour (juce::Slider::thumbColourId, colour());
    compactFader.setColour (juce::Slider::thumbColourId, colour());
    panKnob.setColour (juce::Slider::rotarySliderFillColourId, colour());
    fxPanel->rebuild();
    instrumentButton->repaint();
    resized();
    repaint();
}

void MixerStrip::setCompact (bool c)
{
    if (compact == c) return;
    compact = c;
    for (auto* comp : std::initializer_list<juce::Component*> { instrumentButton.get(), &fxViewport, &fader, &panKnob, &meter })
        comp->setVisible (! compact);
    compactFader.setVisible (compact);
    compactMeter.setVisible (compact);
    resized();
    repaint();
}

void MixerStrip::timerCallback()
{
    const float l = strip.peakL.exchange (0.0f);
    const float r = strip.peakR.exchange (0.0f);
    if (compact) compactMeter.setLevels (l, r);
    else meter.setLevels (l, r);
}

void MixerStrip::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (30);
    menuButton.setBounds (header.removeFromRight (28).reduced (4, 6));

    if (compact)
    {
        r.reduce (6, 2);
        auto row = r.removeFromTop (22);
        muteButton.setBounds (row.removeFromLeft (26));
        row.removeFromLeft (3);
        if (soloButton.isVisible())
        {
            soloButton.setBounds (row.removeFromLeft (26));
            row.removeFromLeft (6);
        }
        compactFader.setBounds (row);
        r.removeFromTop (3);
        compactMeter.setBounds (r.removeFromTop (6));
        return;
    }

    r.reduce (6, 0);
    r.removeFromTop (4);
    instrumentButton->setBounds (r.removeFromTop (40));
    r.removeFromTop (6);

    auto bottom = r.removeFromBottom (30);
    {
        auto b = bottom.reduced (0, 2);
        const int w = (b.getWidth() - 6) / 2;
        muteButton.setBounds (b.removeFromLeft (soloButton.isVisible() ? w : b.getWidth()));
        b.removeFromLeft (6);
        soloButton.setBounds (b);
    }
    auto panArea = r.removeFromBottom (50);
    panKnob.setBounds (panArea.withSizeKeepingCentre (46, 46));
    r.removeFromBottom (18); // dB label row (painted)

    auto right = r.removeFromRight (44);
    meter.setBounds (right.removeFromRight (12).reduced (0, 4));
    fader.setBounds (right.reduced (2, 0));
    r.removeFromRight (4);
    fxViewport.setBounds (r);
    fxPanel->setSize (r.getWidth() - (fxPanel->getIdealHeight() > r.getHeight() ? 7 : 0), fxPanel->getIdealHeight());
}

void MixerStrip::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    Theme::drawCard (g, bounds.reduced (1.0f), 10.0f);

    // header
    const auto c = colour();
    auto header = headerArea().toFloat().reduced (1.0f);
    {
        juce::Path p;
        p.addRoundedRectangle (header.getX(), header.getY(), header.getWidth(), header.getHeight(), 10.0f, 10.0f, true, true, false, false);
        g.setColour (c.withAlpha (0.16f).overlaidWith (juce::Colours::white.withAlpha (0.0f)));
        g.fillPath (p);
    }

    auto hr = header.reduced (7.0f, 6.0f);
    if (strip.isMaster)
    {
        auto badge = hr.removeFromLeft (20.0f);
        g.setColour (Theme::text);
        g.fillRoundedRectangle (badge, 4.0f);
        Theme::drawIcon (g, Theme::Icon::Mixer, badge.reduced (3.0f), juce::Colours::white);
    }
    else
    {
        auto badge = hr.removeFromLeft (24.0f);
        g.setColour (c);
        g.fillRoundedRectangle (badge, 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (Theme::uiFont (11.5f, 700));
        g.drawText (juce::String (track + 1).paddedLeft ('0', 2), badge, juce::Justification::centred);
    }
    hr.removeFromLeft (7.0f);
    hr.removeFromRight (20.0f);
    g.setColour (Theme::text);
    g.setFont (Theme::uiFont (14.0f, 700));
    g.drawFittedText (title(), hr.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);

    if (compact)
        return;

    // dB readout
    const auto db = strip.volumeDb.load();
    const juce::String dbText = db <= -59.9f ? juce::String ("-inf dB") : juce::String (db, 1) + " dB";
    g.setColour (Theme::text);
    g.setFont (Theme::uiFont (11.5f, 700));
    auto dbArea = juce::Rectangle<int> (6, panKnob.getY() - 18, getWidth() - 12, 16);
    g.drawText (dbText, dbArea, juce::Justification::centredRight);

    // pan labels
    g.setColour (Theme::textDim);
    g.setFont (Theme::uiFont (10.5f, 600));
    auto pk = panKnob.getBounds();
    g.drawText ("L", pk.getX() - 14, pk.getBottom() - 14, 12, 12, juce::Justification::centred);
    g.drawText ("R", pk.getRight() + 2, pk.getBottom() - 14, 12, 12, juce::Justification::centred);
    g.drawText ("C", pk.getCentreX() - 6, pk.getBottom() - 6, 12, 12, juce::Justification::centred);
    if (std::abs (strip.pan.load()) > 0.005f)
    {
        const int pv = juce::roundToInt (strip.pan.load() * 100.0f);
        g.drawText ((pv < 0 ? "L" : "R") + juce::String (std::abs (pv)), dbArea, juce::Justification::centredLeft);
    }
}

void MixerStrip::mouseDown (const juce::MouseEvent& e)
{
    if (headerArea().contains (e.getPosition()) && ! strip.isMaster && ctx.onSelectTrack)
        ctx.onSelectTrack (track);
}

void MixerStrip::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (headerArea().contains (e.getPosition()) && ! strip.isMaster)
        renameTrack();
}

//==============================================================================
void MixerStrip::renameTrack()
{
    if (! juce::isPositiveAndBelow (track, ctx.engine.song.numTracks())) return;
    auto* aw = new juce::AlertWindow ("Rename track", "Enter a new name for the track:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", title());
    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
    {
        if (r == 1 && juce::isPositiveAndBelow (track, ctx.engine.song.numTracks()))
        {
            auto name = aw->getTextEditorContents ("name").trim();
            if (name.isNotEmpty())
            {
                ctx.engine.song.tracks[(size_t) track].name = name.substring (0, 24);
                ctx.state.dirty = true;
                if (ctx.onStructureChanged) ctx.onStructureChanged();
            }
        }
    }), true);
}

void MixerStrip::showTrackMenu()
{
    juce::PopupMenu m;
    if (strip.isMaster)
    {
        m.addItem (1, "Reset master strip");
        m.addItem (2, "Audio settings...");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton), [this] (int r)
        {
            if (r == 1)
            {
                strip.volumeDb = 0.0f; strip.pan = 0.0f; strip.mute = false;
                for (int i = 0; i < 3; ++i) strip.getBuiltin (i)->enabled = false;
                refresh();
            }
            else if (r == 2 && ctx.onShowAudioSettings)
                ctx.onShowAudioSettings();
        });
        return;
    }

    const int n = ctx.engine.song.numTracks();
    if (! juce::isPositiveAndBelow (track, n)) return;
    const auto& info = ctx.engine.song.tracks[(size_t) track];

    m.addSectionHeader (info.name);
    m.addItem (1, "Rename...");
    juce::PopupMenu colours;
    for (int i = 0; i < Song::palette().size(); ++i)
        colours.addColouredItem (100 + i, "Colour " + juce::String (i + 1), Song::palette()[i], true, info.colour == Song::palette()[i]);
    m.addSubMenu ("Colour", colours);
    juce::PopupMenu cols;
    for (int i = 1; i <= kMaxNoteColumns; ++i)
        cols.addItem (200 + i, juce::String (i) + (i == 1 ? " column" : " columns"), true, info.noteColumns == i);
    m.addSubMenu ("Note columns", cols);
    m.addSeparator();
    m.addItem (2, "Choose instrument...");
    m.addItem (3, "Add plug-in effect...");
    m.addSeparator();
    m.addItem (4, "Move left", track > 0);
    m.addItem (5, "Move right", track < n - 1);
    m.addItem (6, "Insert track after");
    m.addSeparator();
    m.addItem (7, "Delete track", n > 1);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton), [this] (int r)
    {
        auto& e = ctx.engine;
        const int t = track;
        if (! juce::isPositiveAndBelow (t, e.song.numTracks())) return;

        if (r == 1) renameTrack();
        else if (r >= 100 && r < 200)
        {
            e.song.tracks[(size_t) t].colour = Song::palette()[r - 100];
            ctx.state.dirty = true;
            if (ctx.onStructureChanged) ctx.onStructureChanged();
        }
        else if (r > 200 && r <= 200 + kMaxNoteColumns)
        {
            e.setTrackColumns (t, r - 200);
            ctx.state.dirty = true;
            if (ctx.onStructureChanged) ctx.onStructureChanged();
        }
        else if (r == 2) showInstrumentMenu();
        else if (r == 3) showAddFxMenu();
        else if (r == 4 || r == 5)
        {
            ctx.undo.clearUndoHistory();
            e.moveTrack (t, r == 4 ? t - 1 : t + 1);
            ctx.state.cursorTrack = r == 4 ? t - 1 : t + 1;
            ctx.state.dirty = true;
            if (ctx.onStructureChanged) ctx.onStructureChanged();
        }
        else if (r == 6)
        {
            ctx.undo.clearUndoHistory();
            e.addTrack ("Track " + juce::String (e.song.numTracks() + 1).paddedLeft ('0', 2), t + 1);
            ctx.state.dirty = true;
            if (ctx.onStructureChanged) ctx.onStructureChanged();
        }
        else if (r == 7)
        {
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Delete track",
                "Delete track \"" + e.song.tracks[(size_t) t].name + "\" and its channel strip?", "Delete", "Cancel", nullptr,
                juce::ModalCallbackFunction::create ([this, t] (int ok)
                {
                    if (ok == 0) return;
                    // the strip component may be deleted by the relayout, so take what we need first
                    auto* c = &ctx;
                    c->undo.clearUndoHistory();
                    juce::MessageManager::callAsync ([c, t]
                    {
                        c->engine.removeTrack (t);
                        c->state.dirty = true;
                        if (c->onStructureChanged) c->onStructureChanged();
                    });
                }));
        }
    });
}

void MixerStrip::loadSoundFontPreset (const juce::String& genre, int bank, int program, bool drums, const juce::String& name, std::shared_ptr<SoundFontFile> font)
{
    if (font == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "SoundFont missing",
            "No General MIDI SoundFont was found. Choose a .sf2 file via \"Load SoundFont file...\" or install the bundled GeneralUser GS font.");
        return;
    }

    auto sf = std::make_unique<SoundFontInstrument>();
    sf->genre = genre;
    sf->setPreset (bank, program, drums, name);
    sf->setSoundFont (font);

    auto old = strip.setInstrument (std::move (sf), InstrumentKind::SoundFont);
    if (old != nullptr)
    {
        PluginHost::get().closeEditor (old.get());
        old.reset();
    }
    ctx.state.dirty = true;
    refresh();
}

void MixerStrip::loadPluginInstrument (const juce::PluginDescription& d)
{
    juce::String err;
    auto p = PluginHost::get().createPlugin (d, ctx.engine.getSampleRate(), ctx.engine.getBlockSize(), err);
    if (p == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not load plug-in", d.name + "\n" + err);
        return;
    }
    auto* raw = p.get();
    auto old = strip.setInstrument (std::move (p), InstrumentKind::Plugin);
    if (old != nullptr)
    {
        PluginHost::get().closeEditor (old.get());
        old.reset();
    }
    ctx.state.dirty = true;
    refresh();
    PluginHost::get().showEditor (raw, title() + " - " + raw->getName());
}

void MixerStrip::addPluginFx (const juce::PluginDescription& d)
{
    juce::String err;
    auto p = PluginHost::get().createPlugin (d, ctx.engine.getSampleRate(), ctx.engine.getBlockSize(), err);
    if (p == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not load plug-in", d.name + "\n" + err);
        return;
    }
    auto* raw = p.get();
    strip.insertFx (std::move (p));
    strip.fxCollapsed = false;
    ctx.state.dirty = true;
    refresh();
    PluginHost::get().showEditor (raw, title() + " - " + raw->getName());
}

void MixerStrip::showInstrumentMenu()
{
    if (strip.isMaster) return;

    juce::PopupMenu m;
    auto font = SoundFontFile::getDefault();
    auto* current = strip.getSoundFont();

    m.addSectionHeader ("Built-in instruments (SoundFont)");
    const auto& genres = getGenres();
    for (int gi = 0; gi < (int) genres.size(); ++gi)
    {
        juce::PopupMenu gm;
        for (int i = 0; i < (int) genres[(size_t) gi].presets.size(); ++i)
        {
            const auto& p = genres[(size_t) gi].presets[(size_t) i];
            const bool ticked = current != nullptr && current->genre == genres[(size_t) gi].name && current->getPresetName() == p.name;
            gm.addItem (1000 + gi * 100 + i, p.name, font != nullptr, ticked);
        }
        m.addSubMenu (genres[(size_t) gi].name, gm);
    }

    if (font != nullptr)
    {
        juce::PopupMenu all;
        std::map<int, juce::PopupMenu> banks;
        const auto& presets = font->getPresets();
        for (int i = 0; i < (int) presets.size(); ++i)
            banks[presets[(size_t) i].bank].addItem (20000 + i, juce::String (presets[(size_t) i].program).paddedLeft ('0', 3) + "  " + presets[(size_t) i].name);
        for (auto& b : banks)
            all.addSubMenu (b.first == 128 ? juce::String ("Drum kits (bank 128)") : "Bank " + juce::String (b.first), b.second);
        m.addSubMenu ("All presets: " + font->getFile().getFileNameWithoutExtension(), all);
    }
    m.addItem (9001, "Load SoundFont file (.sf2)...");

    m.addSeparator();
    m.addSectionHeader ("Instrument plug-ins");
    menuPlugins.clear();
    PluginHost::get().addPluginsToMenu (m, true, 40000, menuPlugins);
    m.addItem (9004, "Plug-in Manager / scan...");

    m.addSeparator();
    m.addItem (9002, "Open instrument editor", strip.getInstrumentPlugin() != nullptr);
    m.addItem (9003, "Remove instrument", strip.getInstrument() != nullptr);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (instrumentButton.get()).withMinimumWidth (220),
                     [this, font] (int r)
    {
        if (r <= 0) return;
        const auto& genres = getGenres();

        if (r >= 1000 && r < 1000 + 100 * (int) genres.size())
        {
            const int gi = (r - 1000) / 100, pi = (r - 1000) % 100;
            const auto& p = genres[(size_t) gi].presets[(size_t) pi];
            loadSoundFontPreset (genres[(size_t) gi].name, p.bank, p.program, p.drums, p.name, font);
        }
        else if (r >= 20000 && r < 40000 && font != nullptr)
        {
            const auto& p = font->getPresets()[(size_t) (r - 20000)];
            loadSoundFontPreset ({}, p.bank, p.program, p.bank == 128, p.name, font);
        }
        else if (r >= 40000 && r - 40000 < menuPlugins.size())
        {
            loadPluginInstrument (menuPlugins[r - 40000]);
        }
        else if (r == 9001)
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose a SoundFont", juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.sf2");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                juce::String err;
                auto sf = SoundFontFile::load (f, err);
                if (sf == nullptr)
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "SoundFont", err);
                    return;
                }
                const auto& presets = sf->getPresets();
                if (presets.empty()) return;
                const auto& p = presets.front();
                loadSoundFontPreset ({}, p.bank, p.program, p.bank == 128, p.name, sf);
            });
        }
        else if (r == 9002)
        {
            if (auto* p = strip.getInstrumentPlugin())
                PluginHost::get().showEditor (p, title() + " - " + p->getName());
        }
        else if (r == 9003)
        {
            auto old = strip.setInstrument (nullptr, InstrumentKind::None);
            if (old != nullptr) PluginHost::get().closeEditor (old.get());
            old.reset();
            ctx.state.dirty = true;
            refresh();
        }
        else if (r == 9004)
        {
            PluginHost::get().showPluginManager();
        }
    });
}

void MixerStrip::showAddFxMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Add effect plug-in");
    menuPlugins.clear();
    PluginHost::get().addPluginsToMenu (m, false, 40000, menuPlugins);
    m.addSeparator();
    m.addItem (9004, "Plug-in Manager / scan...");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fxViewport), [this] (int r)
    {
        if (r >= 40000 && r - 40000 < menuPlugins.size())
            addPluginFx (menuPlugins[r - 40000]);
        else if (r == 9004)
            PluginHost::get().showPluginManager();
    });
}

} // namespace artilody
