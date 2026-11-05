#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "HeadphonePresets.h"

ScribaHeadphonesProcessor::ScribaHeadphonesProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pHeadphone = apvts.getRawParameterValue (ids::headphone);
    pEqOn      = apvts.getRawParameterValue (ids::eqOn);
    pAmount    = apvts.getRawParameterValue (ids::amount);
    pXfeedOn   = apvts.getRawParameterValue (ids::xfeedOn);
    pWidth     = apvts.getRawParameterValue (ids::width);
    pDistance  = apvts.getRawParameterValue (ids::distance);
    pMono      = apvts.getRawParameterValue (ids::mono);
    pSide      = apvts.getRawParameterValue (ids::side);
    pSwap      = apvts.getRawParameterValue (ids::swap);
    pCompare   = apvts.getRawParameterValue (ids::compare);
    pOutput    = apvts.getRawParameterValue (ids::output);
    pDevice    = apvts.getRawParameterValue (ids::device);
    pXAmount   = apvts.getRawParameterValue (ids::xamount);
    pSpeaker   = apvts.getRawParameterValue (ids::speaker);
    pAutoGain  = apvts.getRawParameterValue (ids::autoGain);
}

juce::StringArray ScribaHeadphonesProcessor::getHeadphoneChoices()
{
    juce::StringArray s;
    for (const auto& p : scriba::getHeadphonePresets())
        s.add (p.name);
    s.add ("Imported curve");
    return s;
}

int ScribaHeadphonesProcessor::getImportedChoiceIndex()
{
    return (int) scriba::getHeadphonePresets().size();
}

juce::AudioProcessorValueTreeState::ParameterLayout ScribaHeadphonesProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const auto pct = AudioParameterFloatAttributes().withLabel ("%");
    const auto db  = AudioParameterFloatAttributes().withLabel ("dB");

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::headphone, 1 }, "Headphone", getHeadphoneChoices(), 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ids::eqOn, 1 },      "Correction", true));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::amount, 1 },    "Intensity",  NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ids::xfeedOn, 1 },   "Crossfeed",  true));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::width, 2 },     "Width",      NormalisableRange<float> (0.0f, 90.0f, 0.1f), 60.0f,
                                                        AudioParameterFloatAttributes().withLabel (String::fromUTF8 ("\xc2\xb0"))));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::distance, 2 },  "Room",       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::xamount, 1 },   "Amount",     NormalisableRange<float> (0.0f, 100.0f, 0.1f), 70.0f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::speaker, 1 },   "Speaker",    getSpeakerChoices(), 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ids::autoGain, 1 },  "Auto Gain",  true));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ids::mono, 1 },      "Mono",       false));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ids::side, 1 },      "Side",       false));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ids::swap, 1 },      "Swap L/R",  false));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ids::compare, 1 },   "Compare A/B", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::device, 1 },    "Listen On", getDeviceChoices(), 0));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::output, 1 },    "Output",     NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, db));

    return layout;
}

bool ScribaHeadphonesProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void ScribaHeadphonesProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    for (auto& ch : eq)
        for (auto& f : ch)
            f.reset();

    activeBands = 0;
    appliedAmount = -1.0f;
    appliedProfile = -1;
    appliedImportVersion = -1;

    crossfeed.prepare (sampleRate);
    deviceSim.prepare (sampleRate);
    speakerSim.prepare (sampleRate);

    // pesatura simile a K (ITU BS.1770): passa-alto 60 Hz + shelf +4 dB sopra 1,7 kHz
    const auto hp = scriba::designBand ({ scriba::EqBand::HighPass, 60.0f, 0.0f, 0.5f }, 1.0, sampleRate);
    const auto sh = scriba::designBand ({ scriba::EqBand::HighShelf, 1700.0f, 4.0f, 0.7f }, 1.0, sampleRate);
    for (auto* b : { &kInHp, &kOutHp })       { b->setCoeffs (hp); b->reset(); }
    for (auto* b : { &kInShelf, &kOutShelf }) { b->setCoeffs (sh); b->reset(); }
    powIn = powOut = 0.0;
    powCoef = std::exp (-1.0 / (3.0 * sampleRate));   // finestra ~3 s
    agCoef  = std::exp (-1.0 / (0.4 * sampleRate));   // movimento del guadagno ~0,4 s
    autoGainLin = 1.0f;
    meterBuffer.setSize (2, juce::jmax (1, samplesPerBlock));

    preampGain.reset (sampleRate, 0.05);
    compareMix.reset (sampleRate, 0.03);
    outGain.reset (sampleRate, 0.05);
    preampGain.setCurrentAndTargetValue (1.0f);
    compareMix.setCurrentAndTargetValue (*pCompare > 0.5f ? 1.0f : 0.0f);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    dryBuffer.setSize (2, juce::jmax (1, samplesPerBlock));
}

// Ricalcola i coefficienti EQ (thread audio, nessuna allocazione).
bool ScribaHeadphonesProcessor::updateEq (int profileIndex, float amount)
{
    const auto& presets = scriba::getHeadphonePresets();

    auto apply = [this, amount] (const scriba::HeadphoneProfile& p)
    {
        activeBands = juce::jmin ((int) p.bands.size(), maxBands);

        for (int b = 0; b < activeBands; ++b)
        {
            const auto c = scriba::designBand (p.bands[(size_t) b], amount, currentSampleRate);
            eq[0][(size_t) b].setCoeffs (c);
            eq[1][(size_t) b].setCoeffs (c);
        }

        preampGain.setTargetValue (juce::Decibels::decibelsToGain (p.preampDb * amount));
    };

    if (profileIndex >= 0 && profileIndex < (int) presets.size())
    {
        apply (presets[(size_t) profileIndex]);
        return true;
    }

    const juce::SpinLock::ScopedTryLockType lock (importLock);
    if (! lock.isLocked())
        return false; // la GUI sta importando: riprovo al prossimo blocco

    if (imported.bands.empty())
    {
        activeBands = 0;
        preampGain.setTargetValue (1.0f);
    }
    else
    {
        apply (imported);
    }

    return true;
}

void ScribaHeadphonesProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // 1) Utility di ascolto: inverti L/R, mono, solo side
    const bool swap = *pSwap > 0.5f, mono = *pMono > 0.5f, side = *pSide > 0.5f;

    if (swap || mono || side)
    {
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (swap) std::swap (l, r);

            if (side)      { const float s = 0.5f * (l - r); l = s; r = s; }
            else if (mono) { const float m = 0.5f * (l + r); l = m; r = m; }

            L[i] = l; R[i] = r;
        }
    }

    // Copia per il confronto A/B (segnale senza correzione)
    if (dryBuffer.getNumSamples() < n)
        dryBuffer.setSize (2, n, false, false, true);

    dryBuffer.copyFrom (0, 0, buffer, 0, 0, n);
    dryBuffer.copyFrom (1, 0, buffer, 1, 0, n);

    // 2) Simulazione del dispositivo ("Listen On") oppure della cassa da studio
    const int device = juce::jlimit (0, 4, (int) pDevice->load());
    deviceSim.process (L, R, n, device);
    speakerSim.process (L, R, n, device == 0 ? juce::jlimit (0, 6, (int) pSpeaker->load()) : 0);
    const bool xfeedAllowed = deviceSim.crossfeedAllowed();
    crossfeedAllowedForGui.store (xfeedAllowed);

    // 3) Monitor virtuali (testa + stanza)
    crossfeed.process (L, R, n, *pXfeedOn > 0.5f && xfeedAllowed,
                       pWidth->load(), pXAmount->load() / 100.0f, pDistance->load() / 100.0f);

    // 4) Correzione della cuffia (con Amount ammorbidito per evitare scatti)
    const int profileIndex = (int) pHeadphone->load();
    const float targetAmount = *pEqOn > 0.5f ? pAmount->load() / 100.0f : 0.0f;
    const int importVer = importVersion.load();

    const bool profileChanged = profileIndex != appliedProfile
                             || (profileIndex == getImportedChoiceIndex() && importVer != appliedImportVersion);

    if (profileChanged || std::abs (appliedAmount - targetAmount) > 1.0e-4f)
    {
        float next = targetAmount;

        if (! profileChanged && appliedAmount >= 0.0f)
        {
            const float maxStep = (float) n / (float) (0.15 * currentSampleRate);
            next = appliedAmount + juce::jlimit (-maxStep, maxStep, targetAmount - appliedAmount);
        }

        if (profileChanged)
            for (auto& ch : eq)
                for (auto& f : ch)
                    f.reset();

        if (updateEq (profileIndex, next))
        {
            appliedAmount = next;
            appliedProfile = profileIndex;
            appliedImportVersion = importVer;
        }
    }

    for (int b = 0; b < activeBands; ++b)
    {
        eq[0][(size_t) b].process (L, n);
        eq[1][(size_t) b].process (R, n);
    }

    // 5) Preamp della curva, auto-gain, confronto A/B, uscita
    compareMix.setTargetValue (*pCompare > 0.5f ? 1.0f : 0.0f);
    outGain.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    const bool autoGainOn = *pAutoGain > 0.5f;

    const float* dL = dryBuffer.getReadPointer (0);
    const float* dR = dryBuffer.getReadPointer (1);

    for (int i = 0; i < n; ++i)
    {
        const float pg = preampGain.getNextValue();
        const float m  = compareMix.getNextValue();
        const float og = outGain.getNextValue();

        const float wl = pg * L[i], wr = pg * R[i];

        // Auto-gain: confronta il volume (pesato) dell'uscita con quello dell'ingresso
        // e lo riallinea lentamente, cosi' il confronto con l'originale e' onesto.
        float ag = 1.0f;
        if (autoGainOn)
        {
            const float kin  = kInShelf.processSample  (kInHp.processSample  (0.5f * (dL[i] + dR[i])));
            const float kout = kOutShelf.processSample (kOutHp.processSample (0.5f * (wl + wr)));
            powIn  = powCoef * powIn  + (1.0 - powCoef) * (double) (kin * kin);
            powOut = powCoef * powOut + (1.0 - powCoef) * (double) (kout * kout);

            if (powIn > 1.0e-8 && powOut > 1.0e-10)   // ignora il silenzio
            {
                const double target = juce::jlimit (0.125, 8.0, std::sqrt (powIn / powOut));   // +/- 18 dB
                autoGainLin = (float) (agCoef * autoGainLin + (1.0 - agCoef) * target);
            }
            ag = autoGainLin;
        }

        // A/B: con l'auto-gain l'originale passa pulito, senza auto-gain resta allineato al preamp
        const float dryGain = autoGainOn ? 1.0f : pg;
        L[i] = og * (wl * ag * (1.0f - m) + dL[i] * dryGain * m);
        R[i] = og * (wr * ag * (1.0f - m) + dR[i] * dryGain * m);
    }

    autoGainDbForGui.store (autoGainOn ? juce::Decibels::gainToDecibels (autoGainLin) : 0.0f);
}

// ---------------------------------------------------------------- import curva

bool ScribaHeadphonesProcessor::setImportedText (const juce::String& text, const juce::String& name, juce::String& error)
{
    scriba::HeadphoneProfile p;

    if (! scriba::parseAutoEqText (text, p, maxBands))
    {
        error = "This file contains no valid AutoEq filters (ParametricEQ.txt).";
        return false;
    }

    p.name = name.isNotEmpty() ? name : juce::String ("Imported curve");
    p.source = "imported file";

    {
        const juce::SpinLock::ScopedLockType lock (importLock);
        imported = std::move (p);
        importedText = text;
    }

    ++importVersion;
    return true;
}

bool ScribaHeadphonesProcessor::importCurveFile (const juce::File& file, juce::String& error)
{
    const auto name = file.getFileNameWithoutExtension()
                          .replace ("ParametricEQ", "", true)
                          .trim();

    if (! setImportedText (file.loadFileAsString(), name, error))
        return false;

    if (auto* param = apvts.getParameter (ids::headphone))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) getImportedChoiceIndex()));
        param->endChangeGesture();
    }

    return true;
}

scriba::HeadphoneProfile ScribaHeadphonesProcessor::getSelectedProfile() const
{
    const auto& presets = scriba::getHeadphonePresets();
    const int index = (int) pHeadphone->load();

    if (index >= 0 && index < (int) presets.size())
        return presets[(size_t) index];

    const juce::SpinLock::ScopedLockType lock (importLock);
    auto p = imported;
    if (p.bands.empty())
        p.name = "No curve";
    return p;
}

// ---------------------------------------------------------------- stato

void ScribaHeadphonesProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    juce::ValueTree imp ("IMPORTED");
    {
        const juce::SpinLock::ScopedLockType lock (importLock);
        imp.setProperty ("name", imported.name, nullptr);
        imp.setProperty ("text", importedText, nullptr);
    }
    state.appendChild (imp, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ScribaHeadphonesProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    auto imp = state.getChildWithName ("IMPORTED");

    if (imp.isValid())
    {
        const auto text = imp.getProperty ("text").toString();
        juce::String error;
        if (text.isNotEmpty())
            setImportedText (text, imp.getProperty ("name").toString(), error);

        state.removeChild (imp, nullptr);
    }

    apvts.replaceState (state);
}

juce::AudioProcessorEditor* ScribaHeadphonesProcessor::createEditor()
{
    return new ScribaHeadphonesEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ScribaHeadphonesProcessor();
}
