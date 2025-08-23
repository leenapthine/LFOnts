#pragma once
#include <JuceHeader.h>
class PinkELFOntsAudioProcessor;

// ---------- small UI helpers ----------

struct Section : juce::Component
{
    juce::String title;
    explicit Section(juce::String t = {}) : title(std::move(t)) {}

    void paint(juce::Graphics &g) override
    {
        auto bg = juce::Colour(0xFF141821);
        auto stroke = juce::Colour(0xFF262B38);

        g.setColour(bg);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 10.0f);

        g.setColour(stroke);
        g.drawRoundedRectangle(getLocalBounds().toFloat(), 10.0f, 1.0f);

        if (title.isNotEmpty())
        {
            g.setColour(juce::Colour(0xFF9AA7B8));
            g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
            g.drawText(title, getLocalBounds().removeFromTop(20).reduced(10, 0),
                       juce::Justification::left);
            g.setColour(stroke);
            g.fillRect(juce::Rectangle<int>(10, 26, getWidth() - 20, 1));
        }
    }
};

struct Knob : juce::Component
{
    juce::Label caption, value;
    juce::Slider slider; // rotary

    explicit Knob(juce::String captionText = {})
    {
        caption.setText(captionText, juce::dontSendNotification);
        caption.setJustificationType(juce::Justification::centred);
        caption.setColour(juce::Label::textColourId, juce::Colour(0xFF9AA7B8));
        addAndMakeVisible(caption);

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible(slider);

        value.setJustificationType(juce::Justification::centred);
        value.setInterceptsMouseClicks(false, false);
        value.setColour(juce::Label::textColourId, juce::Colour(0xFFE6EBF2));
        addAndMakeVisible(value);
        value.setVisible(false);

        slider.onValueChange = [this]
        { value.setText(juce::String(slider.getValue(), 2), juce::dontSendNotification); };
        slider.onDragStart = [this]
        { value.setVisible(true); };
        slider.onDragEnd = [this]
        { value.setVisible(false); };
    }

    void resized() override
    {
        auto r = getLocalBounds();
        caption.setBounds(r.removeFromTop(16));
        auto pill = r.removeFromBottom(18).reduced(10, 2);
        value.setBounds(pill);
        slider.setBounds(r.reduced(8));
    }
};

// ---- Dual-ring knob: outer = length, inner = curvature (-1..1) -------------
struct DualKnob : juce::Component
{
    juce::Label caption;
    juce::Slider length;
    juce::Slider curve;

    explicit DualKnob(juce::String text = {})
    {
        caption.setText(text, juce::dontSendNotification);
        caption.setJustificationType(juce::Justification::centred);
        caption.setColour(juce::Label::textColourId, juce::Colour(0xFF9AA7B8));
        addAndMakeVisible(caption);

        length.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        length.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        length.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible(length);

        curve.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        curve.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        curve.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible(curve);

        curve.toFront(false);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        caption.setBounds(r.removeFromTop(16));

        auto area = r.reduced(6);
        length.setBounds(area);

        // inner knob ~54% of outer
        auto inner = area.withSizeKeepingCentre(int(area.getWidth() * 0.54f),
                                                int(area.getHeight() * 0.54f));
        curve.setBounds(inner);
    }
};

// ---- Scope: reacts to A/B length, curvature & invert -----------------------
struct ScopeTriangles : juce::Component
{
    explicit ScopeTriangles(int triangles = 2) : num(triangles)
    {
        setInterceptsMouseClicks(false, false);
    }

    void setNumTriangles(int triangles)
    {
        num = juce::jlimit(1, 8, triangles);
        repaint();
    }

    // lengths: split of segment, curvature: -1..1 concave/convex
    // invert in [-1..1], 0 = normal peak, ±1 = fully inverted
    void setShape(float riseA_, float fallA_,
                  float riseB_, float fallB_,
                  float invA_, float invB_,
                  float curvRiseA_, float curvFallA_,
                  float curvRiseB_, float curvFallB_)
    {
        riseA = riseA_;
        fallA = fallA_;
        riseB = riseB_;
        fallB = fallB_;
        invA = invA_;
        invB = invB_;
        cRA = curvRiseA_;
        cFA = curvFallA_;
        cRB = curvRiseB_;
        cFB = curvFallB_;
        repaint();
    }

    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat().reduced(8.0f, 6.0f);
        if (r.isEmpty())
            return;

        const auto grid = findColour(juce::Slider::trackColourId);
        const auto wave = findColour(juce::Slider::thumbColourId);
        const float yMid = r.getCentreY();
        const float amp = r.getHeight() * 0.36f;

        g.setColour(grid.withAlpha(0.45f));
        g.drawLine({r.getX(), yMid, r.getRight(), yMid}, 1.0f);

        auto drawCurvedTri = [&](float x0, float x1, bool up,
                                 float rise, float fall,
                                 float curvRise, float curvFall,
                                 float inv)
        {
            const float segW = x1 - x0;
            float tRise = rise / juce::jmax(0.0001f, (rise + fall));
            tRise = juce::jlimit(0.10f, 0.90f, tRise);
            const float xApex = x0 + segW * tRise;

            const float sign = up ? -1.0f : +1.0f;
            const float mul = std::cos(juce::jlimit(-1.0f, 1.0f, inv) * juce::MathConstants<float>::pi);
            const float yApex = yMid + sign * amp * mul;

            juce::Point<float> P0{x0, yMid};
            juce::Point<float> P1{xApex, yApex};
            juce::Point<float> P2{x1, yMid};

            auto ctrl = [&](juce::Point<float> A, juce::Point<float> B, float curv)
            {
                auto M = (A + B) * 0.5f;
                auto D = B - A;
                auto N = juce::Point<float>{-D.y, D.x};
                float L = std::sqrt(N.x * N.x + N.y * N.y);
                if (L > 0.0001f)
                    N = {N.x / L, N.y / L};

                // ↑ stronger curvature than before
                const float k = 0.65f; // was 0.45
                const float mag = k * std::min(std::abs(D.x), amp);
                float c = juce::jlimit(-1.0f, 1.0f, curv);
                c = c * std::abs(c) * 1.25f; // stronger easing
                return M + N * (c * mag);
            };

            juce::Path p;
            p.startNewSubPath(P0);
            p.quadraticTo(ctrl(P0, P1, curvRise), P1);
            p.quadraticTo(ctrl(P1, P2, curvFall), P2);
            p.lineTo(P0);

            g.setColour(wave.withAlpha(0.22f));
            g.fillPath(p);

            g.setColour(wave);
            g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        };

        const float segW = r.getWidth() / (float)juce::jmax(1, num);
        for (int i = 0; i < num; ++i)
        {
            const float x0 = r.getX() + i * segW;
            const float x1 = x0 + segW;
            const bool up = (i % 2 == 0);
            if (up)
                drawCurvedTri(x0, x1, true, riseA, fallA, cRA, cFA, invA);
            else
                drawCurvedTri(x0, x1, false, riseB, fallB, cRB, cFB, invB);
        }
    }

private:
    int num = 2;
    float riseA = 1.f, fallA = 1.f, riseB = 1.f, fallB = 1.f;
    float invA = 0.f, invB = 0.f;
    float cRA = 0.f, cFA = 0.f, cRB = 0.f, cFB = 0.f;
};

// --------------- main editor ---------------

class PinkELFOntsAudioProcessorEditor
    : public juce::AudioProcessorEditor,
      public juce::ChangeListener
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    explicit PinkELFOntsAudioProcessorEditor(PinkELFOntsAudioProcessor &);
    ~PinkELFOntsAudioProcessorEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;

    void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
    void updateLane1Scope();

    PinkELFOntsAudioProcessor &processor;

    // Top bar
    juce::Label title;
    juce::ComboBox retrigBox, rangeBox;

    // Tabs
    juce::TabbedComponent laneTabs{juce::TabbedButtonBar::TabsAtTop};

    // Sections
    Section secOutput{"Output"};
    Section secLane1{""};
    Section secRandom{"Random"};

    // Global
    Knob depthK{"Depth"};
    Knob phaseNudgeK{"Phase Nudge"};

    // Lane 1
    juce::ToggleButton lane1Enabled{"On"};
    Knob mixK{"Mix"};
    Knob phaseK{"Phase"};
    Knob invertAK{"Invert A"};
    Knob invertBK{"Invert B"};

    DualKnob riseA{"Rise A"};
    DualKnob fallA{"Fall A"};
    DualKnob riseB{"Rise B"};
    DualKnob fallB{"Fall B"};

    ScopeTriangles lane1Scope2{2};
    ScopeTriangles randomScope3{3};

    // Random
    juce::ToggleButton randomEnabled{"On"};
    juce::ComboBox randomRate;
    Knob randomXfadeK{"Xfade (ms)"};
    Knob randomMixK{"Mix"};

    // APVTS attachments
    using SliderAtt = APVTS::SliderAttachment;
    using ButtonAtt = APVTS::ButtonAttachment;
    using ComboAtt = APVTS::ComboBoxAttachment;

    std::unique_ptr<ComboAtt> retrigAtt, rangeAtt, randomRateAtt;
    std::unique_ptr<SliderAtt> depthAtt, phaseNudgeAtt;

    std::unique_ptr<ButtonAtt> lane1OnAtt, randomOnAtt;

    std::unique_ptr<SliderAtt> mixAtt, phaseAtt;
    std::unique_ptr<SliderAtt> riseAAtt, fallAAtt, riseBAtt, fallBAtt;
    std::unique_ptr<SliderAtt> riseACurveAtt, fallACurveAtt, riseBCurveAtt, fallBCurveAtt;
    std::unique_ptr<SliderAtt> invertAAtt, invertBAtt;
    std::unique_ptr<SliderAtt> randomXfadeAtt, randomMixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PinkELFOntsAudioProcessorEditor)
};
