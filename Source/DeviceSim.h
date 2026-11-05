#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include "Biquad.h"

namespace scriba
{
// ----------------------------------------------------------------------------
// "Ascolta su": simula come suona il mix su dispositivi di consumo.
// Ogni dispositivo = somma mono / stereo ristretto + EQ tipico dell'altoparlante
// + compressione/limitazione (i dispositivi piccoli la usano tutti) + leggera
// saturazione dove serve. Il livello e' compensato per confronti onesti.
// ----------------------------------------------------------------------------
enum class Device { Studio = 0, Phone, BluetoothSpeaker, Laptop, Earbuds };

struct DeviceProfile
{
    float width;          // 0 = mono, 1 = stereo invariato
    bool  allowsCrossfeed;
    std::vector<EqBand> bands;
    float threshDb, ratio, attackMs, releaseMs;
    float drive;          // 1 = nessuna saturazione
    float makeupDb;
};

inline const DeviceProfile& getDeviceProfile (Device d)
{
    using B = EqBand;
    static const DeviceProfile studio { 1.0f, true, {}, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };

    static const DeviceProfile phone { 0.0f, false, {
            { B::HighPass, 420.0f, 0.0f, 0.54f }, { B::HighPass, 420.0f, 0.0f, 1.31f },
            { B::Peak, 1000.0f, -2.0f, 1.0f },    { B::Peak, 2500.0f, 4.0f, 1.2f },
            { B::Peak, 5500.0f, 2.0f, 2.0f },     { B::LowPass, 12000.0f, 0.0f, 0.707f } },
        -22.0f, 6.0f, 2.0f, 80.0f, 1.6f, 7.0f };

    static const DeviceProfile bluetooth { 0.0f, false, {
            { B::HighPass, 65.0f, 0.0f, 0.707f }, { B::Peak, 120.0f, 6.0f, 1.4f },
            { B::Peak, 450.0f, -3.0f, 1.0f },     { B::Peak, 3000.0f, 2.0f, 1.5f },
            { B::HighShelf, 9000.0f, -4.0f, 0.7f }, { B::LowPass, 14000.0f, 0.0f, 0.707f } },
        -18.0f, 4.0f, 5.0f, 150.0f, 1.15f, 1.0f };

    static const DeviceProfile laptop { 0.35f, true, {
            { B::HighPass, 230.0f, 0.0f, 0.54f }, { B::HighPass, 230.0f, 0.0f, 1.31f },
            { B::Peak, 900.0f, 2.0f, 1.0f },      { B::Peak, 4200.0f, 4.0f, 1.6f },
            { B::LowPass, 15000.0f, 0.0f, 0.707f } },
        -18.0f, 3.0f, 3.0f, 100.0f, 1.0f, 5.0f };

    static const DeviceProfile earbuds { 1.0f, false, {
            { B::LowShelf, 105.0f, 6.0f, 0.7f },  { B::Peak, 3000.0f, 2.5f, 1.4f },
            { B::HighShelf, 9000.0f, -2.5f, 0.7f } },
        0.0f, 1.0f, 1.0f, 1.0f, 1.0f, -3.0f };

    switch (d)
    {
        case Device::Phone:            return phone;
        case Device::BluetoothSpeaker: return bluetooth;
        case Device::Laptop:           return laptop;
        case Device::Earbuds:          return earbuds;
        case Device::Studio:
        default:                       return studio;
    }
}

// Profilo per indice: 0 = nessuna colorazione (bypass)
using ProfileFn = const DeviceProfile& (*) (int);

inline const DeviceProfile& deviceProfileByIndex (int i) { return getDeviceProfile ((Device) i); }

// ----------------------------------------------------------------------------
// Casse da studio: 3 per il mix e 3 per il mastering (nomi generici).
// Ogni cassa = risposta tipica + angolo/stanza consigliati (applicati dalla GUI).
// ----------------------------------------------------------------------------
struct SpeakerInfo
{
    const char* label;      // testo del pulsante
    const char* info;       // descrizione nella GUI
    float angle, amount, room;
};

inline const SpeakerInfo& getSpeakerInfo (int i)
{
    static const SpeakerInfo infos[] {
        { "OFF",        "No speaker voicing",                                                     60.0f, 70.0f, 35.0f },
        { "CLASSIC NF", "Classic nearfield \xc2\xb7 forward mids at 1.5 kHz \xc2\xb7 lean lows", 60.0f, 70.0f, 30.0f },
        { "CUBE",       "Single full-range cube \xc2\xb7 mono \xc2\xb7 150 Hz to 8 kHz",       0.0f, 70.0f, 25.0f },
        { "MODERN NF",  "Modern 2-way nearfield \xc2\xb7 flat \xc2\xb7 extended to 45 Hz",     60.0f, 70.0f, 35.0f },
        { "REFERENCE",  "Full-range mastering monitors \xc2\xb7 flat to 28 Hz \xc2\xb7 treated room", 60.0f, 75.0f, 40.0f },
        { "BIG MAINS",  "Soffit-mounted mains \xc2\xb7 deep low end \xc2\xb7 bigger room",     70.0f, 80.0f, 65.0f },
        { "HI-FI",      "Living-room floorstanders \xc2\xb7 warm lows \xc2\xb7 airy top",       50.0f, 80.0f, 80.0f } };
    return infos[juce::jlimit (0, 6, i)];
}

inline const DeviceProfile& getSpeakerProfile (int i)
{
    using B = EqBand;
    static const DeviceProfile none { 1.0f, true, {}, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };

    // MIX
    static const DeviceProfile classicNf { 1.0f, true, {
            { B::HighPass, 62.0f, 0.0f, 0.85f }, { B::HighPass, 50.0f, 0.0f, 0.6f },
            { B::Peak, 1500.0f, 3.0f, 1.0f },    { B::Peak, 3500.0f, -1.5f, 2.0f },
            { B::HighShelf, 8000.0f, -2.0f, 0.7f }, { B::LowPass, 18000.0f, 0.0f, 0.707f } },
        0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };

    static const DeviceProfile cube { 0.0f, true, {
            { B::HighPass, 140.0f, 0.0f, 0.54f }, { B::HighPass, 140.0f, 0.0f, 1.31f },
            { B::Peak, 1000.0f, 4.0f, 0.8f },     { B::LowPass, 7000.0f, 0.0f, 0.707f },
            { B::LowPass, 9000.0f, 0.0f, 0.707f } },
        0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };

    static const DeviceProfile modernNf { 1.0f, true, {
            { B::HighPass, 42.0f, 0.0f, 0.54f }, { B::HighPass, 42.0f, 0.0f, 1.31f },
            { B::Peak, 80.0f, 1.0f, 1.0f },      { B::Peak, 2500.0f, -0.5f, 1.5f },
            { B::HighShelf, 12000.0f, 0.5f, 0.7f } },
        0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };

    // MASTERING
    static const DeviceProfile reference { 1.0f, true, {
            { B::HighPass, 28.0f, 0.0f, 0.54f }, { B::HighPass, 28.0f, 0.0f, 1.31f },
            { B::HighShelf, 15000.0f, 0.3f, 0.7f } },
        0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };

    static const DeviceProfile bigMains { 1.0f, true, {
            { B::HighPass, 22.0f, 0.0f, 0.54f }, { B::HighPass, 22.0f, 0.0f, 1.31f },
            { B::LowShelf, 60.0f, 2.5f, 0.7f },  { B::Peak, 3000.0f, -1.0f, 1.2f },
            { B::HighShelf, 10000.0f, -1.5f, 0.7f } },
        0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };

    static const DeviceProfile hifi { 1.0f, true, {
            { B::HighPass, 35.0f, 0.0f, 0.54f }, { B::HighPass, 35.0f, 0.0f, 1.31f },
            { B::LowShelf, 100.0f, 2.0f, 0.7f }, { B::Peak, 250.0f, 1.5f, 1.0f },
            { B::Peak, 3000.0f, -1.5f, 1.0f },   { B::HighShelf, 10000.0f, 1.5f, 0.7f } },
        0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };

    switch (i)
    {
        case 1:  return classicNf;
        case 2:  return cube;
        case 3:  return modernNf;
        case 4:  return reference;
        case 5:  return bigMains;
        case 6:  return hifi;
        default: return none;
    }
}

class DeviceSim
{
public:
    static constexpr int maxBands = 8;

    explicit DeviceSim (ProfileFn fn = &deviceProfileByIndex) : getProfile (fn) {}

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        fade.reset (sr, 0.015);
        fade.setCurrentAndTargetValue (1.0f);
        active = pending = 0;
        configure (0);
    }

    // Applica la simulazione. Il cambio di dispositivo avviene con una
    // dissolvenza rapida (15 ms giu', cambio, 15 ms su) per non fare click.
    void process (float* L, float* R, int n, int requested) noexcept
    {
        if (requested != pending)
        {
            pending = requested;
            fade.setTargetValue (pending == active ? 1.0f : 0.0f);
        }

        for (int i = 0; i < n; ++i)
        {
            if (pending != active && ! fade.isSmoothing() && fade.getCurrentValue() <= 0.0f)
            {
                configure (pending);
                fade.setTargetValue (1.0f);
            }

            float l = L[i], r = R[i];

            if (active != 0)
            {
                const auto& p = *profile;

                // stereo -> mono / stereo stretto
                const float m = 0.5f * (l + r), s = 0.5f * (l - r) * p.width;
                l = m + s; r = m - s;

                for (int b = 0; b < numBands; ++b)
                {
                    l = eqL[(size_t) b].processSample (l);
                    r = eqR[(size_t) b].processSample (r);
                }

                // compressore stereo collegato (rivelatore di picco)
                if (p.ratio > 1.0f)
                {
                    const float peak = juce::jmax (std::abs (l), std::abs (r));
                    const float c = peak > env ? attCoef : relCoef;
                    env = c * env + (1.0f - c) * peak;
                    const float envDb = juce::Decibels::gainToDecibels (env, -120.0f);
                    const float over = envDb - p.threshDb;
                    const float grDb = over > 0.0f ? over * (1.0f / p.ratio - 1.0f) : 0.0f;
                    const float g = juce::Decibels::decibelsToGain (grDb);
                    l *= g; r *= g;
                }

                if (p.drive > 1.0f)
                {
                    l = std::tanh (p.drive * l) / p.drive;
                    r = std::tanh (p.drive * r) / p.drive;
                }

                l *= makeup; r *= makeup;
            }

            const float f = fade.getNextValue();
            L[i] = l * f;
            R[i] = r * f;
        }
    }

    bool crossfeedAllowed() const noexcept { return profile->allowsCrossfeed; }
    int getActive() const noexcept { return active; }

private:
    // Biquad a campione singolo (Transposed Direct Form II)
    struct SampleBiquad
    {
        BiquadCoeffs c; double s1 = 0.0, s2 = 0.0;
        void reset() noexcept { s1 = s2 = 0.0; }
        float processSample (float xf) noexcept
        {
            const double x = xf, y = c.b0 * x + s1;
            s1 = c.b1 * x - c.a1 * y + s2;
            s2 = c.b2 * x - c.a2 * y;
            return (float) y;
        }
    };

    void configure (int d) noexcept
    {
        active = d;
        profile = &getProfile (d);
        numBands = juce::jmin ((int) profile->bands.size(), maxBands);

        for (int b = 0; b < numBands; ++b)
        {
            const auto c = designBand (profile->bands[(size_t) b], 1.0, sr);
            eqL[(size_t) b].c = c; eqR[(size_t) b].c = c;
            eqL[(size_t) b].reset(); eqR[(size_t) b].reset();
        }

        env = 0.0f;
        attCoef = (float) std::exp (-1.0 / (0.001 * profile->attackMs * sr));
        relCoef = (float) std::exp (-1.0 / (0.001 * profile->releaseMs * sr));
        makeup = juce::Decibels::decibelsToGain (profile->makeupDb);
    }

    double sr = 44100.0;
    ProfileFn getProfile;
    int active = 0, pending = 0;
    const DeviceProfile* profile = &getDeviceProfile (Device::Studio);
    std::array<SampleBiquad, maxBands> eqL, eqR;
    int numBands = 0;
    float env = 0.0f, attCoef = 0.0f, relCoef = 0.0f, makeup = 1.0f;
    juce::LinearSmoothedValue<float> fade;
};
} // namespace scriba
