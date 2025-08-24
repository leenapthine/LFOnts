#pragma once
#include <JuceHeader.h>

struct PinkLookAndFeel : juce::LookAndFeel_V4
{
    PinkLookAndFeel()
    {
        using C = juce::Colour;
        setColour(juce::ResizableWindow::backgroundColourId, C::fromString("0xFF0B0D10"));
        setColour(juce::Slider::thumbColourId, C::fromString("0xFFFF4FA3"));
        setColour(juce::Slider::trackColourId, C::fromString("0xFF262B38"));
        setColour(juce::Slider::backgroundColourId, C::fromString("0xFF141821"));
        setColour(juce::ComboBox::backgroundColourId, C::fromString("0xFF141821"));
        setColour(juce::ComboBox::textColourId, C::fromString("0xFFE6EBF2"));
        setColour(juce::Label::textColourId, C::fromString("0xFFE6EBF2"));
        setColour(juce::TextButton::buttonColourId, C::fromString("0xFF141821"));
        setColour(juce::TextButton::textColourOnId, C::fromString("0xFFE6EBF2"));
        setColour(juce::TextButton::textColourOffId, C::fromString("0xFFE6EBF2"));
    }

    // Flat linear slider (supports vertical & horizontal)
    void drawLinearSlider(juce::Graphics &g, int x, int y, int w, int h,
                          float sliderPos, float min, float max,
                          const juce::Slider::SliderStyle style, juce::Slider &s) override
    {
        auto track = findColour(juce::Slider::trackColourId);
        auto back = findColour(juce::Slider::backgroundColourId);
        auto thumb = findColour(juce::Slider::thumbColourId);

        const bool isVertical = (style == juce::Slider::LinearVertical) || (h > w * 1.2f);

        if (isVertical)
        {
            auto r = juce::Rectangle<float>(float(x), float(y), float(w), float(h)).reduced(8, 10);
            // background rail
            g.setColour(back);
            g.fillRoundedRectangle(r.withWidth(6.0f).withCentre({r.getCentreX(), r.getCentreY()}), 3.0f);

            // filled amount
            const float t = juce::jlimit(0.0f, 1.0f, (sliderPos - min) / (max - min));
            auto filled = r.removeFromBottom(r.getHeight() * t);
            g.setColour(track);
            g.fillRoundedRectangle(filled.withWidth(6.0f).withCentre({r.getCentreX(), filled.getCentreY()}), 3.0f);

            // thumb cap
            juce::Rectangle<float> thumbR(14.0f, 22.0f);
            thumbR.setCentre(r.getCentreX(), filled.getY());
            g.setColour(thumb);
            g.fillRoundedRectangle(thumbR, 7.0f);
            return;
        }

        // ---- Horizontal (unchanged look) ----
        auto r = juce::Rectangle<float>(float(x), float(y) + h * 0.5f - 3.0f, float(w), 6.0f);
        auto t = juce::jlimit(0.0f, 1.0f, (sliderPos - min) / (max - min));
        auto rw = r.withWidth(r.getWidth() * t);

        g.setColour(back);
        g.fillRoundedRectangle(r, 3.0f);

        g.setColour(track);
        g.fillRoundedRectangle(rw, 3.0f);

        g.setColour(thumb);
        g.fillEllipse(rw.getRight() - 6.0f, r.getCentreY() - 6.0f, 12.0f, 12.0f);
    }

    // Rotary knob with progress ring + dot
    void drawRotarySlider(juce::Graphics &g, int x, int y, int w, int h,
                          float pos, float a0, float a1, juce::Slider &) override
    {
        auto b = juce::Rectangle<float>(float(x), float(y), float(w), float(h)).reduced(4);
        auto r = b.getWidth() < b.getHeight() ? b.withSizeKeepingCentre(b.getWidth(), b.getWidth())
                                              : b.withSizeKeepingCentre(b.getHeight(), b.getHeight());

        auto track = findColour(juce::Slider::trackColourId);
        auto back = findColour(juce::Slider::backgroundColourId);
        auto thumb = findColour(juce::Slider::thumbColourId);

        // face
        g.setColour(back.darker(0.30f));
        g.fillEllipse(r);

        // progress arc
        juce::Path p;
        const float angle = a0 + pos * (a1 - a0);
        p.addCentredArc(r.getCentreX(), r.getCentreY(),
                        r.getWidth() * 0.48f, r.getHeight() * 0.48f,
                        0.0f, a0, angle, true);

        g.setColour(track);
        g.strokePath(p, juce::PathStrokeType(juce::jmax(2.0f, r.getWidth() * 0.10f),
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

        // thumb dot (manual rotation; Point<float>::rotated() not available in some JUCE versions)
        const float radius = r.getWidth() * 0.40f;
        const float dx = std::cos(angle) * radius;
        const float dy = std::sin(angle) * radius;
        auto centre = r.getCentre();

        juce::Rectangle<float> dot(10.0f, 10.0f);
        dot.setCentre(centre.x + dx, centre.y + dy);

        g.setColour(thumb);
        g.fillEllipse(dot);
    }

    void drawToggleButton(juce::Graphics &g, juce::ToggleButton &b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        auto on = b.getToggleState();

        auto box = r.removeFromLeft(18).reduced(2);
        g.setColour(findColour(juce::Slider::backgroundColourId));
        g.fillRoundedRectangle(box, 3.0f);
        if (on)
        {
            g.setColour(findColour(juce::Slider::thumbColourId));
            g.fillEllipse(box.reduced(3));
        }

        g.setColour(findColour(juce::Label::textColourId));
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText(b.getButtonText(), r.reduced(4, 0), juce::Justification::centredLeft);
    }

    void drawTabButton(juce::TabBarButton &button,
                       juce::Graphics &g,
                       bool isMouseOver, bool /*isMouseDown*/) override
    {
        auto area = button.getLocalBounds().toFloat().reduced(2.0f, 3.0f);
        const bool front = button.isFrontTab();

        const auto pink = juce::Colour(0xFFFF4FA3);
        const auto bgDark = juce::Colour(0xFF0B0D10);
        const auto stroke = juce::Colour(0xFF262B38);
        const auto textOn = juce::Colour(0xFF0B0D10);  // dark text on pink
        const auto textOff = juce::Colour(0xFFE6EBF2); // light text on dark
        const float radius = 8.0f;

        if (front)
        {
            float alpha = isMouseOver ? 0.98f : 0.90f;
            g.setColour(pink.withAlpha(alpha));
            g.fillRoundedRectangle(area, radius);

            g.setColour(pink.darker(0.20f));
            g.drawRoundedRectangle(area, radius, 1.6f);
        }
        else
        {
            float alpha = isMouseOver ? 0.80f : 0.65f;
            g.setColour(bgDark.withAlpha(alpha));
            g.fillRoundedRectangle(area, radius);

            g.setColour(stroke);
            g.drawRoundedRectangle(area, radius, 1.0f);
        }

        g.setColour(front ? textOn : textOff);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold))); // avoid deprecated ctor
        g.drawFittedText(button.getButtonText(), button.getLocalBounds(),
                         juce::Justification::centred, 1);
    }
};
