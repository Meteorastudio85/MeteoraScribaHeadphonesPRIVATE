#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "ScribaLookAndFeel.h"

// Grafico della curva di correzione applicata
class CurveDisplay final : public juce::Component
{
public:
    explicit CurveDisplay (ScribaHeadphonesProcessor& p) : proc (p) {}

    void refresh();
    void paint (juce::Graphics&) override;

    float getMaxCorrectionDb() const noexcept { return maxCorrectionDb; }
    const juce::String& getProfileName() const noexcept { return profileName; }
    const juce::String& getProfileSource() const noexcept { return profileSource; }

private:
    ScribaHeadphonesProcessor& proc;
    std::vector<float> points;
    float maxCorrectionDb = 0.0f;
    juce::String profileName, profileSource;
};

class ScribaHeadphonesEditor final : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit ScribaHeadphonesEditor (ScribaHeadphonesProcessor&);
    ~ScribaHeadphonesEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupToggle (juce::TextButton&, const juce::String& label, bool pill = false);
    void selectSpeaker (int index);
    int currentSpeaker() const;
    void setParam (const char* id, float value);
    int currentDevice() const;
    void importCurve();
    static juce::String shortName (const juce::String& fullName);

    ScribaHeadphonesProcessor& proc;
    ScribaLookAndFeel lnf;

    juce::Image logo;

    juce::Slider amountKnob, widthSlider, xAmountSlider, distanceSlider, outputSlider;
    juce::ComboBox headphoneBox;
    juce::TextButton eqOnBtn, xfeedOnBtn, importBtn, compareBtn, monoBtn, sideBtn, swapBtn, autoBtn;
    CurveDisplay curve { proc };

    static constexpr int numDevices = 5;
    std::array<juce::TextButton, numDevices> deviceBtns;
    static constexpr int numSpeakers = 6;
    std::array<juce::TextButton, numSpeakers> speakerBtns;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SA> amountAtt, widthAtt, xAmountAtt, distanceAtt, outputAtt;
    std::unique_ptr<BA> eqOnAtt, xfeedOnAtt, compareAtt, monoAtt, sideAtt, swapAtt, autoAtt;
    std::unique_ptr<CA> headphoneAtt;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastKey;
    float lastAutoGainDb = -100.0f;

    // aree di layout
    juce::Rectangle<int> leftCard, listenCard, bottomCard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScribaHeadphonesEditor)
};
