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

// ---- Switch matrix (L1..L9) --------------------------------------
struct SwitchMatrix : juce::Component
{
    juce::ToggleButton l1{"L1"}, l2{"L2"}, l3{"L3"}, l4{"L4"}, l5{"L5"},
        l6{"L6"}, l7{"L7"}, l8{"L8"}, l9{"L9"};

    std::array<juce::ToggleButton *, 10> all{&l1, &l2, &l3, &l4, &l5, &l6, &l7, &l8, &l9};

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
        placeRow(5); // L6..L9
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

    // --- optional overlay (e.g., output slope/curve) ------------------------
    void setOverlayEvaluator(std::function<float(float)> fn, juce::Colour c)
    {
        overlayEval = std::move(fn);
        overlayColour = c;
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

        // --- overlay line (e.g., slope/curve hint) --------------------------
        if (overlayEval)
        {
            juce::Path hint;
            const int n = juce::jmax(128, (int)(r.getWidth()));
            for (int i = 0; i <= n; ++i)
            {
                const float xNorm = (float)i / (float)n; // 0..1
                const float ph = std::fmod(phase01 + xNorm * 1.0f, 1.0f);
                const float yN = juce::jlimit(0.0f, 1.0f, overlayEval(ph));
                const float x = r.getX() + xNorm * r.getWidth();
                const float y = yBase - yN * amp;
                (i == 0 ? hint.startNewSubPath(x, y) : hint.lineTo(x, y));
            }
            g.setColour(overlayColour);
            g.strokePath(hint, juce::PathStrokeType(2.0f));
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
    std::function<float(float)> overlayEval;
    juce::Colour overlayColour{juce::Colours::transparentBlack};
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
    void updateLane7Scope();
    void updateLane8Scope();

    // helper
    bool paramExists(const juce::String &id) const;

    PinkELFOntsAudioProcessor &processor;

    // Top bar
    juce::Label title;
    juce::ComboBox retrigBox;
    juce::ComboBox rateBox;

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
    DualKnob slopeK{"Slope / Curve"};

    // Lane 1 controls
    Knob phaseK1{"Phase"};
    Knob invertA1{"Invert A"};
    Knob invertB1{"Invert B"};
    DualKnob timeA1{"Time A / Curve 1"};
    DualKnob timeB1{"Time B / Curve 2"};
    DualKnob intensityA1{"Intensity A / Curve 3"};
    DualKnob intensityB1{"Intensity B / Curve 4"};

    // Lane 2 controls (its own knobs)
    Knob phaseK2{"Phase"};
    Knob invertA2{"Invert A"};
    Knob invertB2{"Invert B"};
    DualKnob timeA2{"Time A / Curve 1"};
    DualKnob timeB2{"Time B / Curve 2"};
    DualKnob intensityA2{"Intensity A / Curve 3"};
    DualKnob intensityB2{"Intensity B / Curve 4"};

    // Lane 3 controls
    Knob phaseK3{"Phase"};
    Knob invertA3{"Invert A"};
    Knob invertB3{"Invert B"};
    DualKnob timeA3{"Time A / Curve 1"};
    DualKnob timeB3{"Time B / Curve 2"};
    DualKnob intensityA3{"Intensity A / Curve 3"};
    DualKnob intensityB3{"Intensity B / Curve 4"};

    // Lane 4 controls (its own knobs)
    Knob phaseK4{"Phase"};
    Knob invertA4{"Invert A"};
    Knob invertB4{"Invert B"};
    DualKnob timeA4{"Time A / Curve 1"};
    DualKnob timeB4{"Time B / Curve 2"};
    DualKnob intensityA4{"Intensity A / Curve 3"};
    DualKnob intensityB4{"Intensity B / Curve 4"};

    // Lane 5 controls
    Knob phaseK5{"Phase"};
    Knob invertA5{"Invert A"};
    Knob invertB5{"Invert B"};
    DualKnob timeA5{"Time A / Curve 1"};
    DualKnob timeB5{"Time B / Curve 2"};
    DualKnob intensityA5{"Intensity A / Curve 3"};
    DualKnob intensityB5{"Intensity B / Curve 4"};

    // Lane 6 controls (its own knobs)
    Knob phaseK6{"Phase"};
    Knob invertA6{"Invert A"};
    Knob invertB6{"Invert B"};
    DualKnob timeA6{"Time A / Curve 1"};
    DualKnob timeB6{"Time B / Curve 2"};
    DualKnob intensityA6{"Intensity A / Curve 3"};
    DualKnob intensityB6{"Intensity B / Curve 4"};

    // Lane 7 (1/32)
    Knob phaseK7{"Phase"};
    Knob invertA7{"Invert A"};
    Knob invertB7{"Invert B"};
    DualKnob timeA7{"Time A / Curve 1"};
    DualKnob timeB7{"Time B / Curve 2"};
    DualKnob intensityA7{"Intensity A"};
    DualKnob intensityB7{"Intensity B"};

    // Lane 8 (1/32T)
    Knob phaseK8{"Phase"};
    Knob invertA8{"Invert A"};
    Knob invertB8{"Invert B"};
    DualKnob timeA8{"Time A / Curve 1"};
    DualKnob timeB8{"Time B / Curve 2"};
    DualKnob intensityA8{"Intensity A"};
    DualKnob intensityB8{"Intensity B"};

    // Scopes
    ScopeTriangles lane1Scope2{2};
    ScopeTriangles lane2Scope3{3}; // Lane-2 triplet scope (A-B-B)
    ScopeTriangles lane3Scope2{2};
    ScopeTriangles lane4Scope3{3}; // Lane-4 triplet scope (A-B-B)
    ScopeTriangles lane5Scope2{2};
    ScopeTriangles lane6Scope3{3}; // Lane-6 triplet scope (A-B-B)
    ScopeTriangles lane7Scope2{2};
    ScopeTriangles lane8Scope3{3};

    // Output-card mixed scope
    ScopeTriangles outputMixScope{2}; // reuse your scope; 2 triangles look fits the motif
    void updateOutputMixScope();      // helper to repaint when things change

    // Global Attachments
    std::unique_ptr<ComboAtt> retrigAtt;
    std::unique_ptr<SliderAtt> depthAtt, phaseNudgeAtt;
    std::unique_ptr<SliderAtt> slopeLenAtt, slopeCurveAtt;
    std::unique_ptr<ComboAtt> rateAtt;

    // Lane 1 attaches
    std::unique_ptr<SliderAtt> phase1Att, invertA1Att, invertB1Att;

    // Time A drives riseA/fallA length + both curves
    std::unique_ptr<SliderAtt> timeA1LenAtt, timeA1LenFallAtt;
    std::unique_ptr<SliderAtt> timeA1CurveRiseAtt, timeA1CurveFallAtt;

    // Time B drives riseB/fallB length + both curves
    std::unique_ptr<SliderAtt> timeB1LenAtt, timeB1LenFallAtt;
    std::unique_ptr<SliderAtt> timeB1CurveRiseAtt, timeB1CurveFallAtt;

    // Intensities + their inner curve rings
    std::unique_ptr<SliderAtt> intensityA1LenAtt, intensityA1CurveAtt;
    std::unique_ptr<SliderAtt> intensityB1LenAtt, intensityB1CurveAtt;

    // Lane 2 attaches (created only if params exist)
    std::unique_ptr<SliderAtt> phase2Att, invertA2Att, invertB2Att;

    // Time A drives riseA/fallA length + both curves
    std::unique_ptr<SliderAtt> timeA2LenAtt, timeA2LenFallAtt;
    std::unique_ptr<SliderAtt> timeA2CurveRiseAtt, timeA2CurveFallAtt;

    // Time B drives riseB/fallB length + both curves
    std::unique_ptr<SliderAtt> timeB2LenAtt, timeB2LenFallAtt;
    std::unique_ptr<SliderAtt> timeB2CurveRiseAtt, timeB2CurveFallAtt;

    // Intensities + their inner curve rings
    std::unique_ptr<SliderAtt> intensityA2LenAtt, intensityA2CurveAtt;
    std::unique_ptr<SliderAtt> intensityB2LenAtt, intensityB2CurveAtt;

    // Lane 3 attaches
    std::unique_ptr<SliderAtt> phase3Att, invertA3Att, invertB3Att;

    // Time A drives riseA/fallA length + both curves
    std::unique_ptr<SliderAtt> timeA3LenAtt, timeA3LenFallAtt;
    std::unique_ptr<SliderAtt> timeA3CurveRiseAtt, timeA3CurveFallAtt;

    // Time B drives riseB/fallB length + both curves
    std::unique_ptr<SliderAtt> timeB3LenAtt, timeB3LenFallAtt;
    std::unique_ptr<SliderAtt> timeB3CurveRiseAtt, timeB3CurveFallAtt;

    // Intensities + their inner curve rings
    std::unique_ptr<SliderAtt> intensityA3LenAtt, intensityA3CurveAtt;
    std::unique_ptr<SliderAtt> intensityB3LenAtt, intensityB3CurveAtt;

    // Lane 4 attaches (created only if params exist)
    std::unique_ptr<SliderAtt> phase4Att, invertA4Att, invertB4Att;

    // Time A drives riseA/fallA length + both curves
    std::unique_ptr<SliderAtt> timeA4LenAtt, timeA4LenFallAtt;
    std::unique_ptr<SliderAtt> timeA4CurveRiseAtt, timeA4CurveFallAtt;

    // Time B drives riseB/fallB length + both curves
    std::unique_ptr<SliderAtt> timeB4LenAtt, timeB4LenFallAtt;
    std::unique_ptr<SliderAtt> timeB4CurveRiseAtt, timeB4CurveFallAtt;

    // Intensities + their inner curve rings
    std::unique_ptr<SliderAtt> intensityA4LenAtt, intensityA4CurveAtt;
    std::unique_ptr<SliderAtt> intensityB4LenAtt, intensityB4CurveAtt;

    // Lane 5 attaches
    std::unique_ptr<SliderAtt> phase5Att, invertA5Att, invertB5Att;

    // Time A drives riseA/fallA length + both curves
    std::unique_ptr<SliderAtt> timeA5LenAtt, timeA5LenFallAtt;
    std::unique_ptr<SliderAtt> timeA5CurveRiseAtt, timeA5CurveFallAtt;

    // Time B drives riseB/fallB length + both curves
    std::unique_ptr<SliderAtt> timeB5LenAtt, timeB5LenFallAtt;
    std::unique_ptr<SliderAtt> timeB5CurveRiseAtt, timeB5CurveFallAtt;

    // Intensities + their inner curve rings
    std::unique_ptr<SliderAtt> intensityA5LenAtt, intensityA5CurveAtt;
    std::unique_ptr<SliderAtt> intensityB5LenAtt, intensityB5CurveAtt;

    // Lane 6 attaches (created only if params exist)
    std::unique_ptr<SliderAtt> phase6Att, invertA6Att, invertB6Att;

    // Time A drives riseA/fallA length + both curves
    std::unique_ptr<SliderAtt> timeA6LenAtt, timeA6LenFallAtt;
    std::unique_ptr<SliderAtt> timeA6CurveRiseAtt, timeA6CurveFallAtt;

    // Time B drives riseB/fallB length + both curves
    std::unique_ptr<SliderAtt> timeB6LenAtt, timeB6LenFallAtt;
    std::unique_ptr<SliderAtt> timeB6CurveRiseAtt, timeB6CurveFallAtt;

    // Intensities + their inner curve rings
    std::unique_ptr<SliderAtt> intensityA6LenAtt, intensityA6CurveAtt;
    std::unique_ptr<SliderAtt> intensityB6LenAtt, intensityB6CurveAtt;

    // Lane 7 and 9 attachments

    std::unique_ptr<SliderAtt> phase7Att, invertA7Att, invertB7Att;
    std::unique_ptr<SliderAtt> timeA7LenAtt, timeA7LenFallAtt, timeA7CurveRiseAtt;
    std::unique_ptr<SliderAtt> timeB7LenAtt, timeB7LenFallAtt, timeB7CurveRiseAtt;
    std::unique_ptr<SliderAtt> intensityA7LenAtt, intensityA7CurveAtt;
    std::unique_ptr<SliderAtt> intensityB7LenAtt, intensityB7CurveAtt;

    std::unique_ptr<SliderAtt> phase8Att, invertA8Att, invertB8Att;
    std::unique_ptr<SliderAtt> timeA8LenAtt, timeA8LenFallAtt, timeA8CurveRiseAtt;
    std::unique_ptr<SliderAtt> timeB8LenAtt, timeB8LenFallAtt, timeB8CurveRiseAtt;
    std::unique_ptr<SliderAtt> intensityA8LenAtt, intensityA8CurveAtt;
    std::unique_ptr<SliderAtt> intensityB8LenAtt, intensityB8CurveAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PinkELFOntsAudioProcessorEditor)
};