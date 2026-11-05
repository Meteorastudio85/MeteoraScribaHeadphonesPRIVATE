#pragma once
#include "EqTypes.h"
#include <cmath>
#include <complex>

namespace scriba
{
struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

// Formule RBJ (Audio EQ Cookbook), le stesse convenzioni usate da AutoEq.
// gainScale scala il guadagno della banda (manopola Amount, 0..1).
inline BiquadCoeffs designBand (const EqBand& band, double gainScale, double sampleRate)
{
    const double pi    = 3.14159265358979323846;
    const double freq  = std::min ((double) band.freq, sampleRate * 0.49);
    const double A     = std::pow (10.0, (band.gainDb * gainScale) / 40.0);
    const double w0    = 2.0 * pi * freq / sampleRate;
    const double cw    = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * band.q);
    const double sA    = std::sqrt (A);

    double b0, b1, b2, a0, a1, a2;

    switch (band.type)
    {
        case EqBand::LowShelf:
            b0 =        A * ((A + 1) - (A - 1) * cw + 2 * sA * alpha);
            b1 =  2.0 * A * ((A - 1) - (A + 1) * cw);
            b2 =        A * ((A + 1) - (A - 1) * cw - 2 * sA * alpha);
            a0 =             (A + 1) + (A - 1) * cw + 2 * sA * alpha;
            a1 = -2.0 *     ((A - 1) + (A + 1) * cw);
            a2 =             (A + 1) + (A - 1) * cw - 2 * sA * alpha;
            break;

        case EqBand::HighShelf:
            b0 =        A * ((A + 1) + (A - 1) * cw + 2 * sA * alpha);
            b1 = -2.0 * A * ((A - 1) + (A + 1) * cw);
            b2 =        A * ((A + 1) + (A - 1) * cw - 2 * sA * alpha);
            a0 =             (A + 1) - (A - 1) * cw + 2 * sA * alpha;
            a1 =  2.0 *     ((A - 1) - (A + 1) * cw);
            a2 =             (A + 1) - (A - 1) * cw - 2 * sA * alpha;
            break;

        case EqBand::HighPass:
            b0 =  (1 + cw) / 2;
            b1 = -(1 + cw);
            b2 =  (1 + cw) / 2;
            a0 =   1 + alpha;
            a1 =  -2 * cw;
            a2 =   1 - alpha;
            break;

        case EqBand::LowPass:
            b0 = (1 - cw) / 2;
            b1 =  1 - cw;
            b2 = (1 - cw) / 2;
            a0 =  1 + alpha;
            a1 = -2 * cw;
            a2 =  1 - alpha;
            break;

        case EqBand::Peak:
        default:
            b0 = 1 + alpha * A;
            b1 = -2 * cw;
            b2 = 1 - alpha * A;
            a0 = 1 + alpha / A;
            a1 = -2 * cw;
            a2 = 1 - alpha / A;
            break;
    }

    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

inline double magnitudeDb (const BiquadCoeffs& c, double freq, double sampleRate)
{
    const double w = 2.0 * 3.14159265358979323846 * freq / sampleRate;
    const std::complex<double> z1 = std::polar (1.0, -w);
    const std::complex<double> z2 = z1 * z1;
    const auto h = (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
    return 20.0 * std::log10 (std::max (std::abs (h), 1.0e-9));
}

// Biquad in forma Transposed Direct Form II, nessuna allocazione: sicuro sul thread audio.
class Biquad
{
public:
    void setCoeffs (const BiquadCoeffs& newCoeffs) noexcept { c = newCoeffs; }
    void reset() noexcept { s1 = s2 = 0.0; }

    float processSample (float xf) noexcept
    {
        const double x = xf;
        const double y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return (float) y;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const double x = data[i];
            const double y = c.b0 * x + s1;
            s1 = c.b1 * x - c.a1 * y + s2;
            s2 = c.b2 * x - c.a2 * y;
            data[i] = (float) y;
        }
    }

private:
    BiquadCoeffs c;
    double s1 = 0.0, s2 = 0.0;
};
} // namespace scriba
