#include "PluginEditor.h"
#include "BinaryData.h"

using namespace scriba::ui;

namespace
{
    constexpr int editorW = 760, editorH = 830;

    constexpr const char* deviceLabels[] { "STUDIO", "PHONE", "BT SPEAKER", "LAPTOP", "EARBUDS" };
    constexpr const char* deviceInfo[] {
        "Studio monitors (choose a speaker above, or none)",
        "Mono \xc2\xb7 nothing below 400 Hz \xc2\xb7 forward mids \xc2\xb7 limiter",
        "Mono \xc2\xb7 boomy 120 Hz \xc2\xb7 soft highs \xc2\xb7 compression",
        "Narrow stereo \xc2\xb7 200 Hz cut \xc2\xb7 4 kHz peak",
        "Boosted bass and presence \xc2\xb7 full stereo, no crossfeed" };

    void drawCard (juce::Graphics& g, juce::Rectangle<int> r)
    {
        const auto f = r.toFloat();
        g.setColour (card);
        g.fillRoundedRectangle (f, 18.0f);
        g.setColour (cardEdge);
        g.drawRoundedRectangle (f.reduced (0.5f), 18.0f, 1.0f);
    }
}

// =============================================================== CurveDisplay

void CurveDisplay::refresh()
{
    const auto profile = proc.getSelectedProfile();
    const bool eqOn = proc.apvts.getRawParameterValue (ids::eqOn)->load() > 0.5f;
    const float amount = eqOn ? proc.apvts.getRawParameterValue (ids::amount)->load() / 100.0f : 0.0f;

    profileName = profile.name;
    profileSource = profile.source;

    constexpr int numPoints = 240;
    constexpr double sr = 48000.0;
    points.assign (numPoints, 0.0f);

    std::vector<scriba::BiquadCoeffs> coeffs;
    for (const auto& b : profile.bands)
        coeffs.push_back (scriba::designBand (b, amount, sr));

    maxCorrectionDb = 0.0f;

    for (int i = 0; i < numPoints; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, (double) i / (numPoints - 1));
        double db = 0.0;
        for (const auto& c : coeffs)
            db += scriba::magnitudeDb (c, f, sr);

        points[(size_t) i] = (float) db;
        maxCorrectionDb = juce::jmax (maxCorrectionDb, (float) std::abs (db));
    }

    repaint();
}

void CurveDisplay::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (inset);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (cardEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

    const auto plot = r.reduced (14.0f, 12.0f).withTrimmedTop (10.0f);
    const float rangeDb = 12.0f;

    auto xFor = [&plot] (double f) { return plot.getX() + plot.getWidth() * (float) (std::log10 (f / 20.0) / 3.0); };
    auto yFor = [&plot, rangeDb] (float db) { return plot.getCentreY() - plot.getHeight() * 0.5f * juce::jlimit (-1.0f, 1.0f, db / rangeDb); };

    // griglia
    g.setFont (font (9.5f, false));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xFor (f);
        g.setColour (track.withAlpha (0.7f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (textDim.withAlpha (0.7f));
        const auto label = f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + "k" : juce::String ((int) f);
        g.drawText (label, juce::Rectangle<float> (30, 12).withCentre ({ x, plot.getBottom() + 4.0f }), juce::Justification::centred);
    }

    for (float db : { -6.0f, 6.0f })
    {
        g.setColour (track.withAlpha (0.5f));
        g.drawHorizontalLine ((int) yFor (db), plot.getX(), plot.getRight());
    }

    g.setColour (textDim.withAlpha (0.8f));
    g.drawHorizontalLine ((int) yFor (0.0f), plot.getX(), plot.getRight());

    drawCaps (g, "CORRECTION CURVE", { r.getX() + 14, r.getY() + 6, 200, 14 }, 9.5f, textDim, juce::Justification::centredLeft, 0.18f);
    if (profileSource.isNotEmpty())
        drawCaps (g, juce::String::fromUTF8 ("AutoEq \xc2\xb7 ") + profileSource, { r.getRight() - 214, r.getY() + 6, 200, 14 }, 9.5f, textDim,
                  juce::Justification::centredRight, 0.06f, false);

    if (points.empty())
        return;

    juce::Path line;
    for (size_t i = 0; i < points.size(); ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) (points.size() - 1);
        const float y = yFor (points[i]);
        if (i == 0) line.startNewSubPath (x, y);
        else        line.lineTo (x, y);
    }

    juce::Path fill (line);
    fill.lineTo (plot.getRight(), yFor (0.0f));
    fill.lineTo (plot.getX(), yFor (0.0f));
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.12f));
    g.fillPath (fill);

    g.setColour (accent);
    g.strokePath (line, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// =============================================================== Editor

ScribaHeadphonesEditor::ScribaHeadphonesEditor (ScribaHeadphonesProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    // manopola grande "Intensity"
    amountKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    amountKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amountKnob.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    amountKnob.getProperties().set ("subtitle", juce::String ("INTENSITY"));
    amountKnob.setDoubleClickReturnValue (true, 100.0);
    addAndMakeVisible (amountKnob);

    for (auto* s : { &widthSlider, &xAmountSlider, &distanceSlider, &outputSlider })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->onValueChange = [this] { repaint(); };
        addAndMakeVisible (*s);
    }
    outputSlider.getProperties().set ("bipolar", true);
    outputSlider.setDoubleClickReturnValue (true, 0.0);
    widthSlider.setDoubleClickReturnValue (true, 60.0);
    xAmountSlider.setDoubleClickReturnValue (true, 70.0);
    distanceSlider.setDoubleClickReturnValue (true, 35.0);

    headphoneBox.addItemList (ScribaHeadphonesProcessor::getHeadphoneChoices(), 1);
    addAndMakeVisible (headphoneBox);

    setupToggle (eqOnBtn, "ON", true);
    setupToggle (xfeedOnBtn, "ON", true);
    setupToggle (autoBtn, "AUTO", true);
    setupToggle (monoBtn, "MONO");
    setupToggle (sideBtn, "SIDE");
    setupToggle (swapBtn, "SWAP L/R");
    setupToggle (compareBtn, "COMPARE A/B");
    compareBtn.getProperties().set ("cta", true);

    // Mono e Side si escludono a vicenda
    monoBtn.onClick = [this] { if (monoBtn.getToggleState()) setParam (ids::side, 0.0f); };
    sideBtn.onClick = [this] { if (sideBtn.getToggleState()) setParam (ids::mono, 0.0f); };

    importBtn.setButtonText ("IMPORT AUTOEQ CURVE");
    importBtn.onClick = [this] { importCurve(); };
    addAndMakeVisible (importBtn);

    // casse da studio (3 mix + 3 mastering)
    for (int i = 0; i < numSpeakers; ++i)
    {
        auto& b = speakerBtns[(size_t) i];
        b.setButtonText (scriba::getSpeakerInfo (i + 1).label);
        b.getProperties().set ("fontSize", 11.0f);
        b.onClick = [this, i] { selectSpeaker (i + 1); };
        addAndMakeVisible (b);
    }

    // dispositivi di consumo
    for (int i = 0; i < numDevices; ++i)
    {
        auto& b = deviceBtns[(size_t) i];
        b.setButtonText (deviceLabels[i]);
        b.getProperties().set ("icon", i);
        b.getProperties().set ("fontSize", 11.0f);
        b.onClick = [this, i] { setParam (ids::device, (float) i); };
        addAndMakeVisible (b);
    }

    auto& st = proc.apvts;
    amountAtt    = std::make_unique<SA> (st, ids::amount,   amountKnob);
    widthAtt     = std::make_unique<SA> (st, ids::width,    widthSlider);
    xAmountAtt   = std::make_unique<SA> (st, ids::xamount,  xAmountSlider);
    distanceAtt  = std::make_unique<SA> (st, ids::distance, distanceSlider);
    outputAtt    = std::make_unique<SA> (st, ids::output,   outputSlider);
    eqOnAtt      = std::make_unique<BA> (st, ids::eqOn,     eqOnBtn);
    xfeedOnAtt   = std::make_unique<BA> (st, ids::xfeedOn,  xfeedOnBtn);
    autoAtt      = std::make_unique<BA> (st, ids::autoGain, autoBtn);
    compareAtt   = std::make_unique<BA> (st, ids::compare,  compareBtn);
    monoAtt      = std::make_unique<BA> (st, ids::mono,     monoBtn);
    sideAtt      = std::make_unique<BA> (st, ids::side,     sideBtn);
    swapAtt      = std::make_unique<BA> (st, ids::swap,     swapBtn);
    headphoneAtt = std::make_unique<CA> (st, ids::headphone, headphoneBox);

    addAndMakeVisible (curve);

    setSize (editorW, editorH);
    timerCallback();
    startTimerHz (15);
}

ScribaHeadphonesEditor::~ScribaHeadphonesEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ScribaHeadphonesEditor::setupToggle (juce::TextButton& b, const juce::String& label, bool pill)
{
    b.setButtonText (label);
    b.setClickingTogglesState (true);
    if (pill)
    {
        b.getProperties().set ("pill", true);
        b.getProperties().set ("fontSize", 11.0f);
    }
    addAndMakeVisible (b);
}

void ScribaHeadphonesEditor::setParam (const char* id, float value)
{
    if (auto* param = proc.apvts.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    }
}

// Sceglie una cassa: ne carica angolo, Amount e stanza consigliati
// (poi si possono ritoccare a mano). Un secondo click la spegne.
void ScribaHeadphonesEditor::selectSpeaker (int index)
{
    const int next = (index == currentSpeaker() && currentDevice() == 0) ? 0 : index;
    setParam (ids::device, 0.0f);
    setParam (ids::speaker, (float) next);

    if (next != 0)
    {
        const auto& info = scriba::getSpeakerInfo (next);
        setParam (ids::width, info.angle);
        setParam (ids::xamount, info.amount);
        setParam (ids::distance, info.room);
        setParam (ids::xfeedOn, 1.0f);
    }
}

void ScribaHeadphonesEditor::importCurve()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose an AutoEq ParametricEQ.txt file",
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                   "*.txt");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (! file.existsAsFile())
                                  return;

                              juce::String error;
                              if (! proc.importCurveFile (file, error))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                          "SCRIBA Headphones", error);
                          });
}

juce::String ScribaHeadphonesEditor::shortName (const juce::String& fullName)
{
    // "HiFiMan HE400se" -> "HE400se", "Sennheiser HD 600" -> "HD 600"
    const auto rest = fullName.fromFirstOccurrenceOf (" ", false, false).trim();
    return rest.isNotEmpty() ? rest : fullName;
}

int ScribaHeadphonesEditor::currentDevice() const
{
    return juce::jlimit (0, numDevices - 1, (int) proc.apvts.getRawParameterValue (ids::device)->load());
}

int ScribaHeadphonesEditor::currentSpeaker() const
{
    return juce::jlimit (0, numSpeakers, (int) proc.apvts.getRawParameterValue (ids::speaker)->load());
}

void ScribaHeadphonesEditor::timerCallback()
{
    auto& st = proc.apvts;
    const int importVer = proc.getImportVersion();
    const int device = currentDevice();
    const int speaker = currentSpeaker();

    const juce::String key = juce::String (st.getRawParameterValue (ids::headphone)->load())
                           + "|" + juce::String (st.getRawParameterValue (ids::amount)->load())
                           + "|" + juce::String (st.getRawParameterValue (ids::eqOn)->load())
                           + "|" + juce::String (importVer)
                           + "|" + juce::String (device) + "|" + juce::String (speaker);

    if (key != lastKey)
    {
        lastKey = key;
        curve.refresh();
        amountKnob.getProperties().set ("title", shortName (curve.getProfileName()).toUpperCase());
        amountKnob.setEnabled (st.getRawParameterValue (ids::eqOn)->load() > 0.5f);
        amountKnob.repaint();

        for (int i = 0; i < numDevices; ++i)
            deviceBtns[(size_t) i].setToggleState (i == device, juce::dontSendNotification);

        for (int i = 0; i < numSpeakers; ++i)
        {
            speakerBtns[(size_t) i].setToggleState (device == 0 && speaker == i + 1, juce::dontSendNotification);
            speakerBtns[(size_t) i].setAlpha (device == 0 ? 1.0f : 0.45f);
        }

        // crossfeed escluso dai dispositivi mono e dagli auricolari
        const bool xAllowed = scriba::getDeviceProfile ((scriba::Device) device).allowsCrossfeed;
        for (juce::Component* c : { (juce::Component*) &xfeedOnBtn, (juce::Component*) &widthSlider,
                                    (juce::Component*) &xAmountSlider, (juce::Component*) &distanceSlider })
            c->setAlpha (xAllowed ? 1.0f : 0.45f);

        repaint();
    }

    for (auto* b : { &eqOnBtn, &xfeedOnBtn })
    {
        const juce::String label = b->getToggleState() ? "ON" : "OFF";
        if (b->getButtonText() != label)
            b->setButtonText (label);
    }

    // valore dell'auto-gain accanto al pulsante AUTO
    const float agDb = proc.getAutoGainDb();
    if (std::abs (agDb - lastAutoGainDb) > 0.05f)
    {
        lastAutoGainDb = agDb;
        repaint (bottomCard);
    }

    const bool comparing = st.getRawParameterValue (ids::compare)->load() > 0.5f;
    const juce::String ctaLabel = comparing ? "HEARING ORIGINAL" : "COMPARE A/B";
    if (compareBtn.getButtonText() != ctaLabel)
    {
        compareBtn.setButtonText (ctaLabel);
        repaint();
    }
}

void ScribaHeadphonesEditor::resized()
{
    leftCard   = { 20, 100, 300, 392 };
    listenCard = { 20, 504, 720, 192 };
    bottomCard = { 20, 708, 720, 86 };

    eqOnBtn.setBounds (leftCard.getRight() - 76, 112, 56, 28);
    amountKnob.setBounds (leftCard.getX() + 25, 140, 250, 250);
    importBtn.setBounds (leftCard.getX() + 20, 446, 260, 34);

    const int rx = 340, rw = 400;
    headphoneBox.setBounds (rx, 124, rw, 40);
    curve.setBounds (rx, 172, rw, 92);
    xfeedOnBtn.setBounds (rx + rw - 56, 272, 56, 28);
    widthSlider.setBounds    (rx - 2, 318, rw + 4, 26);
    xAmountSlider.setBounds  (rx - 2, 362, rw + 4, 26);
    distanceSlider.setBounds (rx - 2, 406, rw + 4, 26);

    compareBtn.setBounds (rx, 446, rw, 46);

    // casse: 3 MIX | 3 MASTERING, con uno spazio in piu' tra i gruppi
    const int sw = (680 - 5 * 10 - 20) / numSpeakers;
    for (int i = 0; i < numSpeakers; ++i)
        speakerBtns[(size_t) i].setBounds (40 + i * (sw + 10) + (i >= 3 ? 20 : 0), 556, sw, 38);

    const int dw = (680 - 4 * 10) / numDevices;
    for (int i = 0; i < numDevices; ++i)
        deviceBtns[(size_t) i].setBounds (40 + i * (dw + 10), 624, dw, 60);

    monoBtn.setBounds (40, 746, 94, 34);
    sideBtn.setBounds (142, 746, 94, 34);
    swapBtn.setBounds (244, 746, 126, 34);
    autoBtn.setBounds (514, 717, 64, 24);
    outputSlider.setBounds (410, 752, 312, 28);
}

void ScribaHeadphonesEditor::paint (juce::Graphics& g)
{
    auto& st = proc.apvts;
    const auto bounds = getLocalBounds().toFloat();
    const int device = currentDevice();
    const int speaker = currentSpeaker();
    const bool simulating = device != 0;
    const bool speakerOn = ! simulating && speaker != 0;
    const bool xAllowed = scriba::getDeviceProfile ((scriba::Device) device).allowsCrossfeed;

    // sfondo sfumato blu notte, come Scriba Align
    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, bounds.getWidth() * 0.6f, bounds.getHeight(), false));
    g.fillAll();

    // ---------------- header
    if (logo.isValid())
        g.drawImage (logo, juce::Rectangle<float> (22, 20, 66, 66), juce::RectanglePlacement::centred);

    const auto titleFont = font (30.0f, true, 0.24f);
    g.setFont (titleFont);
    g.setColour (text);
    g.drawText ("SCRIBA", juce::Rectangle<float> (104, 22, 200, 40), juce::Justification::centredLeft, false);
    const float scribaW = juce::GlyphArrangement::getStringWidth (titleFont, "SCRIBA");
    g.setColour (accent);
    g.drawText ("HEADPHONES", juce::Rectangle<float> (104 + scribaW + 18, 22, 320, 40), juce::Justification::centredLeft, false);
    drawCaps (g, juce::String::fromUTF8 ("CORRECTION \xc2\xb7 CROSSFEED \xc2\xb7 MONITORING"),
              { 106, 62, 400, 18 }, 11.0f, textDim, juce::Justification::centredLeft, 0.3f, false);

    // pillola: dispositivo simulato (ambra), cassa attiva, oppure la cuffia
    {
        juce::String name = shortName (curve.getProfileName()).toUpperCase();
        if (simulating)     name = deviceLabels[device];
        else if (speakerOn) name = scriba::getSpeakerInfo (speaker).label;

        const auto pf = font (13.0f, true, 0.16f);
        const float pw = juce::jmax (110.0f, juce::GlyphArrangement::getStringWidth (pf, name) + 60.0f);
        const auto pill = juce::Rectangle<float> (bounds.getRight() - 24 - pw, 30, pw, 40);
        g.setColour (inset);
        g.fillRoundedRectangle (pill, 20.0f);
        g.setColour (cardEdge);
        g.drawRoundedRectangle (pill.reduced (0.5f), 20.0f, 1.0f);
        g.setColour (simulating ? amber : (st.getRawParameterValue (ids::eqOn)->load() > 0.5f || speakerOn ? accent : textDim));
        g.fillEllipse (juce::Rectangle<float> (12, 12).withCentre ({ pill.getX() + 24, pill.getCentreY() }));
        g.setColour (text);
        g.setFont (pf);
        g.drawText (name, pill.withTrimmedLeft (40).withTrimmedRight (16), juce::Justification::centredLeft, false);
    }

    // ---------------- card sinistra: correzione
    drawCard (g, leftCard);
    drawCaps (g, "CORRECTION", { 40, 112, 200, 28 }, 12.5f, textDim);

    {
        const auto kb = amountKnob.getBounds().toFloat();
        const auto c = kb.getCentre();
        const float radius = kb.getWidth() * 0.5f - 16.0f + 34.0f;
        g.setFont (font (11.0f, false));
        const struct { float deg; const char* label; } ticks[] { { -135.0f, "0" }, { -67.5f, "25" }, { 67.5f, "75" }, { 135.0f, "100" } };
        for (auto t : ticks)
        {
            const float a = juce::degreesToRadians (t.deg);
            const auto pt = c + juce::Point<float> (std::sin (a), -std::cos (a)) * radius;
            g.setColour (textDim);
            g.drawText (t.label, juce::Rectangle<float> (34, 14).withCentre (pt), juce::Justification::centred);
        }
    }

    {
        const float maxDb = curve.getMaxCorrectionDb();
        const auto bar = juce::Rectangle<float> (40, 404, 260, 6);
        g.setColour (track);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, maxDb / 12.0f)), 3.0f);

        g.setFont (font (13.5f, false));
        g.setColour (textDim);
        g.drawText ("Max correction", juce::Rectangle<float> (40, 416, 180, 22), juce::Justification::centredLeft);
        g.setFont (font (13.5f, true));
        g.setColour (text);
        g.drawText (juce::String (maxDb, 1) + " dB", juce::Rectangle<float> (200, 416, 100, 22), juce::Justification::centredRight);
    }

    // ---------------- colonna destra
    drawCaps (g, "HEADPHONE", { 344, 100, 200, 20 }, 12.5f, textDim);

    {
        juce::Graphics::ScopedSaveState save (g);
        g.setOpacity (xAllowed ? 1.0f : 0.45f);
        drawCaps (g, "VIRTUAL MONITORS", { 344, 272, 240, 28 }, 12.5f, text);

        const float w = (float) widthSlider.getValue();
        const float a = (float) xAmountSlider.getValue();
        const float d = (float) distanceSlider.getValue();

        g.setFont (font (13.0f, false));
        g.setColour (textDim);
        g.drawText ("width",  juce::Rectangle<float> (344, 302, 150, 16), juce::Justification::centredLeft);
        g.drawText ("amount", juce::Rectangle<float> (344, 346, 150, 16), juce::Justification::centredLeft);
        g.drawText ("room",   juce::Rectangle<float> (344, 390, 150, 16), juce::Justification::centredLeft);
        g.setFont (font (13.5f, true));
        g.setColour (text);
        g.drawText (juce::String (juce::roundToInt (w)) + juce::String::fromUTF8 ("\xc2\xb0"),
                    juce::Rectangle<float> (590, 302, 150, 16), juce::Justification::centredRight);
        g.drawText (juce::String (juce::roundToInt (a)) + "%", juce::Rectangle<float> (590, 346, 150, 16), juce::Justification::centredRight);
        g.drawText (juce::String (juce::roundToInt (d)) + "%", juce::Rectangle<float> (590, 390, 150, 16), juce::Justification::centredRight);
    }

    if (! xAllowed)
    {
        g.setFont (font (11.0f, false));
        g.setColour (amber);
        g.drawText ("disabled by " + juce::String (deviceLabels[device]).toLowerCase(),
                    juce::Rectangle<float> (440, 272, 230, 28), juce::Justification::centredRight);
    }

    // ---------------- card "Listen on": casse + dispositivi
    drawCard (g, listenCard);
    drawCaps (g, "LISTEN ON", { 40, 512, 200, 24 }, 12.5f, textDim);

    juce::String info = juce::String::fromUTF8 (deviceInfo[device]);
    if (speakerOn)
        info = juce::String::fromUTF8 (scriba::getSpeakerInfo (speaker).info);
    g.setFont (font (12.0f, false));
    g.setColour (simulating ? amber : (speakerOn ? accent : textDim));
    g.drawText (info, juce::Rectangle<float> (220, 512, 500, 24), juce::Justification::centredRight);

    {
        juce::Graphics::ScopedSaveState save (g);
        g.setOpacity (simulating ? 0.45f : 1.0f);
        drawCaps (g, "MIX", speakerBtns[0].getBounds().toFloat().withY (538).withHeight (14), 10.0f, textDim);
        drawCaps (g, "MASTERING", speakerBtns[3].getBounds().toFloat().withY (538).withHeight (14).withWidth (200), 10.0f, textDim);
    }
    drawCaps (g, "CONSUMER DEVICES", { 40, 606, 300, 14 }, 10.0f, textDim);

    // ---------------- card in basso: monitor e uscita
    drawCard (g, bottomCard);
    drawCaps (g, "MONITOR", { 40, 716, 200, 22 }, 12.5f, textDim);
    drawCaps (g, "OUTPUT", { 414, 716, 100, 22 }, 12.5f, textDim);

    const float outDb = (float) outputSlider.getValue();
    g.setFont (font (14.0f, true));
    g.setColour (text);
    g.drawText ((outDb > 0.0f ? "+" : "") + juce::String (outDb, 1) + " dB",
                juce::Rectangle<float> (640, 716, 80, 22), juce::Justification::centredRight);

    if (autoBtn.getToggleState())
    {
        g.setFont (font (11.0f, false));
        g.setColour (textDim);
        g.drawText ((lastAutoGainDb > 0.0f ? "+" : "") + juce::String (lastAutoGainDb, 1) + " dB",
                    juce::Rectangle<float> (584, 716, 60, 22), juce::Justification::centredLeft);
    }

    // ---------------- footer
    const bool comparing = st.getRawParameterValue (ids::compare)->load() > 0.5f;
    juce::String footer = "Reminder: bypass SCRIBA Headphones before bouncing.";
    if (comparing)
        footer = "Listening without processing (level matched).";
    else if (simulating)
        footer = juce::String (deviceLabels[device]) + " simulation on: for listening checks only.";

    g.setFont (font (12.5f, false));
    g.setColour (comparing || simulating ? amber : textDim);
    g.drawText (footer, juce::Rectangle<float> (22, 802, 420, 22), juce::Justification::centredLeft);
    g.setColour (textDim);
    g.drawText (juce::String::fromUTF8 ("\xc2\xa9 2026 Meteora Plugins - Scriba Headphones - v1.2.0"),
                juce::Rectangle<float> (380, 802, 358, 22), juce::Justification::centredRight);
}
