// MFR-Mayo - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// MFR-Mayo signal chain: gold-foil style pickup, op-amp Big Muff style fuzz,
// JCM800-style power-amp drive, 4x12 cab, and the microphonic feedback loop,
// with a noise gate in front of it all:
//
//   guitar --+--> gate --> pickup --> fuzz --> amp --> cab --+--> out
//            ^                                               |
//            +--------- resonator <-- air delay <------------+
//
// The whistle is not triggered by anything. While strings ring, the fuzz is
// saturated and the loop has too little gain to sustain itself; when they stop,
// the fuzz opens up and the loop runs away at the resonator's pitch, and the
// fuzz squares it into odd harmonics.
//
// Pure C++ (no JUCE) so tools/MayoTest can run it offline. Everything runs at
// the oversampled rate.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mayo
{

constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

//==============================================================================
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }

    // RBJ audio EQ cookbook designs.
    void lowpass (double fs, double f, double q)
    {
        const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double fs, double f, double q)
    {
        const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void bandpass (double fs, double f, double q)   // 0 dB at the centre
    {
        const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    void peaking (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowshelf (double fs, double f, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos (w);
        const double al = std::sin (w) / 2 * std::sqrt (2.0), s = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
             (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
    }
    void highshelf (double fs, double f, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos (w);
        const double al = std::sin (w) / 2 * std::sqrt (2.0), s = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
             (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
    }

private:
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = float (B0 / A0); b1 = float (B1 / A0); b2 = float (B2 / A0);
        a1 = float (A1 / A0); a2 = float (A2 / A0);
    }
};

struct OnePole
{
    float a = 0, z = 0;
    void setCutoff (double fs, double f) { a = float (1 - std::exp (-2 * kPi * f / fs)); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { z += a * (x - z); return x - z; }
    void reset() { z = 0; }
};

// Symmetric silicon-diode style soft clipper: unity gain for small signals,
// levels off at +-1. Symmetric, so it adds odd harmonics only.
inline float diodeClip (float x) { return x / std::sqrt (1.0f + x * x); }

//==============================================================================
// Op-amp Big Muff style: input boost, two clipping stages whose gain rolls off
// above ~1.2-1.8 kHz (the Miller caps), then the LP/HP blend tone control that
// scoops ~1 kHz at noon. Stage corners follow the ElectroSmash analysis.
class Fuzz
{
public:
    void prepare (double fs)
    {
        inHP.setCutoff (fs, 30);
        s1HP.setCutoff (fs, 55);   s1LP.setCutoff (fs, 1780); s1Post.setCutoff (fs, 7000);
        s2HP.setCutoff (fs, 94);   s2LP.setCutoff (fs, 1170); s2Post.setCutoff (fs, 7000);
        toneLP.setCutoff (fs, 408); toneHP.setCutoff (fs, 1800);
        outHP.setCutoff (fs, 20);
        reset();
    }
    void reset() { for (auto* p : { &inHP, &s1HP, &s1LP, &s1Post, &s2HP, &s2LP, &s2Post, &toneLP, &toneHP, &outHP }) p->reset(); }

    // sustain, tone: 0..1. volume: linear gain.
    void set (float sustain, float tone, float volume)
    {
        g1 = 1.0f + 90.0f * sustain * sustain;   // 0 .. ~39 dB into the first clipper
        t = tone;
        vol = volume;
    }

    float process (float x)
    {
        x = 6.8f * inHP.hp (x);                               // input booster (~17 dB)

        float y = s1HP.hp (x);
        y = diodeClip (y + (g1 - 1.0f) * s1LP.lp (y));        // gain falls to unity above the Miller corner
        y = s1Post.lp (y);

        y = s2HP.hp (y);
        // ~25 dB. The small offset models mismatched diodes/op-amp swing: a little even-harmonic content.
        y = diodeClip (y + 17.0f * s2LP.lp (y) + kAsym) - kAsymOut;
        y = s2Post.lp (y);

        const float lo = toneLP.lp (y), hi = toneHP.hp (y);
        y = (1.0f - t) * lo + t * 1.4f * hi;
        return outHP.hp (y) * vol;
    }

private:
    OnePole inHP, s1HP, s1LP, s1Post, s2HP, s2LP, s2Post, toneLP, toneHP, outHP;
    static constexpr float kAsym = 0.55f;
    const float kAsymOut = kAsym / std::sqrt (1.0f + kAsym * kAsym);
    float g1 = 30, t = 0.5f, vol = 1;
};

//==============================================================================
// Marshall-style amp, driven the classic 90s way: low-sensitivity input, preamp
// barely on, master up, so most of the grit is the power stage.
class Amp
{
public:
    void prepare (double sampleRate) { fs = sampleRate; couple.setCutoff (fs, 60); reset(); }
    void reset() { couple.reset(); for (auto* b : { &bass, &mid, &treble, &presence }) b->reset(); }

    // drive 0..1, eq values in dB (+-12)
    void set (float driveKnob, float bassDb, float midDb, float trebleDb, float presenceDb)
    {
        drive = 1.0f + 24.0f * driveKnob * driveKnob;
        norm = 1.0f / (0.5f + 0.5f * std::sqrt (drive));
        bass.lowshelf (fs, 120, bassDb);
        mid.peaking (fs, 650, 0.7, midDb);
        treble.highshelf (fs, 2200, trebleDb);
        presence.highshelf (fs, 3500, presenceDb);
    }

    float process (float x)
    {
        x = couple.hp (x) * 1.6f;
        x = std::tanh (x + 0.12f) - 0.1194f;                  // lightly driven, slightly asymmetric preamp
        x = treble.process (mid.process (bass.process (x)));
        x = std::tanh (x * drive);                            // push-pull power stage
        return presence.process (x) * norm;
    }

private:
    double fs = 192000;
    OnePole couple;
    Biquad bass, mid, treble, presence;
    float drive = 4, norm = 0.5f;
};

//==============================================================================
// Rough 4x12 closed-back response: low cut, box thump, upper-mid bite, steep top roll-off.
class Cab
{
public:
    void prepare (double fs)
    {
        hp.highpass (fs, 75, 0.8);
        thump.peaking (fs, 110, 1.2, 3.0);
        bite.peaking (fs, 2500, 1.0, 4.0);
        lp1.lowpass (fs, 5000, 0.7);
        lp2.lowpass (fs, 6500, 0.6);
        reset();
    }
    void reset() { for (auto* b : { &hp, &thump, &bite, &lp1, &lp2 }) b->reset(); }
    float process (float x) { return lp2.process (lp1.process (bite.process (thump.process (hp.process (x))))); }

private:
    Biquad hp, thump, bite, lp1, lp2;
};

//==============================================================================
// The microphonic path: sound from the cab travels through the air (a short
// delay), shakes the loose pickup coil (a resonator) and re-enters the chain.
class MicroLoop
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        buffer.assign ((size_t) (fs * 0.04) + 4, 0.0f);
        pos = 0;
        driftCoef = float (1 - std::exp (-2 * kPi * 0.6 / fs));
        reset();
    }
    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        res.reset();
        driftState = driftTarget = 0;
        countdown = 0;
    }

    // pitchHz: resonator centre. distanceMs: air delay. drift 0..1. bloom 0..1. loopGain: linear.
    void set (float pitchHz, float distanceMs, float driftAmt, float bloom, float loopGainLin)
    {
        pitch = pitchHz;
        delaySamples = std::max (2.0f, distanceMs * 0.001f * (float) fs);
        drift = driftAmt;
        q = 2.0f + 28.0f * bloom * bloom;
        gain = loopGainLin;
        countdown = 0;   // recompute the resonator now
    }

    float read()
    {
        // Slow random wander of the path length and the coil's resonance: pitch glides and mode hops.
        if (--countdown <= 0)
        {
            countdown = 64;
            rng = rng * 1664525u + 1013904223u;
            if ((rng >> 24) < 2)                                  // new target every ~0.5 s on average
                driftTarget = ((float) ((rng >> 8) & 0xffff) / 32768.0f - 1.0f);
            res.bandpass (fs, pitch * (1.0f + 0.03f * drift * driftState), q);
        }
        driftState += 64.0f * driftCoef * (driftTarget - driftState) * (1.0f / 64.0f);

        const float d = delaySamples * (1.0f + 0.04f * drift * driftState);
        float rp = (float) pos - d;
        while (rp < 0) rp += (float) buffer.size();
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % (int) buffer.size();
        const float frac = rp - (float) i0;
        const float air = buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);

        ring = res.process (air);
        return ring * gain;
    }

    void write (float acoustic)
    {
        buffer[(size_t) pos] = acoustic;
        if (++pos == (int) buffer.size())
            pos = 0;
    }

    float lastRing() const { return ring; }

private:
    double fs = 192000;
    std::vector<float> buffer;
    int pos = 0, countdown = 0;
    Biquad res;
    float pitch = 490, delaySamples = 1600, drift = 0.3f, q = 10, gain = 0, ring = 0;
    float driftState = 0, driftTarget = 0, driftCoef = 0;
    uint32_t rng = 0x12345678u;
};

//==============================================================================
// GGate-style noise gate. It listens to the guitar alone (a real pickup would
// hear the whistle too, which would hold the gate open forever) and, once the
// guitar falls below the threshold, fades out everything entering the chain,
// whistle included, and the chain's output as well.
// Attack = time to open fully, Fade = time to close fully.
class Gate
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        envDecay = float (std::exp (-1.0 / (0.02 * fs)));   // ~20 ms peak decay, stops chatter
        reset();
    }
    void reset() { env = 0; gain = 0; }                       // starts closed: no whistle on load

    void set (bool enabled, float thresholdDb, float attackMs, float fadeS)
    {
        on = enabled;
        threshold = dbToGain (thresholdDb);
        openStep = attackMs <= 0.0f ? 1.0f : 1.0f / (attackMs * 0.001f * (float) fs);
        closeStep = 1.0f / (std::max (fadeS, 0.001f) * (float) fs);
    }

    float process (float detector)
    {
        if (! on)
            return gain = 1.0f;
        env = std::max (std::abs (detector), env * envDecay);
        gain = env > threshold ? std::min (1.0f, gain + openStep) : std::max (0.0f, gain - closeStep);
        return gain;
    }

    float getGain() const { return gain; }

private:
    double fs = 192000;
    bool on = true;
    float threshold = 0, openStep = 1, closeStep = 0, env = 0, gain = 0, envDecay = 0;
};

//==============================================================================
struct Params
{
    bool  gateOn = true;
    float gateThresholdDb = -40, gateAttackMs = 2, gateFadeS = 0.4f;
    float inputDb = 0;
    float sustain = 0.7f, tone = 0.45f, fuzzVolumeDb = 0;
    bool  ampOn = true;
    float drive = 0.5f, bassDb = 0, midDb = 0, trebleDb = 0, presenceDb = 0;
    float squeal = 0.6f;       // 0..1, 0 = no feedback
    float pitchHz = 490, distanceMs = 8.3f, drift = 0.3f, bloom = 0.35f;
    float outputDb = 0;
};

// Maps the Squeal knob to the microphonic loop gain. Calibrated with tools/MayoTest.
inline float squealToLoopGain (float squeal)
{
    if (squeal <= 0.0f)
        return 0.0f;
    return dbToGain (-90.0f + 70.0f * squeal);
}

class Chain
{
public:
    void prepare (double oversampledRate)
    {
        fs = oversampledRate;
        pickup.lowpass (fs, 5200, 1.6);        // gold-foil: bright, glassy, a little peaky
        gate.prepare (fs);
        fuzz.prepare (fs);
        amp.prepare (fs);
        cab.prepare (fs);
        loop.prepare (fs);
        meterCoef = float (1 - std::exp (-1.0 / (0.05 * fs)));
        setParams (params);
        reset();
    }

    void reset()
    {
        gate.reset(); pickup.reset(); fuzz.reset(); amp.reset(); cab.reset(); loop.reset();
        meter = 0;
    }

    void setParams (const Params& p)
    {
        params = p;
        gate.set (p.gateOn, p.gateThresholdDb, p.gateAttackMs, p.gateFadeS);
        inGain = dbToGain (p.inputDb);
        fuzz.set (p.sustain, p.tone, dbToGain (p.fuzzVolumeDb));
        amp.set (p.drive, p.bassDb, p.midDb, p.trebleDb, p.presenceDb);
        loop.set (p.pitchHz, p.distanceMs, p.drift, p.bloom, squealToLoopGain (p.squeal));
        outGain = dbToGain (p.outputDb);
    }

    float process (float guitar)
    {
        // A pickup is never perfectly silent; this hiss (about -100 dBFS) seeds the feedback.
        noise = noise * 1664525u + 1013904223u;
        const float hiss = ((float) (noise >> 9) / 4194304.0f - 1.0f) * 1.0e-5f;

        // Gate first: it hears only the guitar, but closes on everything coming out of the pickup.
        const float g = gate.process (guitar);
        const float micro = loop.read();
        const float p = pickup.process ((guitar + micro + hiss) * g * inGain);
        const float f = fuzz.process (p);
        const float c = cab.process (amp.process (f));
        loop.write (c);   // the loop always runs through the internal amp and cab, even when the amp is bypassed

        const float r = std::abs (loop.lastRing());
        meter += meterCoef * (r - meter);

        // The gate also fades the output, so closing it is heard as a fade rather than a
        // hold-then-drop (the fuzz would otherwise keep the whistle at full level until the
        // loop collapses). Squared for a fade that sounds even rather than hanging then dropping.
        const float out = (params.ampOn ? c : f) * outGain * g * g;
        return std::isfinite (out) ? out : (reset(), 0.0f);
    }

    // Level of the whistle band arriving at the pickup, for the UI meter.
    float getSquealMeter() const { return meter; }
    float getGateGain() const { return gate.getGain(); }

private:
    double fs = 192000;
    Params params;
    Biquad pickup;
    Gate gate;
    Fuzz fuzz;
    Amp amp;
    Cab cab;
    MicroLoop loop;
    float inGain = 1, outGain = 1, meter = 0, meterCoef = 0;
    uint32_t noise = 0x9e3779b9u;
};

} // namespace mayo
