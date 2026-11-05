#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Stile grafico della famiglia SCRIBA (ripreso da Scriba Align).
namespace scriba::ui
{
    const juce::Colour bgTop       { 0xff121826 };
    const juce::Colour bgBottom    { 0xff3d5c84 };
    const juce::Colour card        { 0xe6162131 };
    const juce::Colour cardEdge    { 0xff2a3a52 };
    const juce::Colour inset       { 0xff111a28 };
    const juce::Colour control     { 0xff172133 };
    const juce::Colour track       { 0xff26354b };
    const juce::Colour accent      { 0xff35dfc5 };
    const juce::Colour accentDark  { 0xff15373b };
    const juce::Colour ctaLight    { 0xffe3fcf6 };
    const juce::Colour ctaText     { 0xff0e3b3d };
    const juce::Colour amber       { 0xfff0b45a };
    const juce::Colour amberLight  { 0xfffbe6c2 };
    const juce::Colour text        { 0xffeef2f7 };
    const juce::Colour textDim     { 0xff8b9ab0 };

    inline juce::Font font (float size, bool bold = true, float tracking = 0.0f)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain))
                   .withExtraKerningFactor (tracking);
    }

    // Etichetta maiuscola spaziata, come "QUESTA TRACCIA" in Scriba Align
    inline void drawCaps (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r,
                          float size, juce::Colour c, juce::Justification j = juce::Justification::centredLeft,
                          float tracking = 0.18f, bool bold = true)
    {
        g.setColour (c);
        g.setFont (font (size, bold, tracking));
        g.drawText (s, r, j, false);
    }
}

class ScribaLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ScribaLookAndFeel()
    {
        using namespace scriba::ui;
        setColour (juce::ComboBox::backgroundColourId, control);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, cardEdge);
        setColour (juce::ComboBox::arrowColourId, accent);
        setColour (juce::PopupMenu::backgroundColourId, control);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDark);
        setColour (juce::PopupMenu::highlightedTextColourId, accent);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, accent);
        setColour (juce::ScrollBar::thumbColourId, track);
    }

    // ---------------- manopola grande ad arco
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace scriba::ui;
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const auto c = bounds.getCentre();
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 16.0f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        juce::Path trackPath;
        trackPath.addCentredArc (c.x, c.y, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (track);
        g.strokePath (trackPath, juce::PathStrokeType (24.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (pos > 0.001f)
        {
            juce::Path valuePath;
            valuePath.addCentredArc (c.x, c.y, radius, radius, 0.0f, startAngle, angle, true);
            g.setColour (s.isEnabled() ? accent : textDim.withAlpha (0.6f));
            g.strokePath (valuePath, juce::PathStrokeType (16.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // punto di partenza (anello bianco) e maniglia turchese
        const auto at = [&c, radius] (float a) { return c + juce::Point<float> (std::sin (a), -std::cos (a)) * radius; };
        const auto p0 = at (startAngle);
        g.setColour (bgTop);   g.fillEllipse (juce::Rectangle<float> (16, 16).withCentre (p0));
        g.setColour (text);    g.drawEllipse (juce::Rectangle<float> (12, 12).withCentre (p0), 2.5f);

        const auto ph = at (angle);
        g.setColour (bgTop);   g.fillEllipse (juce::Rectangle<float> (32, 32).withCentre (ph));
        g.setColour (s.isEnabled() ? accent : textDim);  g.fillEllipse (juce::Rectangle<float> (25, 25).withCentre (ph));

        // cerchio interno con i valori
        const float inner = radius * 0.66f;
        const auto innerBox = juce::Rectangle<float> (inner * 2, inner * 2).withCentre (c);
        g.setColour (inset.withAlpha (0.6f));
        g.fillEllipse (innerBox);
        g.setColour (cardEdge.brighter (0.1f));
        g.drawEllipse (innerBox, 1.5f);

        const auto title    = s.getProperties()["title"].toString();
        const auto subtitle = s.getProperties()["subtitle"].toString();
        const auto value    = juce::String (juce::roundToInt (s.getValue())) + "%";

        drawCaps (g, title, { c.x - inner, c.y - inner * 0.62f, inner * 2, 18 }, 13.0f, accent, juce::Justification::centred, 0.14f);
        g.setColour (s.isEnabled() ? text : textDim);
        g.setFont (font (40.0f, true));
        g.drawText (value, juce::Rectangle<float> (inner * 2, 48).withCentre (c.translated (0, 2)), juce::Justification::centred);
        drawCaps (g, subtitle, { c.x - inner, c.y + inner * 0.36f, inner * 2, 16 }, 11.0f, textDim, juce::Justification::centred, 0.16f, false);
    }

    // ---------------- slider orizzontale (come "CORREZIONE" di Align)
    int getSliderThumbRadius (juce::Slider&) override { return 13; }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        using namespace scriba::ui;
        const float cy = (float) y + (float) h * 0.5f;
        const auto trackBox = juce::Rectangle<float> ((float) x, cy - 3.0f, (float) w, 6.0f);
        g.setColour (track);
        g.fillRoundedRectangle (trackBox, 3.0f);

        const bool bipolar = s.getProperties()["bipolar"];
        const float from = bipolar ? trackBox.getCentreX() : trackBox.getX();

        if (bipolar)
        {
            g.setColour (textDim.withAlpha (0.6f));
            g.fillRect (juce::Rectangle<float> (2.0f, 16.0f).withCentre ({ from, cy }));
        }

        g.setColour (s.isEnabled() ? accent : textDim);
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (from, sliderPos), cy - 3.0f,
                                                        std::abs (sliderPos - from), 6.0f), 3.0f);

        const auto thumb = juce::Rectangle<float> (26, 26).withCentre ({ sliderPos, cy });
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (thumb.translated (0, 2));
        g.setColour (juce::Colour (0xfff2f5f9));
        g.fillEllipse (thumb);
    }

    // ---------------- pulsanti
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        using namespace scriba::ui;
        const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();

        if (b.getProperties()["cta"])
        {
            juce::ColourGradient grad (on ? amberLight : ctaLight, r.getX(), r.getY(),
                                       on ? amber : accent, r.getRight(), r.getY(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, 16.0f);
            if (over || down)
            {
                g.setColour (juce::Colours::white.withAlpha (down ? 0.18f : 0.08f));
                g.fillRoundedRectangle (r, 16.0f);
            }
            return;
        }

        const float radius = b.getProperties()["pill"] ? r.getHeight() * 0.5f : 12.0f;
        g.setColour (on ? accentDark : control.brighter (over ? 0.08f : 0.0f));
        g.fillRoundedRectangle (r, radius);
        g.setColour (on ? accent : cardEdge);
        g.drawRoundedRectangle (r.reduced (on ? 1.0f : 0.5f), radius, on ? 2.0f : 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        using namespace scriba::ui;
        const auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();

        if (b.getProperties()["cta"])
        {
            const auto f = font (20.0f, true, 0.22f);
            const float tw = juce::GlyphArrangement::getStringWidth (f, b.getButtonText());
            const float total = tw + 34.0f;
            const float x0 = r.getCentreX() - total * 0.5f;
            const auto dot = juce::Rectangle<float> (20, 20).withCentre ({ x0 + 10.0f, r.getCentreY() });

            g.setColour (ctaText);
            if (on) g.drawEllipse (dot.reduced (1.5f), 3.0f);
            else    g.fillEllipse (dot);

            g.setFont (f);
            g.drawText (b.getButtonText(), juce::Rectangle<float> (x0 + 34.0f, r.getY(), tw + 10.0f, r.getHeight()),
                        juce::Justification::centredLeft, false);
            return;
        }

        const float size = b.getProperties().contains ("fontSize") ? (float) b.getProperties()["fontSize"] : 12.5f;

        if (b.getProperties().contains ("icon"))
        {
            const auto colour = on ? accent : text;
            auto icon = r.withSizeKeepingCentre (26.0f, 26.0f).translated (0.0f, -9.0f);
            drawDeviceIcon (g, (int) b.getProperties()["icon"], icon, colour);
            g.setColour (colour);
            g.setFont (font (size, true, 0.14f));
            g.drawText (b.getButtonText(), r.withTrimmedTop (r.getHeight() * 0.58f).withTrimmedBottom (6.0f),
                        juce::Justification::centred, false);
            return;
        }

        g.setColour (on ? accent : (b.isEnabled() ? text : textDim));
        g.setFont (font (size, true, 0.16f));
        g.drawText (b.getButtonText(), r, juce::Justification::centred, false);
    }

    // ---------------- icone "Ascolta su" (tratto sottile, stile minimal)
    static void drawDeviceIcon (juce::Graphics& g, int which, juce::Rectangle<float> r, juce::Colour c)
    {
        const float s = r.getWidth() / 26.0f;
        auto P = [&r, s] (float x, float y) { return juce::Point<float> (r.getX() + x * s, r.getY() + y * s); };
        auto R = [&r, s] (float x, float y, float w, float h) { return juce::Rectangle<float> (r.getX() + x * s, r.getY() + y * s, w * s, h * s); };
        juce::Path p;

        switch (which)
        {
            case 0: // studio monitor
                p.addRoundedRectangle (R (6.0f, 2.5f, 14.0f, 21.0f), 2.5f * s);
                p.addEllipse (R (10.8f, 5.8f, 4.4f, 4.4f));
                p.addEllipse (R (9.0f, 12.5f, 8.0f, 8.0f));
                break;
            case 1: // telefono
                p.addRoundedRectangle (R (7.5f, 2.5f, 11.0f, 21.0f), 2.5f * s);
                p.startNewSubPath (P (11.0f, 20.0f)); p.lineTo (P (15.0f, 20.0f));
                break;
            case 2: // cassa bluetooth
                p.addRoundedRectangle (R (2.5f, 7.0f, 21.0f, 12.0f), 6.0f * s);
                p.addEllipse (R (6.0f, 10.5f, 5.0f, 5.0f));
                p.addEllipse (R (15.0f, 10.5f, 5.0f, 5.0f));
                break;
            case 3: // laptop
                p.addRoundedRectangle (R (5.0f, 5.0f, 16.0f, 11.0f), 1.5f * s);
                p.startNewSubPath (P (2.5f, 20.0f)); p.lineTo (P (23.5f, 20.0f));
                break;
            default: // auricolari
                p.addEllipse (R (4.5f, 4.5f, 7.0f, 7.0f));
                p.startNewSubPath (P (8.0f, 11.5f)); p.lineTo (P (8.0f, 21.5f));
                p.addEllipse (R (14.5f, 4.5f, 7.0f, 7.0f));
                p.startNewSubPath (P (18.0f, 11.5f)); p.lineTo (P (18.0f, 21.5f));
                break;
        }

        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.8f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ---------------- menu a tendina
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        using namespace scriba::ui;
        const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
        g.setColour (control);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (cardEdge);
        g.drawRoundedRectangle (r, 12.0f, 1.0f);

        juce::Path chevron;
        const float cx = (float) w - 26.0f, cy = (float) h * 0.5f;
        chevron.startNewSubPath (cx - 7.0f, cy - 3.5f);
        chevron.lineTo (cx, cy + 3.5f);
        chevron.lineTo (cx + 7.0f, cy - 3.5f);
        g.setColour (accent);
        g.strokePath (chevron, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return scriba::ui::font (16.0f, true); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (14, 1, box.getWidth() - 50, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Font getPopupMenuFont() override { return scriba::ui::font (14.5f, false); }

    void drawPopupMenuBackground (juce::Graphics& g, int, int) override
    {
        g.fillAll (scriba::ui::control);
    }
};
