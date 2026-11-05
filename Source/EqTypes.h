#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace scriba
{
struct EqBand
{
    enum Type { Peak, LowShelf, HighShelf, HighPass, LowPass };
    Type type;
    float freq;
    float gainDb;
    float q;
};

struct HeadphoneProfile
{
    juce::String name;
    juce::String source;
    float preampDb = 0.0f;
    std::vector<EqBand> bands;
};

// Legge un file "ParametricEQ.txt" di AutoEq, es.:
//   Preamp: -6.0 dB
//   Filter 1: ON LSC Fc 105 Hz Gain 6.7 dB Q 0.70
inline bool parseAutoEqText (const juce::String& text, HeadphoneProfile& out, int maxBands)
{
    HeadphoneProfile p;
    auto lines = juce::StringArray::fromLines (text);

    for (auto line : lines)
    {
        line = line.trim();

        if (line.startsWithIgnoreCase ("Preamp:"))
        {
            p.preampDb = line.fromFirstOccurrenceOf (":", false, false).trim().getFloatValue();
            continue;
        }

        if (! line.startsWithIgnoreCase ("Filter"))
            continue;

        auto t = juce::StringArray::fromTokens (line, " \t", "");
        t.removeEmptyStrings();

        const int on = t.indexOf ("ON", true);
        if (on < 0 || on + 1 >= t.size())
            continue; // filtro OFF o riga non valida

        const auto typeStr = t[on + 1].toUpperCase();
        EqBand b { EqBand::Peak, 1000.0f, 0.0f, 0.707f };

        if (typeStr == "PK" || typeStr == "PEQ")               b.type = EqBand::Peak;
        else if (typeStr.startsWith ("LS"))                    b.type = EqBand::LowShelf;
        else if (typeStr.startsWith ("HS"))                    b.type = EqBand::HighShelf;
        else continue;

        auto valueAfter = [&t] (const char* key, float fallback)
        {
            const int i = t.indexOf (key, true);
            return (i >= 0 && i + 1 < t.size()) ? t[i + 1].getFloatValue() : fallback;
        };

        b.freq   = valueAfter ("Fc", 1000.0f);
        b.gainDb = valueAfter ("Gain", 0.0f);
        b.q      = valueAfter ("Q", 0.707f);

        if (b.freq <= 0.0f || b.q <= 0.0f)
            continue;

        if ((int) p.bands.size() < maxBands)
            p.bands.push_back (b);
    }

    if (p.bands.empty())
        return false;

    out = std::move (p);
    return true;
}
} // namespace scriba
