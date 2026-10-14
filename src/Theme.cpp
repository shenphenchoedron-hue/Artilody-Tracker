#include "Theme.h"
#include "BinaryData.h"

namespace artilody
{

namespace
{
    struct Typefaces
    {
        juce::Typeface::Ptr regular, semibold, bold, mono, monoBold;

        Typefaces()
        {
            regular  = juce::Typeface::createSystemTypefaceFor (BinaryData::inter400_ttf, BinaryData::inter400_ttfSize);
            semibold = juce::Typeface::createSystemTypefaceFor (BinaryData::inter600_ttf, BinaryData::inter600_ttfSize);
            bold     = juce::Typeface::createSystemTypefaceFor (BinaryData::inter700_ttf, BinaryData::inter700_ttfSize);
            mono     = juce::Typeface::createSystemTypefaceFor (BinaryData::mono500_ttf, BinaryData::mono500_ttfSize);
            monoBold = juce::Typeface::createSystemTypefaceFor (BinaryData::mono700_ttf, BinaryData::mono700_ttfSize);
        }

        static Typefaces& get()
        {
            static Typefaces t;
            return t;
        }
    };
}

namespace Theme
{
    juce::Font uiFont (float height, int weight)
    {
        auto& t = Typefaces::get();
        auto tf = weight >= 700 ? t.bold : (weight >= 600 ? t.semibold : t.regular);
        return juce::Font (juce::FontOptions (tf).withHeight (height));
    }

    juce::Font monoFont (float height, bool bold)
    {
        auto& t = Typefaces::get();
        return juce::Font (juce::FontOptions (bold ? t.monoBold : t.mono).withHeight (height));
    }

    juce::Image logoFull()
    {
        static juce::Image img = juce::ImageCache::getFromMemory (BinaryData::logo_full_png, BinaryData::logo_full_pngSize);
        return img;
    }

    juce::Image logoMark()
    {
        static juce::Image img = juce::ImageCache::getFromMemory (BinaryData::logo_mark_png, BinaryData::logo_mark_pngSize);
        return img;
    }

    void drawShadow (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        juce::DropShadow ds (juce::Colour (0x1a1c2f50), 12, { 0, 3 });
        juce::Path p;
        p.addRoundedRectangle (r, radius);
        ds.drawForPath (g, p);
    }

    void drawCard (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        drawShadow (g, r, radius);
        g.setColour (panel);
        g.fillRoundedRectangle (r, radius);
        g.setColour (border.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }

    void drawInstrumentIcon (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& seed, juce::Colour base)
    {
        const auto h = (uint32_t) seed.hashCode();
        const float hueShift = (float) (h % 1000) / 1000.0f * 0.25f - 0.12f;
        auto c1 = base.withRotatedHue (hueShift).darker (0.6f);
        auto c2 = base.withRotatedHue (-hueShift).brighter (0.3f);

        juce::ColourGradient grad (c1, r.getX(), r.getY(), c2, r.getRight(), r.getBottom(), false);
        grad.addColour (0.55, juce::Colour (0xff10182a));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 5.0f);

        // little waveform / light sweep
        juce::Path wave;
        const int seg = 12;
        for (int i = 0; i <= seg; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) seg;
            const float y = r.getCentreY() + std::sin ((float) i * 0.9f + (float) (h % 7)) * r.getHeight() * 0.18f;
            if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
        }
        g.setColour (c2.withAlpha (0.85f));
        g.strokePath (wave, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour c)
    {
        g.setColour (c);
        auto s = juce::jmin (r.getWidth(), r.getHeight());
        auto b = r.withSizeKeepingCentre (s, s);
        juce::Path p;
        const float st = juce::jmax (1.5f, s * 0.11f);
        auto stroke = [&] (const juce::Path& path) { g.strokePath (path, juce::PathStrokeType (st, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); };

        switch (icon)
        {
            case Icon::Play:
                p.addTriangle (b.getX() + s * 0.28f, b.getY() + s * 0.2f, b.getX() + s * 0.28f, b.getBottom() - s * 0.2f, b.getRight() - s * 0.18f, b.getCentreY());
                g.fillPath (p, {});
                break;
            case Icon::Stop:
                g.fillRoundedRectangle (b.reduced (s * 0.25f), s * 0.06f);
                break;
            case Icon::Record:
                g.fillEllipse (b.reduced (s * 0.22f));
                break;
            case Icon::Rewind:
                g.fillRect (b.getX() + s * 0.22f, b.getY() + s * 0.25f, s * 0.1f, s * 0.5f);
                p.addTriangle (b.getRight() - s * 0.22f, b.getY() + s * 0.25f, b.getRight() - s * 0.22f, b.getBottom() - s * 0.25f, b.getX() + s * 0.34f, b.getCentreY());
                g.fillPath (p);
                break;
            case Icon::Loop:
            {
                auto rr = b.reduced (s * 0.22f);
                p.addRoundedRectangle (rr, s * 0.15f);
                stroke (p);
                juce::Path a;
                a.addTriangle (rr.getRight() - s * 0.12f, rr.getY() - s * 0.1f, rr.getRight() - s * 0.12f, rr.getY() + s * 0.1f, rr.getRight() + s * 0.05f, rr.getY());
                g.fillPath (a);
                break;
            }
            case Icon::Plus:
                p.startNewSubPath (b.getCentreX(), b.getY() + s * 0.22f); p.lineTo (b.getCentreX(), b.getBottom() - s * 0.22f);
                p.startNewSubPath (b.getX() + s * 0.22f, b.getCentreY()); p.lineTo (b.getRight() - s * 0.22f, b.getCentreY());
                stroke (p);
                break;
            case Icon::Dots:
                for (int i = -1; i <= 1; ++i)
                    g.fillEllipse (b.getCentreX() + (float) i * s * 0.25f - s * 0.07f, b.getCentreY() - s * 0.07f, s * 0.14f, s * 0.14f);
                break;
            case Icon::Gear:
            {
                auto cr = b.reduced (s * 0.18f);
                for (int i = 0; i < 8; ++i)
                {
                    juce::Path tooth;
                    tooth.addRoundedRectangle (-s * 0.07f, -cr.getHeight() * 0.5f, s * 0.14f, s * 0.2f, s * 0.03f);
                    tooth.applyTransform (juce::AffineTransform::rotation ((float) i * juce::MathConstants<float>::pi / 4.0f).translated (cr.getCentreX(), cr.getCentreY()));
                    g.fillPath (tooth);
                }
                p.addEllipse (cr.reduced (s * 0.08f));
                stroke (p);
                g.fillEllipse (cr.withSizeKeepingCentre (s * 0.14f, s * 0.14f));
                break;
            }
            case Icon::Save:
            {
                auto rr = b.reduced (s * 0.2f);
                p.addRoundedRectangle (rr, s * 0.06f);
                stroke (p);
                g.fillRect (rr.getX() + rr.getWidth() * 0.25f, rr.getY(), rr.getWidth() * 0.5f, rr.getHeight() * 0.3f);
                g.fillRect (rr.getX() + rr.getWidth() * 0.2f, rr.getBottom() - rr.getHeight() * 0.38f, rr.getWidth() * 0.6f, rr.getHeight() * 0.25f);
                break;
            }
            case Icon::Folder:
            {
                auto rr = b.reduced (s * 0.18f, s * 0.26f);
                p.startNewSubPath (rr.getX(), rr.getBottom());
                p.lineTo (rr.getX(), rr.getY());
                p.lineTo (rr.getX() + rr.getWidth() * 0.35f, rr.getY());
                p.lineTo (rr.getX() + rr.getWidth() * 0.45f, rr.getY() + rr.getHeight() * 0.18f);
                p.lineTo (rr.getRight(), rr.getY() + rr.getHeight() * 0.18f);
                p.lineTo (rr.getRight(), rr.getBottom());
                p.closeSubPath();
                stroke (p);
                break;
            }
            case Icon::Chevron:
            case Icon::ChevronDown:
                p.startNewSubPath (b.getX() + s * 0.3f, b.getY() + s * 0.4f);
                p.lineTo (b.getCentreX(), b.getY() + s * 0.62f);
                p.lineTo (b.getRight() - s * 0.3f, b.getY() + s * 0.4f);
                stroke (p);
                break;
            case Icon::ChevronUp:
                p.startNewSubPath (b.getX() + s * 0.3f, b.getY() + s * 0.6f);
                p.lineTo (b.getCentreX(), b.getY() + s * 0.38f);
                p.lineTo (b.getRight() - s * 0.3f, b.getY() + s * 0.6f);
                stroke (p);
                break;
            case Icon::Close:
                p.startNewSubPath (b.getX() + s * 0.3f, b.getY() + s * 0.3f); p.lineTo (b.getRight() - s * 0.3f, b.getBottom() - s * 0.3f);
                p.startNewSubPath (b.getRight() - s * 0.3f, b.getY() + s * 0.3f); p.lineTo (b.getX() + s * 0.3f, b.getBottom() - s * 0.3f);
                stroke (p);
                break;
            case Icon::Power:
            {
                auto rr = b.reduced (s * 0.22f);
                p.addCentredArc (rr.getCentreX(), rr.getCentreY(), rr.getWidth() * 0.5f, rr.getHeight() * 0.5f, 0.0f, 0.7f, juce::MathConstants<float>::twoPi - 0.7f, true);
                p.startNewSubPath (rr.getCentreX(), rr.getY() - s * 0.04f);
                p.lineTo (rr.getCentreX(), rr.getCentreY());
                stroke (p);
                break;
            }
            case Icon::Mixer:
                for (int i = 0; i < 3; ++i)
                {
                    const float x = b.getX() + s * (0.28f + 0.22f * (float) i);
                    p.startNewSubPath (x, b.getY() + s * 0.22f);
                    p.lineTo (x, b.getBottom() - s * 0.22f);
                    stroke (p);
                    p.clear();
                    const float y = b.getY() + s * (i == 1 ? 0.62f : 0.38f);
                    g.fillRoundedRectangle (x - s * 0.08f, y - s * 0.05f, s * 0.16f, s * 0.1f, 1.5f);
                }
                break;
            case Icon::Plug:
            {
                auto rr = b.reduced (s * 0.28f, s * 0.3f);
                g.fillRoundedRectangle (rr.withTrimmedTop (rr.getHeight() * 0.35f), s * 0.08f);
                g.fillRect (rr.getX() + rr.getWidth() * 0.2f, rr.getY(), s * 0.08f, rr.getHeight() * 0.4f);
                g.fillRect (rr.getRight() - rr.getWidth() * 0.2f - s * 0.08f, rr.getY(), s * 0.08f, rr.getHeight() * 0.4f);
                g.fillRect (rr.getCentreX() - s * 0.04f, rr.getBottom(), s * 0.08f, s * 0.15f);
                break;
            }
            case Icon::Export:
                p.startNewSubPath (b.getCentreX(), b.getY() + s * 0.2f); p.lineTo (b.getCentreX(), b.getY() + s * 0.62f);
                p.startNewSubPath (b.getX() + s * 0.32f, b.getY() + s * 0.45f); p.lineTo (b.getCentreX(), b.getY() + s * 0.65f); p.lineTo (b.getRight() - s * 0.32f, b.getY() + s * 0.45f);
                p.startNewSubPath (b.getX() + s * 0.2f, b.getY() + s * 0.7f); p.lineTo (b.getX() + s * 0.2f, b.getBottom() - s * 0.18f);
                p.lineTo (b.getRight() - s * 0.2f, b.getBottom() - s * 0.18f); p.lineTo (b.getRight() - s * 0.2f, b.getY() + s * 0.7f);
                stroke (p);
                break;
            case Icon::Grid:
                for (int i = 0; i < 2; ++i)
                    for (int j = 0; j < 2; ++j)
                        g.fillRoundedRectangle (b.getX() + s * (0.22f + 0.3f * (float) i), b.getY() + s * (0.22f + 0.3f * (float) j), s * 0.24f, s * 0.24f, 2.0f);
                break;
            case Icon::Follow:
                p.addEllipse (b.reduced (s * 0.22f));
                stroke (p);
                g.fillEllipse (b.reduced (s * 0.4f));
                break;
            case Icon::Undo:
            case Icon::Redo:
            {
                const bool undo = icon == Icon::Undo;
                p.addCentredArc (b.getCentreX(), b.getCentreY() + s * 0.06f, s * 0.26f, s * 0.24f, 0.0f,
                                 undo ? -1.9f : 1.9f, undo ? 1.4f : -1.4f, true);
                stroke (p);
                juce::Path a;
                const float ax = undo ? b.getX() + s * 0.22f : b.getRight() - s * 0.22f;
                a.addTriangle (ax - s * 0.1f, b.getY() + s * 0.3f, ax + s * 0.1f, b.getY() + s * 0.3f, ax, b.getY() + s * 0.48f);
                g.fillPath (a);
                break;
            }
        }
    }
} // namespace Theme

//==============================================================================
ArtilodyLookAndFeel::ArtilodyLookAndFeel()
{
    auto& cs = getCurrentColourScheme();
    cs.setUIColour (ColourScheme::windowBackground, Theme::background);
    cs.setUIColour (ColourScheme::widgetBackground, Theme::panel);
    cs.setUIColour (ColourScheme::menuBackground, Theme::panel);
    cs.setUIColour (ColourScheme::outline, Theme::border);
    cs.setUIColour (ColourScheme::defaultText, Theme::text);
    cs.setUIColour (ColourScheme::defaultFill, Theme::accent);
    cs.setUIColour (ColourScheme::highlightedText, Theme::text);
    cs.setUIColour (ColourScheme::highlightedFill, Theme::selection);
    cs.setUIColour (ColourScheme::menuText, Theme::text);

    setColour (juce::ResizableWindow::backgroundColourId, Theme::background);
    setColour (juce::DocumentWindow::backgroundColourId, Theme::background);
    setColour (juce::TextButton::buttonColourId, Theme::panelAlt);
    setColour (juce::TextButton::buttonOnColourId, Theme::accent);
    setColour (juce::TextButton::textColourOffId, Theme::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ComboBox::backgroundColourId, Theme::panelAlt);
    setColour (juce::ComboBox::textColourId, Theme::text);
    setColour (juce::ComboBox::outlineColourId, Theme::border);
    setColour (juce::ComboBox::arrowColourId, Theme::text);
    setColour (juce::PopupMenu::backgroundColourId, Theme::panel);
    setColour (juce::PopupMenu::textColourId, Theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::selection);
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::text);
    setColour (juce::PopupMenu::headerTextColourId, Theme::textDim);
    setColour (juce::Label::textColourId, Theme::text);
    setColour (juce::TextEditor::backgroundColourId, Theme::panelAlt);
    setColour (juce::TextEditor::textColourId, Theme::text);
    setColour (juce::TextEditor::outlineColourId, Theme::border);
    setColour (juce::TextEditor::focusedOutlineColourId, Theme::accentBlue);
    setColour (juce::TextEditor::highlightColourId, Theme::selection);
    setColour (juce::CaretComponent::caretColourId, Theme::accentBlue);
    setColour (juce::Slider::textBoxTextColourId, Theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, Theme::accent);
    setColour (juce::Slider::thumbColourId, Theme::accent);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xffc3ccda));
    setColour (juce::ListBox::backgroundColourId, Theme::panel);
    setColour (juce::ListBox::textColourId, Theme::text);
    setColour (juce::TooltipWindow::backgroundColourId, Theme::text);
    setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::outlineColourId, Theme::text);
    setColour (juce::AlertWindow::backgroundColourId, Theme::panel);
    setColour (juce::AlertWindow::textColourId, Theme::text);
    setColour (juce::AlertWindow::outlineColourId, Theme::border);
    setColour (juce::ToggleButton::textColourId, Theme::text);
    setColour (juce::ToggleButton::tickColourId, Theme::accent);
    setColour (juce::TableHeaderComponent::backgroundColourId, Theme::panelAlt);
    setColour (juce::TableHeaderComponent::textColourId, Theme::text);
    setColour (juce::TableHeaderComponent::outlineColourId, Theme::border);
    setColour (juce::TableListBox::backgroundColourId, Theme::panel);
    setColour (juce::ProgressBar::foregroundColourId, Theme::accent);
    setColour (juce::ProgressBar::backgroundColourId, Theme::panelAlt);
    setColour (juce::GroupComponent::outlineColourId, Theme::border);
    setColour (juce::GroupComponent::textColourId, Theme::text);
    setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, Theme::selection);
    setColour (juce::DirectoryContentsDisplayComponent::textColourId, Theme::text);
    setColour (juce::FileChooserDialogBox::titleTextColourId, Theme::text);
    setColour (juce::PropertyComponent::backgroundColourId, Theme::panelAlt);
    setColour (juce::PropertyComponent::labelTextColourId, Theme::text);

    setDefaultSansSerifTypeface (Typefaces::get().regular);
}

juce::Typeface::Ptr ArtilodyLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    auto& t = Typefaces::get();
    const auto name = f.getTypefaceName();
    if (name == juce::Font::getDefaultMonospacedFontName())
        return f.isBold() ? t.monoBold : t.mono;
    if (name == juce::Font::getDefaultSansSerifFontName() || name.isEmpty())
        return f.isBold() ? t.semibold : t.regular;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void ArtilodyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : bg;
    if (down) c = c.darker (0.08f);
    else if (hi) c = c.brighter (0.04f).overlaidWith (Theme::accentBlue.withAlpha (0.04f));
    g.setColour (c);
    g.fillRoundedRectangle (r, 7.0f);
    if (! b.getToggleState())
    {
        g.setColour (Theme::border);
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
    }
}

juce::Font ArtilodyLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return Theme::uiFont (juce::jmin (14.0f, (float) h * 0.5f), 600);
}

void ArtilodyLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                  .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
}

void ArtilodyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const auto fill = s.findColour (juce::Slider::rotarySliderFillColourId);
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f, start, end, true);
    g.setColour (juce::Colour (0xffe3e8f0));
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc
    const float from = bipolar ? (start + end) * 0.5f : start;
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (fill);
    g.strokePath (arc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob body
    auto knob = juce::Rectangle<float> (radius * 1.25f, radius * 1.25f).withCentre (centre);
    juce::DropShadow (juce::Colour (0x301c2f50), 4, { 0, 1 }).drawForRectangle (g, knob.toNearestInt());
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, knob.getX(), knob.getY(), juce::Colour (0xffe9edf3), knob.getX(), knob.getBottom(), false));
    g.fillEllipse (knob);
    g.setColour (Theme::border);
    g.drawEllipse (knob, 1.0f);

    // pointer
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.5f, -radius * 0.6f, 3.0f, radius * 0.32f, 1.5f);
    g.setColour (Theme::text);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (centre));
}

void ArtilodyLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                            juce::Slider::SliderStyle style, juce::Slider& s)
{
    const auto fill = s.findColour (juce::Slider::thumbColourId);
    if (style == juce::Slider::LinearVertical)
    {
        const float cx = (float) x + (float) w * 0.5f;
        auto track = juce::Rectangle<float> (cx - 2.0f, (float) y, 4.0f, (float) h);
        g.setColour (juce::Colour (0xffdfe5ee));
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (fill.withAlpha (0.55f));
        g.fillRoundedRectangle (track.withTop (pos), 2.0f);

        auto thumb = juce::Rectangle<float> ((float) w * 0.72f, 14.0f).withCentre ({ cx, pos });
        juce::DropShadow (juce::Colour (0x401c2f50), 4, { 0, 1 }).drawForRectangle (g, thumb.toNearestInt());
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (fill);
        g.fillRoundedRectangle (thumb.reduced (2.0f, 5.5f), 1.0f);
        g.setColour (Theme::border);
        g.drawRoundedRectangle (thumb, 3.0f, 1.0f);
    }
    else
    {
        const float cy = (float) y + (float) h * 0.5f;
        auto track = juce::Rectangle<float> ((float) x, cy - 2.0f, (float) w, 4.0f);
        g.setColour (juce::Colour (0xffdfe5ee));
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (fill);
        g.fillRoundedRectangle (track.withRight (pos), 2.0f);
        g.setColour (juce::Colours::white);
        auto thumb = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ pos, cy });
        g.fillEllipse (thumb);
        g.setColour (fill);
        g.drawEllipse (thumb, 2.0f);
    }
}

void ArtilodyLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    Theme::drawIcon (g, Theme::Icon::Chevron, juce::Rectangle<float> ((float) w - 22.0f, 0, 18.0f, (float) h), box.findColour (juce::ComboBox::arrowColourId));
}

juce::Font ArtilodyLookAndFeel::getComboBoxFont (juce::ComboBox& b)
{
    return Theme::uiFont (juce::jmin (14.0f, (float) b.getHeight() * 0.55f), 600);
}

juce::Font ArtilodyLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

juce::Font ArtilodyLookAndFeel::getPopupMenuFont()
{
    return Theme::uiFont (14.5f, 400);
}

void ArtilodyLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::border);
    g.drawRect (0, 0, w, h, 1);
}

void ArtilodyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto sw = juce::Rectangle<float> (30.0f, 16.0f).withCentre ({ r.getX() + 17.0f, r.getCentreY() });
    g.setColour (b.getToggleState() ? Theme::accent : juce::Colour (0xffd2d9e4));
    g.fillRoundedRectangle (sw, 8.0f);
    g.setColour (juce::Colours::white);
    const float kx = b.getToggleState() ? sw.getRight() - 14.0f : sw.getX() + 2.0f;
    g.fillEllipse (kx, sw.getY() + 2.0f, 12.0f, 12.0f);
    g.setColour (Theme::text.withAlpha (hi ? 1.0f : 0.9f));
    g.setFont (Theme::uiFont (13.5f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (38), juce::Justification::centredLeft, 1);
}

void ArtilodyLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar& bar, int x, int y, int w, int h, bool vertical,
                                         int thumbStart, int thumbSize, bool over, bool down)
{
    juce::ignoreUnused (x, y);
    auto thumb = vertical ? juce::Rectangle<float> (2.0f, (float) thumbStart, (float) w - 4.0f, (float) thumbSize)
                          : juce::Rectangle<float> ((float) thumbStart, 2.0f, (float) thumbSize, (float) h - 4.0f);
    g.setColour (bar.findColour (juce::ScrollBar::thumbColourId).withAlpha (over || down ? 0.95f : 0.7f));
    g.fillRoundedRectangle (thumb, juce::jmin (thumb.getWidth(), thumb.getHeight()) * 0.5f);
}

void ArtilodyLookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    juce::DropShadow (juce::Colour (0x401c2f50), 14, { 0, 4 }).drawForPath (g, path);
    g.setColour (Theme::panel);
    g.fillPath (path);
    g.setColour (Theme::border);
    g.strokePath (path, juce::PathStrokeType (1.0f));
}

void ArtilodyLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 6.0f);
}

void ArtilodyLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    if (! e.isEnabled()) return;
    g.setColour (e.hasKeyboardFocus (true) ? e.findColour (juce::TextEditor::focusedOutlineColourId) : e.findColour (juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 6.0f, 1.0f);
}

//==============================================================================
void IconButton::paintButton (juce::Graphics& g, bool hi, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    if (drawBackground || on)
    {
        auto c = on ? onFill : fill;
        if (down) c = c.darker (0.1f);
        else if (hi) c = c.brighter (0.05f).overlaidWith (Theme::accentBlue.withAlpha (on ? 0.0f : 0.05f));
        g.setColour (c);
        g.fillRoundedRectangle (r, radius);
    }
    else if (hi)
    {
        g.setColour (Theme::accentBlue.withAlpha (0.07f));
        g.fillRoundedRectangle (r, radius);
    }
    const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.62f;
    Theme::drawIcon (g, icon, r.withSizeKeepingCentre (s, s), (on ? onIcon : iconColour).withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f));
}

} // namespace artilody
