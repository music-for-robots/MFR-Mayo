// MFR-Mayo - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Offline checks for the MFR-Mayo chain, run directly at the oversampled rate:
//   1. silence: does the loop start whistling on its own, how fast, at what pitch, how odd?
//   2. play/stop: is the whistle held down while strings ring, and does it bloom on the stops?
//   3. squeal sweep: calibration table for the Squeal knob.
//   4. pitch knob tracking.
//   5. gate/choke: silent at rest, and how the whistle fades after a stop.
// Tests 1-4 measure the feedback loop itself, so they run with the gate off.
// Also renders mayo_demo.wav (a synthetic strummed chord with stops) for listening.
#include "../Source/MayoDSP.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>

using namespace mayo;

constexpr double kFs = 48000.0 * 4;   // plugin runs 4x oversampled at 48 kHz

// Karplus-Strong string; mute() damps it hard, like a palm or fretting-hand stop.
struct KsString
{
    std::vector<float> buf; size_t pos = 0; float damp = 0.996f; uint32_t rng;
    KsString (double hz, uint32_t seed) : buf ((size_t) (kFs / hz)), rng (seed) {}
    void pluck (float amp)
    {
        for (auto& s : buf) { rng = rng * 1664525u + 1013904223u; s = amp * ((float) (rng >> 9) / 4194304.0f - 1.0f); }
        damp = 0.9995f;
    }
    void mute() { damp = 0.2f; }
    float tick()
    {
        const size_t nx = (pos + 1) % buf.size();
        const float out = buf[pos];
        buf[pos] = damp * 0.5f * (buf[pos] + buf[nx]);
        pos = nx;
        return out;
    }
};

struct Guitar
{
    // An open-ish power chord low in the neck (D3, A3, D4), strummed.
    std::vector<KsString> strings { KsString (146.83, 1), KsString (220.0, 2), KsString (293.66, 3) };
    void strum (float amp) { for (auto& s : strings) s.pluck (amp); }
    void mute() { for (auto& s : strings) s.mute(); }
    float tick() { float y = 0; for (auto& s : strings) y += s.tick(); return y; }
};

static void writeWav (const std::string& path, const std::vector<float>& data, int sr)
{
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    const uint32_t bytes = (uint32_t) data.size() * 2;
    f.write ("RIFF", 4); u32 (36 + bytes); f.write ("WAVEfmt ", 8);
    u32 (16); u16 (1); u16 (1); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 2); u16 (2); u16 (16);
    f.write ("data", 4); u32 (bytes);
    for (float s : data)
    {
        const auto v = (int16_t) std::lround (std::clamp (s, -1.0f, 1.0f) * 32767.0f);
        f.write ((const char*) &v, 2);
    }
}

static double rmsDb (const std::vector<float>& x, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
    return 10 * std::log10 (s / double (b - a) + 1e-20);
}

static double goertzel (const std::vector<float>& x, size_t a, size_t b, double f)
{
    const double w = 2 * kPi * f / kFs, c = 2 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = a; i < b; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2 * kPi * double (i - a) / double (b - a));
        const double s0 = x[i] * win + c * s1 - s2; s2 = s1; s1 = s0;
    }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / double (b - a);
}

// Strongest fundamental between 150 and 1500 Hz (coarse Goertzel scan, then refine).
static double findF0 (const std::vector<float>& x, size_t a, size_t b)
{
    double best = 0, bf = 0;
    for (double f = 150; f < 1500; f += 4) { const double m = goertzel (x, a, b, f); if (m > best) { best = m; bf = f; } }
    for (double f = bf - 4; f <= bf + 4; f += 0.25) { const double m = goertzel (x, a, b, f); if (m > best) { best = m; bf = f; } }
    return bf;
}

struct SilenceResult { double onsetMs, levelDb, f0, oddEvenDb, strongestHarm; bool finite; };

static SilenceResult silenceTest (Params p, double seconds = 2.0)
{
    Chain ch; ch.prepare (kFs); ch.setParams (p);
    const size_t n = (size_t) (seconds * kFs);
    std::vector<float> y (n);
    bool finite = true;
    for (size_t i = 0; i < n; ++i) { y[i] = ch.process (0.0f); finite &= std::isfinite (y[i]); }

    const size_t tail = n - (size_t) (0.4 * kFs);
    SilenceResult r {};
    r.finite = finite;
    r.levelDb = rmsDb (y, tail, n);
    r.onsetMs = -1;
    const size_t win = (size_t) (0.005 * kFs);
    for (size_t i = 0; i + win < n; i += win)
        if (rmsDb (y, i, i + win) > r.levelDb - 6) { r.onsetMs = 1000.0 * i / kFs; break; }
    r.f0 = findF0 (y, tail, n);
    double odd = 0, even = 0, hmax = 0; int hbest = 1;
    for (int h = 1; h <= 9; ++h)
    {
        const double m = goertzel (y, tail, n, h * r.f0);
        (h % 2 ? odd : even) += m * m;
        if (m > hmax) { hmax = m; hbest = h; }
    }
    r.oddEvenDb = 10 * std::log10 (odd / (even + 1e-30));
    r.strongestHarm = hbest;
    return r;
}

// Strum for 1.2 s, stop for 0.4 s (the length of the stops in the reference), three times.
// Returns the output plus the whistle-band level (relative to squeal off) while playing and in the stops.
struct PlayStopResult { double whistlePlayingDb, whistleStopDb, bloomMs, f0; };

static std::vector<float> runPlayStop (Params p, int cycles = 3, double playS = 1.2, double stopS = 0.4)
{
    Chain ch; ch.prepare (kFs); ch.setParams (p);
    Guitar g;
    const size_t play = (size_t) (playS * kFs), stop = (size_t) (stopS * kFs);
    std::vector<float> y;
    y.reserve ((size_t) cycles * (play + stop));
    for (int c = 0; c < cycles; ++c)
    {
        g.strum (0.25f);
        for (size_t i = 0; i < play; ++i) y.push_back (ch.process (g.tick()));
        g.mute();
        for (size_t i = 0; i < stop; ++i) y.push_back (ch.process (g.tick()));
    }
    return y;
}

static PlayStopResult playStopTest (Params p)
{
    const double playS = 1.2, stopS = 0.4;
    const size_t play = (size_t) (playS * kFs), stop = (size_t) (stopS * kFs), cyc = play + stop;
    auto on = runPlayStop (p);
    Params off = p; off.squeal = 0;
    auto dry = runPlayStop (off);

    // Whistle pitch, from the last stop.
    const size_t lastStop = 2 * cyc + play;
    PlayStopResult r {};
    r.f0 = findF0 (on, lastStop + stop / 4, lastStop + stop);

    // Energy at the whistle's harmonics, with vs without the loop, in the middle of playing and in the stops.
    auto band = [&] (const std::vector<float>& x, size_t a, size_t b)
    {
        double e = 0; for (int h = 1; h <= 7; h += 2) { const double m = goertzel (x, a, b, h * r.f0); e += m * m; }
        return 10 * std::log10 (e + 1e-30);
    };
    double playDiff = 0, stopDiff = 0;
    for (int c = 1; c < 3; ++c)
    {
        const size_t pa = (size_t) c * cyc + play / 3, pb = (size_t) c * cyc + play - play / 6;
        const size_t sa = (size_t) c * cyc + play + stop / 4, sb = (size_t) c * cyc + play + stop;
        playDiff += band (on, pa, pb) - band (dry, pa, pb);
        stopDiff += band (on, sa, sb) - band (dry, sa, sb);
    }
    r.whistlePlayingDb = playDiff / 2;
    r.whistleStopDb = stopDiff / 2;

    // Time from the stop until the output is back within 6 dB of its stop level.
    const size_t s0 = lastStop, win = (size_t) (0.004 * kFs);
    const double settle = rmsDb (on, s0 + stop / 2, s0 + stop);
    r.bloomMs = -1;
    for (size_t i = s0 + (size_t) (0.02 * kFs); i + win < s0 + stop; i += win)
        if (rmsDb (on, i, i + win) > settle - 6) { r.bloomMs = 1000.0 * (i - s0) / kFs; break; }
    return r;
}

// Play 1.2 s, then stop for `stopS`; returns output level in windows after the stop.
static void chokeTest (Params p, double stopS)
{
    Chain ch; ch.prepare (kFs); ch.setParams (p);
    Guitar g;
    g.strum (0.25f);
    std::vector<float> y;
    for (size_t i = 0; i < (size_t) (1.2 * kFs); ++i) y.push_back (ch.process (g.tick()));
    g.mute();
    const size_t s0 = y.size();
    for (size_t i = 0; i < (size_t) (stopS * kFs); ++i) y.push_back (ch.process (g.tick()));
    std::printf ("   fade %.2f s:", p.gateFadeS);
    for (double t : { 0.05, 0.2, 0.4, 0.6, 1.0, 2.0, 3.0, 4.5, 5.5 })
        if (t + 0.05 <= stopS)
        {
            const size_t a = s0 + (size_t) (t * kFs), b = a + (size_t) (0.05 * kFs);
            std::printf ("  %.2fs %5.0f", t, rmsDb (y, a, b));
        }
    std::printf ("  (dBFS)\n");
}

int main()
{
    Params p;
    p.gateOn = false;   // tests 1-4 characterise the loop itself

    std::printf ("== 1. Silence (defaults: squeal %.2f, pitch %.0f Hz, distance %.1f ms)\n", p.squeal, p.pitchHz, p.distanceMs);
    auto t0 = std::chrono::steady_clock::now();
    const auto s = silenceTest (p);
    const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("   onset %.0f ms, level %.1f dBFS, f0 %.1f Hz, odd-even %+.1f dB, strongest harmonic %d (%.0f Hz), %s\n",
                 s.onsetMs, s.levelDb, s.f0, s.oddEvenDb, (int) s.strongestHarm, s.strongestHarm * s.f0, s.finite ? "finite" : "NON-FINITE");
    std::printf ("   speed: 2 s of audio in %.0f ms (%.1f%% of one core at 48 kHz x4)\n", ms, ms / 2000.0 * 100.0);

    std::printf ("\n== 2. Play/stop (defaults)\n");
    const auto ps = playStopTest (p);
    std::printf ("   whistle vs squeal-off: while playing %+.1f dB, in stops %+.1f dB; bloom %.0f ms; f0 %.1f Hz\n",
                 ps.whistlePlayingDb, ps.whistleStopDb, ps.bloomMs, ps.f0);

    std::printf ("\n== 3. Squeal sweep\n   squeal  loop dB | silence: onset   level   f0      odd-even | playing  stops   bloom\n");
    for (float sq = 0.1f; sq <= 1.001f; sq += 0.1f)
    {
        Params q = p; q.squeal = sq;
        const auto a = silenceTest (q, 1.5);
        const auto b = playStopTest (q);
        std::printf ("   %.1f    %6.1f  | %7.0f ms %6.1f %7.1f %+6.1f   | %+6.1f  %+6.1f  %5.0f ms\n",
                     sq, 20 * std::log10 (squealToLoopGain (sq)), a.onsetMs, a.levelDb, a.f0, a.oddEvenDb,
                     b.whistlePlayingDb, b.whistleStopDb, b.bloomMs);
    }

    std::printf ("\n== 4. Pitch knob tracking (silence)\n");
    for (float hz : { 300.0f, 400.0f, 490.0f, 600.0f, 800.0f, 1100.0f })
    {
        Params q = p; q.pitchHz = hz;
        const auto a = silenceTest (q, 1.5);
        std::printf ("   pitch %5.0f Hz -> whistle %6.1f Hz  (level %.1f dBFS)\n", hz, a.f0, a.levelDb);
    }

    std::printf ("\n== 5. Gate / choke (gate on: threshold %.0f dB, attack %.1f ms)\n", Params().gateThresholdDb, Params().gateAttackMs);
    {
        Params q;   // defaults, gate on
        const auto a = silenceTest (q, 1.5);
        std::printf ("   silence with gate on: %.1f dBFS\n", a.levelDb);
        chokeTest (q, 6.0);
        q.gateFadeS = 5.0f;
        chokeTest (q, 6.0);
        q.gateOn = false;
        std::printf ("   gate off  :");
        chokeTest (q, 6.0);
        const auto ps = playStopTest (Params());
        std::printf ("   play/stop with gate on (0.4 s stops): playing %+.1f dB, stops %+.1f dB\n", ps.whistlePlayingDb, ps.whistleStopDb);
    }

    std::printf ("\n== 6. Gate threshold vs a strummed test guitar\n");
    {
        Guitar g; g.strum (0.25f);
        double peak = 0, sum = 0; const size_t n = (size_t) (1.2 * kFs);
        for (size_t i = 0; i < n; ++i) { const double v = g.tick(); peak = std::max (peak, std::abs (v)); sum += v * v; }
        std::printf ("   test guitar: peak %.1f dBFS, RMS %.1f dBFS over the strum\n", 20 * std::log10 (peak), 10 * std::log10 (sum / n));
        for (float thr : { -60.0f, -50.0f, -40.0f, -30.0f, -20.0f, -15.0f, -10.0f })
        {
            Params q; q.gateThresholdDb = thr;
            const auto r = playStopTest (q);
            Params qq = q; qq.gateOn = true;
            const auto sil = silenceTest (qq, 1.0);
            std::printf ("   threshold %5.0f dB: whistle in stops %+6.1f dB, while playing %+5.1f dB, silence %6.1f dBFS\n",
                         thr, r.whistleStopDb, r.whistlePlayingDb, sil.levelDb);
        }
    }

    // Demo: play/stop at 48 kHz (decimate the 4x signal with a simple averaging filter).
    auto y = runPlayStop (Params(), 4, 1.6, 0.6);   // shipped defaults, gate on
    std::vector<float> out;
    float z1 = 0, z2 = 0;
    for (size_t i = 0; i < y.size(); ++i)
    {
        z1 += 0.25f * (y[i] - z1); z2 += 0.25f * (z1 - z2);
        if (i % 4 == 3) out.push_back (z2 * 0.7f);
    }
    writeWav ("mayo_demo.wav", out, 48000);
    std::printf ("\nwrote mayo_demo.wav (%.1f s)\n", out.size() / 48000.0);
    return s.finite ? 0 : 1;
}
