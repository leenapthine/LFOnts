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

// --------------- main editor ---------------

class PinkELFOntsAudioProcessorEditor
    : public juce::AudioProcessorEditor,
      public juce::ChangeListener // listen for tab changes from TabbedButtonBar
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    explicit PinkELFOntsAudioProcessorEditor(PinkELFOntsAudioProcessor &);
    ~PinkELFOntsAudioProcessorEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;

    // ChangeListener
    void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
    PinkELFOntsAudioProcessor &processor;

    // Top bar
    juce::Label title;
    juce::ComboBox retrigBox, rangeBox;

    // LFO pages (tabs at top, content in the card area)
    juce::TabbedComponent laneTabs{juce::TabbedButtonBar::TabsAtTop};

    // Sections
    Section secOutput{"Output"};
    Section secLane1{""};        // title blank — the tabs ARE the header
    Section secRandom{"Random"}; // unused card (kept for parity), stays hidden

    // Global knobs (in Output section for now)
    Knob depthK{"Depth"};
    Knob phaseNudgeK{"Phase Nudge"};

    // Lane 1
    juce::ToggleButton lane1Enabled{"On"};
    Knob lane1MixK{"Mix"};
    Knob lane1PhaseDegK{"Phase"};
    Knob lane1RiseAK{"Rise A"};
    Knob lane1FallAK{"Fall A"};
    Knob lane1RiseBK{"Rise B"};
    Knob lane1FallBK{"Fall B"};

    // Random
    juce::ToggleButton randomEnabled{"On"};
    juce::ComboBox randomRate;
    Knob randomXfadeK{"Xfade (ms)"};
    Knob randomMixK{"Mix"};

    // Attachments
    using SliderAtt = APVTS::SliderAttachment;
    using ButtonAtt = APVTS::ButtonAttachment;
    using ComboAtt = APVTS::ComboBoxAttachment;

    std::unique_ptr<ComboAtt> retrigAtt, rangeAtt, randomRateAtt;

    std::unique_ptr<SliderAtt> depthAtt, phaseNudgeAtt;
    std::unique_ptr<ButtonAtt> lane1OnAtt;
    std::unique_ptr<SliderAtt> lane1MixAtt, lane1PhaseAtt, lane1RiseAAtt, lane1FallAAtt, lane1RiseBAtt, lane1FallBAtt;

    std::unique_ptr<ButtonAtt> randomOnAtt;
    std::unique_ptr<SliderAtt> randomXfadeAtt, randomMixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PinkELFOntsAudioProcessorEditor)
};
