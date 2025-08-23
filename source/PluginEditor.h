#pragma once
#include <JuceHeader.h>
#include "LFOShape.h" // single source of truth for the lane shape (namespace LFO)

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
    juce::Slider length; // outer
    juce::Slider curve;  // inner (-1..1)

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

        curve.toFront(false); // draw inner on top
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

// ---- Scope that can render from either UI shape OR a processor evaluator ---
struct ScopeTriangles : juce::Component
{
    explicit ScopeTriangles(int triangles = 2) : numTriangles(triangles)
    {
        setInterceptsMouseClicks(false, false);
    }

    void setNumTriangles(int n)
    {
        numTriangles = juce::jlimit(1, 16, n);
        repaint();
    }

    // Option 1: UI-driven shape preview (fallback if no evaluator is set)
    void setFromShape(const LFO::Shape &s, float phaseDeg)
    {
        shape = s;
        phase01 = juce::jlimit(0.0f, 1.0f, phaseDeg / 360.0f);
        repaint();
    }

    // Option 2: exact DSP preview — provide a function phase[0..1] -> value[-1..1]
    void setEvaluator(std::function<float(float)> fn)
    {
        evaluator = std::move(fn);
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

        // Midline
        g.setColour(grid.withAlpha(0.45f));
        g.drawLine({r.getX(), yMid, r.getRight(), yMid}, 1.0f);

        // Wave
        const float periods = 0.5f * (float)numTriangles; // 2 triangles = 1 period
        const int steps = juce::jmax(64, (int)r.getWidth());

        juce::Path p;
        bool first = true;

        for (int i = 0; i <= steps; ++i)
        {
            const float xNorm = (float)i / (float)steps; // 0..1 across width
            const float ph = std::fmod(phase01 + xNorm * periods, 1.0f);

            float yN = evaluator
                           ? juce::jlimit(-1.0f, 1.0f, evaluator(ph)) // DSP exact
                           : LFO::evalCycle(ph, shape);               // UI fallback

            const float x = r.getX() + xNorm * r.getWidth();
            const float y = yMid - yN * amp;

            if (first)
            {
                p.startNewSubPath(x, y);
                first = false;
            }
            else
            {
                p.lineTo(x, y);
            }
        }

        // Fill to midline
        juce::Path fill = p;
        fill.lineTo(r.getRight(), yMid);
        fill.lineTo(r.getX(), yMid);
        fill.closeSubPath();

        g.setColour(wave.withAlpha(0.22f));
        g.fillPath(fill);
        g.setColour(wave);
        g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    }

private:
    int numTriangles = 2;
    float phase01 = 0.0f;
    LFO::Shape shape{};
    std::function<float(float)> evaluator; // if set, used instead of shape
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
    // Update UI-driven scope preview (safe even if evaluator is set; it will just be ignored)
    void updateLane1Scope();

    PinkELFOntsAudioProcessor &processor;

    // Top bar
    juce::Label title;
    juce::ComboBox retrigBox, rangeBox;

    // Tabs
    juce::TabbedComponent laneTabs{juce::TabbedButtonBar::TabsAtTop};

    // Sections
    Section secOutput{"Output"};
    Section secLane1{""};        // tabs are the header
    Section secRandom{"Random"}; // kept for parity (hidden in layout)

    // Global
    Knob depthK{"Depth"};
    Knob phaseNudgeK{"Phase Nudge"};

    // Lane 1 (top row)
    juce::ToggleButton lane1Enabled{"On"};
    Knob mixK{"Mix"};
    Knob phaseK{"Phase"};
    Knob invertAK{"Invert A"};
    Knob invertBK{"Invert B"};

    // Lane 1 (shape rows)
    DualKnob riseA{"Rise A"};
    DualKnob fallA{"Fall A"};
    DualKnob riseB{"Rise B"};
    DualKnob fallB{"Fall B"};

    // Scopes
    ScopeTriangles lane1Scope2{2};
    ScopeTriangles randomScope3{3};

    // Random tab (placeholder)
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
    std::unique_ptr<SliderAtt> riseAAtt, fallAAtt, riseBAtt, fallBAtt;                     // lengths (outer)
    std::unique_ptr<SliderAtt> riseACurveAtt, fallACurveAtt, riseBCurveAtt, fallBCurveAtt; // curvature (inner)
    std::unique_ptr<SliderAtt> invertAAtt, invertBAtt;
    std::unique_ptr<SliderAtt> randomXfadeAtt, randomMixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PinkELFOntsAudioProcessorEditor)
};
