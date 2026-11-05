#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <cmath>

namespace scriba
{
// ----------------------------------------------------------------------------
// Monitor virtuali: una coppia di casse davanti all'ascoltatore, in una stanza.
//
//  * ogni cassa arriva a ENTRAMBE le orecchie: l'orecchio lontano la riceve in
//    ritardo (ITD, formula di Woodworth, su tutta la banda come nelle casse vere)
//    e con meno acuti (ombra della testa, modello sferico di Brown & Duda);
//  * lavora in mid/side: il centro (mid) resta intatto, il modello della testa
//    agisce solo sul side -> voce, cassa e basso al centro non cambiano timbro;
//  * Amount = quanto segnale arriva all'orecchio opposto (0 = cuffia pura,
//    100 % = casse reali);
//  * Room = prime riflessioni (scrivania, pareti, soffitto) + coda diffusa
//    decorrelata (FDN a 8 linee), che portano il suono fuori dalla testa.
//
// Taratura: confronto con CLA Nx e HoRNet VHS sullo stesso brano
// (vedi docs del progetto: analisi-confronto-cla-nx-hornet-vhs).
// ----------------------------------------------------------------------------
class Crossfeed
{
public:
    // tarati sul confronto con CLA Nx / HoRNet VHS: Room 35 % ~ -12 dB, Room 100 % ~ 0 dB di stanza
    static constexpr float erGainMax = 0.9f, tailGainMax = 1.8f;

    void prepare (double newSampleRate)
    {
        sr = newSampleRate;

        itdSize = (int) std::ceil (0.0012 * sr) + 4;
        itdBuf.assign ((size_t) itdSize, 0.0f);

        erSize = (int) std::ceil (0.03 * sr) + 4;
        erL.assign ((size_t) erSize, 0.0f);
        erR.assign ((size_t) erSize, 0.0f);

        itdPos = erPos = 0;
        ipsi.reset(); contra.reset(); norm.reset();
        erLpL = erLpR = 0.0f;

        for (auto& r : reflections)
        {
            r.samplesL = (int) std::round (r.ms * 0.001 * sr);
            r.samplesR = (int) std::round (r.ms * 1.07 * 0.001 * sr);
        }

        prepareTail();

        mix.reset (sr, 0.03);            mix.setCurrentAndTargetValue (0.0f);
        erGain.reset (sr, 0.05);         erGain.setCurrentAndTargetValue (0.0f);
        tailGain.reset (sr, 0.05);       tailGain.setCurrentAndTargetValue (0.0f);
        directGain.reset (sr, 0.05);     directGain.setCurrentAndTargetValue (1.0f);
        delaySmooth.reset (sr, 0.05);    delaySmooth.setCurrentAndTargetValue (0.0f);

        currentAngle = currentAmount = currentRoom = -1.0f;
        erLpCoef = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 6000.0 / sr);
        erHpCoef = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 250.0 / sr);
        erHpL = erHpR = erHp2L = erHp2R = 0.0f;
    }

    // enabled: crossfeed acceso; angleDeg: 0..90; amount: 0..1; room: 0..1
    void process (float* L, float* R, int n, bool enabled, float angleDeg, float amount, float room) noexcept
    {
        mix.setTargetValue (enabled ? 1.0f : 0.0f);

        if (! enabled && ! mix.isSmoothing() && mix.getCurrentValue() <= 0.0f)
        {
            feedIdle (L, R, n);
            return;
        }

        if (std::abs (angleDeg - currentAngle) > 0.05f || std::abs (amount - currentAmount) > 0.002f)
            updateHeadModel (angleDeg, amount);

        if (std::abs (room - currentRoom) > 0.002f)
            updateRoom (room);

        for (int i = 0; i < n; ++i)
        {
            const float xl = L[i], xr = R[i];
            const float d = delaySmooth.getNextValue();

            // --- testa: side' = N * (I - g*C*ITD) / (1 + g) * side
            const float mid  = 0.5f * (xl + xr);
            const float side = 0.5f * (xl - xr);

            const float nearEar = ipsi.process (side);
            itdBuf[(size_t) itdPos] = contra.process (side);
            const float farEar = readFrac (itdBuf, itdSize, itdPos, d);
            itdPos = (itdPos + 1) % itdSize;

            const float sideOut = norm.process ((nearEar - g * farEar) * invOnePlusG);

            float earL = mid + sideOut;
            float earR = mid - sideOut;

            // --- stanza
            erL[(size_t) erPos] = xl;
            erR[(size_t) erPos] = xr;
            const float eg = erGain.getNextValue();
            const float tg = tailGain.getNextValue();
            const float dg = directGain.getNextValue();

            if (eg > 1.0e-5f || tg > 1.0e-5f)
            {
                float sl = 0.0f, sr2 = 0.0f;
                for (const auto& t : reflections)
                {
                    const auto& forL = t.opposite ? erR : erL;
                    const auto& forR = t.opposite ? erL : erR;
                    sl  += t.gain * forL[(size_t) wrap (erPos - t.samplesL)];
                    sr2 += t.gain * forR[(size_t) wrap (erPos - t.samplesR)];
                }
                erLpL = (1.0f - erLpCoef) * sl  + erLpCoef * erLpL;
                erLpR = (1.0f - erLpCoef) * sr2 + erLpCoef * erLpR;

                // niente bassi nelle riflessioni: i bassi restano mono e asciutti
                // (due poli a 250 Hz, come la stanza di CLA Nx che tiene mono i bassi)
                erHpL = (1.0f - erHpCoef) * erLpL + erHpCoef * erHpL;
                erHpR = (1.0f - erHpCoef) * erLpR + erHpCoef * erHpR;
                const float h1L = erLpL - erHpL, h1R = erLpR - erHpR;
                erHp2L = (1.0f - erHpCoef) * h1L + erHpCoef * erHp2L;
                erHp2R = (1.0f - erHpCoef) * h1R + erHpCoef * erHp2R;
                const float refL = h1L - erHp2L, refR = h1R - erHp2R;

                // la coda parte dalle riflessioni (gia' ritardate e filtrate)
                float tl = 0.0f, tr = 0.0f;
                processTail (refL, refR, tl, tr);

                earL = dg * earL + eg * refL + tg * tl;
                earR = dg * earR + eg * refR + tg * tr;
            }
            erPos = (erPos + 1) % erSize;

            const float m = mix.getNextValue();
            L[i] = xl + m * (earL - xl);
            R[i] = xr + m * (earR - xr);
        }
    }

private:
    // ---------------------------------------------------------------- filtri
    struct OnePole
    {
        float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f, z = 0.0f;
        void reset() noexcept { z = 0.0f; }
        float process (float x) noexcept
        {
            const float y = b0 * x + z;
            z = b1 * x - a1 * y;
            return y;
        }
        // H(s) = (1 + num * s/w) / (1 + den * s/w), k = K/w (bilineare con prewarp)
        void set (double num, double den, double k) noexcept
        {
            const double a0 = 1.0 + den * k;
            b0 = (float) ((1.0 + num * k) / a0);
            b1 = (float) ((1.0 - num * k) / a0);
            a1 = (float) ((1.0 - den * k) / a0);
        }
    };

    struct Reflection { float ms, gain; bool opposite; int samplesL = 0, samplesR = 0; };

    // Brown & Duda: alpha(theta) = (1 + amin/2) + (1 - amin/2) cos(theta / thetaMin * 180 deg)
    static double alphaFor (double thetaDeg)
    {
        constexpr double amin = 0.1, thetaMin = 150.0;
        const double t = juce::jmin (thetaDeg, thetaMin);
        return (1.0 + amin / 2.0) + (1.0 - amin / 2.0) * std::cos (t / thetaMin * juce::MathConstants<double>::pi);
    }

    // Woodworth: ritardo di arrivo a un orecchio rispetto al centro della testa
    static double woodworth (double thetaDeg)
    {
        constexpr double aOverC = 0.0875 / 343.0;
        const double t = juce::degreesToRadians (thetaDeg);
        const double halfPi = juce::MathConstants<double>::halfPi;
        return t < halfPi ? -aOverC * std::cos (t) : aOverC * (t - halfPi);
    }

    void updateHeadModel (float angleDeg, float amount)
    {
        currentAngle = angleDeg;
        currentAmount = amount;
        g = amount;
        invOnePlusG = 1.0f / (1.0f + g);

        const double half = 0.5 * angleDeg;
        constexpr double earDeg = 100.0;
        const double thetaIpsi   = earDeg - half;
        const double thetaContra = earDeg + half;

        const double aI = alphaFor (thetaIpsi);
        const double aC = alphaFor (thetaContra);
        const double aM = (aI + g * aC) / (1.0 + g);   // risposta del centro da compensare

        const double w = 2.0 * 343.0 / 0.0875;           // polo del modello (circa 1,25 kHz)
        const double k = (w / std::tan (w / (2.0 * sr))) / w;

        ipsi.set (aI, 1.0, k);
        contra.set (aC, 1.0, k);
        norm.set (1.0, aM, k);

        const double itd = juce::jmax (0.0, woodworth (thetaContra) - woodworth (thetaIpsi));
        delaySmooth.setTargetValue ((float) (itd * sr));
    }

    void updateRoom (float room)
    {
        currentRoom = room;
        erGain.setTargetValue (erGainMax * room);
        tailGain.setTargetValue (tailGainMax * std::pow (room, 1.2f));
        directGain.setTargetValue (1.0f - 0.35f * room);

        // RT60 da 0,15 s (stanza piccola e trattata) a 0,35 s
        const double rt60 = 0.15 + 0.20 * room;
        for (size_t i = 0; i < tailLines.size(); ++i)
            tailLines[i].feedback = (float) std::pow (10.0, -3.0 * (double) tailLines[i].length / (rt60 * sr));
    }

    // ---------------------------------------------------------------- coda (FDN 8x8 Householder)
    struct TailLine
    {
        std::vector<float> buf;
        int length = 1, pos = 0;
        float feedback = 0.0f, lp = 0.0f;
    };

    void prepareTail()
    {
        static constexpr int lengths48k[8] { 1031, 1327, 1523, 1709, 1913, 2129, 2357, 2549 };
        for (size_t i = 0; i < tailLines.size(); ++i)
        {
            auto& t = tailLines[i];
            t.length = juce::jmax (1, (int) std::round (lengths48k[i] * sr / 48000.0));
            t.buf.assign ((size_t) t.length, 0.0f);
            t.pos = 0; t.lp = 0.0f; t.feedback = 0.0f;
        }
        tailDamp = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 5000.0 / sr);
        tailHpCoef = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 180.0 / sr);
        tailHpL = tailHpR = 0.0f;
    }

    void processTail (float inL, float inR, float& outL, float& outR) noexcept
    {
        // niente bassi nella coda: i bassi restano asciutti e precisi
        tailHpL = (1.0f - tailHpCoef) * inL + tailHpCoef * tailHpL;
        tailHpR = (1.0f - tailHpCoef) * inR + tailHpCoef * tailHpR;
        const float hl = inL - tailHpL, hr = inR - tailHpR;

        std::array<float, 8> y {};
        float sum = 0.0f;
        for (size_t i = 0; i < 8; ++i)
        {
            auto& t = tailLines[i];
            float v = t.buf[(size_t) t.pos];
            t.lp = (1.0f - tailDamp) * v + tailDamp * t.lp;   // assorbimento degli acuti
            y[i] = t.lp * t.feedback;
            sum += y[i];
        }

        const float h = 2.0f / 8.0f * sum;   // matrice di Householder: y - 2/N * somma
        static constexpr float inSignL[8] { 1, 1, 1, 1, 0, 0, 0, 0 };
        static constexpr float inSignR[8] { 0, 0, 0, 0, 1, 1, 1, 1 };
        static constexpr float outL8[8]   { 1, -1, 1, -1, 1, -1, 1, -1 };
        static constexpr float outR8[8]   { 1, 1, -1, -1, 1, 1, -1, -1 };

        outL = outR = 0.0f;
        for (size_t i = 0; i < 8; ++i)
        {
            auto& t = tailLines[i];
            const float v = y[i] - h + 0.35f * (inSignL[i] * hl + inSignR[i] * hr);
            t.buf[(size_t) t.pos] = v;
            t.pos = (t.pos + 1) % t.length;
            outL += outL8[i] * y[i];
            outR += outR8[i] * y[i];
        }
        outL *= 0.5f; outR *= 0.5f;
    }

    void feedIdle (const float* L, const float* R, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            erL[(size_t) erPos] = L[i];
            erR[(size_t) erPos] = R[i];
            erPos = (erPos + 1) % erSize;
        }
    }

    int wrap (int p) const noexcept { return p < 0 ? p + erSize : p; }

    static float readFrac (const std::vector<float>& buf, int size, int writePos, float delay) noexcept
    {
        float rp = (float) writePos - delay;
        if (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp % size;
        const int i1 = (i0 + 1) % size;
        const float frac = rp - std::floor (rp);
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * frac;
    }

    double sr = 44100.0;
    OnePole ipsi, contra, norm;
    float g = 1.0f, invOnePlusG = 0.5f;
    std::vector<float> itdBuf, erL, erR;
    int itdSize = 1, erSize = 1, itdPos = 0, erPos = 0;
    float erLpL = 0.0f, erLpR = 0.0f, erLpCoef = 0.0f, erHpL = 0.0f, erHpR = 0.0f, erHp2L = 0.0f, erHp2R = 0.0f, erHpCoef = 0.0f;
    float currentAngle = -1.0f, currentAmount = -1.0f, currentRoom = -1.0f;
    juce::LinearSmoothedValue<float> mix, erGain, tailGain, directGain, delaySmooth;

    std::array<TailLine, 8> tailLines;
    float tailDamp = 0.0f, tailHpCoef = 0.0f, tailHpL = 0.0f, tailHpR = 0.0f;

    // prime riflessioni: scrivania, pareti laterali, soffitto, parete di fondo
    std::array<Reflection, 10> reflections { {
        { 1.25f, 0.60f, false }, { 2.2f, 0.50f, true },  { 3.5f, 0.45f, false }, { 4.8f, 0.40f, true },
        { 6.2f, 0.34f, false },  { 7.9f, 0.28f, true },  { 9.8f, 0.24f, false }, { 12.6f, 0.20f, true },
        { 15.9f, 0.16f, false }, { 19.7f, 0.12f, true } } };
};
} // namespace scriba
