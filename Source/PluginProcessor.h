#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "EqTypes.h"
#include "Biquad.h"
#include "Crossfeed.h"
#include "DeviceSim.h"

namespace ids
{
    inline constexpr auto headphone = "headphone";
    inline constexpr auto eqOn      = "eqOn";
    inline constexpr auto amount    = "amount";
    inline constexpr auto xfeedOn   = "xfeedOn";
    inline constexpr auto width     = "width";
    inline constexpr auto distance  = "distance";
    inline constexpr auto mono      = "mono";
    inline constexpr auto side      = "side";
    inline constexpr auto swap      = "swap";
    inline constexpr auto compare   = "compare";
    inline constexpr auto output    = "output";
    inline constexpr auto device    = "device";
    inline constexpr auto xamount   = "xamount";
    inline constexpr auto speaker   = "speaker";
    inline constexpr auto autoGain  = "autoGain";
}

class ScribaHeadphonesProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int maxBands = 16;

    ScribaHeadphonesProcessor();
    ~ScribaHeadphonesProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    const juce::String getName() const override            { return JucePlugin_Name; }
    bool acceptsMidi() const override                      { return false; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 0.0; }

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- usati dalla GUI (message thread) ---
    static juce::StringArray getHeadphoneChoices();
    static juce::StringArray getDeviceChoices() { return { "Studio", "Phone", "BT Speaker", "Laptop", "Earbuds" }; }
    static juce::StringArray getSpeakerChoices() { return { "Off", "Classic NF", "Cube", "Modern NF", "Reference", "Big Mains", "Hi-Fi" }; }
    float getAutoGainDb() const noexcept { return autoGainDbForGui.load(); }
    bool isCrossfeedAllowed() const noexcept { return crossfeedAllowedForGui.load(); }
    static int getImportedChoiceIndex();
    bool importCurveFile (const juce::File& file, juce::String& error);
    scriba::HeadphoneProfile getSelectedProfile() const;
    int getImportVersion() const noexcept { return importVersion.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bool setImportedText (const juce::String& text, const juce::String& name, juce::String& error);
    bool updateEq (int profileIndex, float amount);

    mutable juce::SpinLock importLock;
    scriba::HeadphoneProfile imported;   // protetto da importLock
    juce::String importedText;
    std::atomic<int> importVersion { 0 };

    std::atomic<float>* pHeadphone = nullptr;
    std::atomic<float>* pEqOn      = nullptr;
    std::atomic<float>* pAmount    = nullptr;
    std::atomic<float>* pXfeedOn   = nullptr;
    std::atomic<float>* pWidth     = nullptr;
    std::atomic<float>* pDistance  = nullptr;
    std::atomic<float>* pMono      = nullptr;
    std::atomic<float>* pSide      = nullptr;
    std::atomic<float>* pSwap      = nullptr;
    std::atomic<float>* pCompare   = nullptr;
    std::atomic<float>* pOutput    = nullptr;
    std::atomic<float>* pDevice    = nullptr;
    std::atomic<float>* pXAmount   = nullptr;
    std::atomic<float>* pSpeaker   = nullptr;
    std::atomic<float>* pAutoGain  = nullptr;

    double currentSampleRate = 44100.0;
    std::array<std::array<scriba::Biquad, maxBands>, 2> eq;
    int activeBands = 0;
    float appliedAmount = -1.0f;
    int appliedProfile = -1, appliedImportVersion = -1;

    scriba::Crossfeed crossfeed;
    scriba::DeviceSim deviceSim;
    scriba::DeviceSim speakerSim { &scriba::getSpeakerProfile };

    // auto-gain: misura il volume (pesato in stile K) di ingresso e uscita
    scriba::Biquad kInHp, kInShelf, kOutHp, kOutShelf;
    double powIn = 0.0, powOut = 0.0, powCoef = 0.0, agCoef = 0.0;
    float autoGainLin = 1.0f;
    juce::AudioBuffer<float> meterBuffer;
    juce::LinearSmoothedValue<float> preampGain, compareMix, outGain;
    juce::AudioBuffer<float> dryBuffer;
    std::atomic<bool> crossfeedAllowedForGui { true };
    std::atomic<float> autoGainDbForGui { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScribaHeadphonesProcessor)
};
