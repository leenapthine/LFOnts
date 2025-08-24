#pragma once
#include <JuceHeader.h>
#include <array>
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

// ---- Dual-ring knob: outer = length, inner = curvature (0..1) -------------
struct DualKnob : juce::Component
{
    juce::Label caption;
    juce::Slider length; // outer
    juce::Slider curve;  // inner (0..1)

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

// ---- Switch matrix (L1..L9 + Random) --------------------------------------
struct SwitchMatrix : juce::Component
{
    juce::ToggleButton l1{"L1"}, l2{"L2"}, l3{"L3"}, l4{"L4"}, l5{"L5"},
        l6{"L6"}, l7{"L7"}, l8{"L8"}, l9{"L9"}, random{"Random"};

    std::array<juce::ToggleButton *, 10> all{&l1, &l2, &l3, &l4, &l5, &l6, &l7, &l8, &l9, &random};

    SwitchMatrix()
    {
        for (auto *b : all)
            addAndMakeVisible(*b);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int rowH = 28;
        const int gapX = 16;
        const int gapY = 10;
        const int cols = 5;
        const int cellW = (r.getWidth() - gapX * (cols - 1)) / cols;

        auto placeRow = [&](int startIdx)
        {
            auto row = r.removeFromTop(rowH);
            for (int i = 0; i < cols; ++i)
            {
                auto *b = all[(size_t)startIdx + (size_t)i];
                if (!b)
                    continue;
                b->setBounds(row.removeFromLeft(cellW));
                if (i < cols - 1)
                    row.removeFromLeft(gapX);
            }
        };

        placeRow(0); // L1..L5
        r.removeFromTop(gapY);
        placeRow(5); // L6..L9 + Random
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
    void setABTripletMode(bool on)
    {
        abTripletMode = on;
        repaint();
    }

    void setFromShape(const LFO::Shape &s, float phaseDeg)
    {
        shape = s;
        phase01 = juce::jlimit(0.0f, 1.0f, phaseDeg / 360.0f);
        repaint();
    }

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

        // baseline lower (closer to the bottom) + taller triangles
        const float yBase = r.getBottom() - r.getHeight() * 0.24f; // ~20% above bottom
        const float amp = r.getHeight() * 0.65f;                   // peaks reach higher

        // Baseline guide
        g.setColour(grid.withAlpha(0.45f));
        g.drawLine({r.getX(), yBase, r.getRight(), yBase}, 1.0f);

        // ABB triplet branch (static triangles, unipolar 0..1 visual)
        if (abTripletMode && numTriangles == 3 && !evaluator)
        {
            const float rA = juce::jmax(0.0001f, shape.riseA);
            const float fA = juce::jmax(0.0001f, shape.fallA);
            const float rB = juce::jmax(0.0001f, shape.riseB);
            const float fB = juce::jmax(0.0001f, shape.fallB);

            const float apexFracA = (rA + fA > 0.0f) ? (rA / (rA + fA)) : 0.5f;
            const float apexFracB = (rB + fB > 0.0f) ? (rB / (rB + fB)) : 0.5f;

            const float slotW = r.getWidth() / 3.0f;

            auto drawTri = [&](int slot, float frac, juce::Colour stroke, juce::Colour fill)
            {
                const float xL = r.getX() + slotW * (float)slot;
                const float xR = xL + slotW;
                const float xA = xL + frac * slotW;

                const float baseY = yBase;
                const float peakY = yBase - amp;

                juce::Path p;
                p.startNewSubPath(xL, baseY);
                p.lineTo(xA, peakY);
                p.lineTo(xR, baseY);

                juce::Path fillP = p;
                fillP.lineTo(xL, baseY);
                fillP.closeSubPath();

                g.setColour(fill);
                g.fillPath(fillP);
                g.setColour(stroke);
                g.strokePath(p, juce::PathStrokeType(1.5f));
            };

            auto colAStroke = wave.withHue(0.56f).withAlpha(0.95f);
            auto colAFill = colAStroke.withAlpha(0.18f);
            auto colBStroke = wave.withHue(0.86f).withAlpha(0.95f);
            auto colBFill = colBStroke.withAlpha(0.18f);

            // A, B, B
            drawTri(0, apexFracA, colAStroke, colAFill);
            drawTri(1, apexFracB, colBStroke, colBFill);
            drawTri(2, apexFracB, colBStroke, colBFill);
            return;
        }

        // If an evaluator is set, draw exactly one full cycle (0..1).
        // Otherwise (static preview), show triangles based on numTriangles.
        const float periods = (evaluator ? 1.0f : 0.5f * (float)numTriangles);

        const int steps = juce::jmax(64, (int)r.getWidth());

        juce::Path p;
        bool first = true;

        for (int i = 0; i <= steps; ++i)
        {
            const float xNorm = (float)i / (float)steps; // 0..1 across width
            const float ph = std::fmod(phase01 + xNorm * periods, 1.0f);

            float yN = evaluator ? juce::jlimit(0.0f, 1.0f, evaluator(ph))
                                 : LFO::evalCycle(ph, shape); // unipolar 0..1

            const float x = r.getX() + xNorm * r.getWidth();
            const float y = yBase - yN * amp;

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

        juce::Path fill = p;
        fill.lineTo(r.getRight(), yBase);
        fill.lineTo(r.getX(), yBase);
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
    bool abTripletMode = false;            // NEW
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
    void updateLane2Scope();
    void updateLane3Scope();
    void updateLane4Scope();
    void updateLane5Scope();
    void updateLane6Scope();

    // helper
    bool paramExists(const juce::String &id) const;

    PinkELFOntsAudioProcessor &processor;

    // Top bar
    juce::Label title;
    juce::ComboBox retrigBox;

    // Tabs
    juce::TabbedComponent laneTabs{juce::TabbedButtonBar::TabsAtTop};

    // Sections
    Section secOutput{"Output"};
    Section secLane{""};

    // APVTS attachments
    using SliderAtt = APVTS::SliderAttachment;
    using ButtonAtt = APVTS::ButtonAttachment;
    using ComboAtt = APVTS::ComboBoxAttachment;

    // Mixer (top-right)
    Section secMixer{"Mixer"};
    std::array<juce::Slider, 8> mixerFader;
    std::array<std::unique_ptr<SliderAtt>, 8> mixerFaderAtt;
    std::array<juce::ToggleButton, 8> mixerOn;
    std::array<std::unique_ptr<ButtonAtt>, 8> mixerOnAtt;
    std::array<juce::Label, 8> mixerLbl;

    // Global
    Knob depthK{"Depth"};
    Knob phaseNudgeK{"Phase Nudge"};

    // Lane 1 controls
    Knob mixK1{"Mix"};
    Knob phaseK1{"Phase"};
    Knob invertA1{"Invert A"};
    Knob invertB1{"Invert B"};
    DualKnob riseA1{"Rise A"};
    DualKnob fallA1{"Fall A"};
    DualKnob riseB1{"Rise B"};
    DualKnob fallB1{"Fall B"};

    // Lane 2 controls (its own knobs)
    Knob mixK2{"Mix"};
    Knob phaseK2{"Phase"};
    Knob invertA2{"Invert A"};
    Knob invertB2{"Invert B"};
    DualKnob riseA2{"Rise A"};
    DualKnob fallA2{"Fall A"};
    DualKnob riseB2{"Rise B"};
    DualKnob fallB2{"Fall B"};

    // Lane 3 controls
    Knob mixK3{"Mix"};
    Knob phaseK3{"Phase"};
    Knob invertA3{"Invert A"};
    Knob invertB3{"Invert B"};
    DualKnob riseA3{"Rise A"};
    DualKnob fallA3{"Fall A"};
    DualKnob riseB3{"Rise B"};
    DualKnob fallB3{"Fall B"};

    // Lane 4 controls (its own knobs)
    Knob mixK4{"Mix"};
    Knob phaseK4{"Phase"};
    Knob invertA4{"Invert A"};
    Knob invertB4{"Invert B"};
    DualKnob riseA4{"Rise A"};
    DualKnob fallA4{"Fall A"};
    DualKnob riseB4{"Rise B"};
    DualKnob fallB4{"Fall B"};

    // Lane 5 controls
    Knob mixK5{"Mix"};
    Knob phaseK5{"Phase"};
    Knob invertA5{"Invert A"};
    Knob invertB5{"Invert B"};
    DualKnob riseA5{"Rise A"};
    DualKnob fallA5{"Fall A"};
    DualKnob riseB5{"Rise B"};
    DualKnob fallB5{"Fall B"};

    // Lane 6 controls (its own knobs)
    Knob mixK6{"Mix"};
    Knob phaseK6{"Phase"};
    Knob invertA6{"Invert A"};
    Knob invertB6{"Invert B"};
    DualKnob riseA6{"Rise A"};
    DualKnob fallA6{"Fall A"};
    DualKnob riseB6{"Rise B"};
    DualKnob fallB6{"Fall B"};

    // Scopes
    ScopeTriangles lane1Scope2{2};
    ScopeTriangles lane2Scope3{3}; // Lane-2 triplet scope (A-B-B)
    ScopeTriangles lane3Scope2{2};
    ScopeTriangles lane4Scope3{3}; // Lane-4 triplet scope (A-B-B)
    ScopeTriangles lane5Scope2{2};
    ScopeTriangles lane6Scope3{3}; // Lane-6 triplet scope (A-B-B)
    ScopeTriangles randomScope3{3};

    // Random tab (placeholder)
    juce::ComboBox randomRate;
    Knob randomXfadeK{"Xfade (ms)"};
    Knob randomMixK{"Mix"};

    std::unique_ptr<ComboAtt> retrigAtt;
    std::unique_ptr<SliderAtt> depthAtt, phaseNudgeAtt;

    // Lane 1 attaches
    std::unique_ptr<SliderAtt> mix1Att, phase1Att;
    std::unique_ptr<SliderAtt> riseA1Att, fallA1Att, riseB1Att, fallB1Att;                     // lengths
    std::unique_ptr<SliderAtt> riseA1CurveAtt, fallA1CurveAtt, riseB1CurveAtt, fallB1CurveAtt; // curvature
    std::unique_ptr<SliderAtt> invertA1Att, invertB1Att;

    // Lane 2 attaches (created only if params exist)
    std::unique_ptr<SliderAtt> mix2Att, phase2Att;
    std::unique_ptr<SliderAtt> riseA2Att, fallA2Att, riseB2Att, fallB2Att;
    std::unique_ptr<SliderAtt> riseA2CurveAtt, fallA2CurveAtt, riseB2CurveAtt, fallB2CurveAtt;
    std::unique_ptr<SliderAtt> invertA2Att, invertB2Att;

    // Lane 3 attaches
    std::unique_ptr<SliderAtt> mix3Att, phase3Att;
    std::unique_ptr<SliderAtt> riseA3Att, fallA3Att, riseB3Att, fallB3Att;                     // lengths
    std::unique_ptr<SliderAtt> riseA3CurveAtt, fallA3CurveAtt, riseB3CurveAtt, fallB3CurveAtt; // curvature
    std::unique_ptr<SliderAtt> invertA3Att, invertB3Att;

    // Lane 4 attaches (created only if params exist)
    std::unique_ptr<SliderAtt> mix4Att, phase4Att;
    std::unique_ptr<SliderAtt> riseA4Att, fallA4Att, riseB4Att, fallB4Att;
    std::unique_ptr<SliderAtt> riseA4CurveAtt, fallA4CurveAtt, riseB4CurveAtt, fallB4CurveAtt;
    std::unique_ptr<SliderAtt> invertA4Att, invertB4Att;

    // Lane 5 attaches
    std::unique_ptr<SliderAtt> mix5Att, phase5Att;
    std::unique_ptr<SliderAtt> riseA5Att, fallA5Att, riseB5Att, fallB5Att;                     // lengths
    std::unique_ptr<SliderAtt> riseA5CurveAtt, fallA5CurveAtt, riseB5CurveAtt, fallB5CurveAtt; // curvature
    std::unique_ptr<SliderAtt> invertA5Att, invertB5Att;

    // Lane 6 attaches (created only if params exist)
    std::unique_ptr<SliderAtt> mix6Att, phase6Att;
    std::unique_ptr<SliderAtt> riseA6Att, fallA6Att, riseB6Att, fallB6Att;
    std::unique_ptr<SliderAtt> riseA6CurveAtt, fallA6CurveAtt, riseB6CurveAtt, fallB6CurveAtt;
    std::unique_ptr<SliderAtt> invertA6Att, invertB6Att;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PinkELFOntsAudioProcessorEditor)
};