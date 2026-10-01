// MFR-Mayo - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using FloatParam = juce::AudioParameterFloat;
using Attrs = juce::AudioParameterFloatAttributes;

std::unique_ptr<FloatParam> percent (const juce::String& id, const juce::String& name, float def)
{
    return std::make_unique<FloatParam> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 1.0f), def,
                                         Attrs().withStringFromValueFunction ([] (float v, int)
                                         { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }));
}

std::unique_ptr<FloatParam> decibels (const juce::String& id, const juce::String& name, float lo, float hi, float def)
{
    return std::make_unique<FloatParam> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi), def,
                                         Attrs().withStringFromValueFunction ([] (float v, int)
                                         { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout MayoProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Gate (first in the chain), GGate-style ranges.
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "gateon", 1 }, "Gate On", true));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "gatethresh", 1 }, "Gate Threshold",
                                              juce::NormalisableRange<float> (-100.0f, -1.0f), -40.0f,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return juce::String (v, 1) + " dB"; })));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "gateattack", 1 }, "Gate Attack",
                                              juce::NormalisableRange<float> (0.0f, 30.0f, 0.0f, 0.5f), 2.0f,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return juce::String (v, 1) + " ms"; })));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "gatefade", 1 }, "Gate Fade",
                                              juce::NormalisableRange<float> (0.01f, 5.0f, 0.0f, 0.5f), 0.4f,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return juce::String (v, 2) + " s"; })));

    layout.add (decibels ("input", "Input", -24.0f, 24.0f, 0.0f));

    layout.add (percent ("sustain", "Sustain", 0.7f));
    layout.add (percent ("tone", "Tone", 0.45f));
    layout.add (decibels ("fuzzvol", "Fuzz Volume", -24.0f, 12.0f, 0.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "ampon", 1 }, "Amp On", true));
    layout.add (percent ("drive", "Amp Drive", 0.5f));
    layout.add (decibels ("bass", "Bass", -12.0f, 12.0f, 0.0f));
    layout.add (decibels ("mid", "Middle", -12.0f, 12.0f, 0.0f));
    layout.add (decibels ("treble", "Treble", -12.0f, 12.0f, 0.0f));
    layout.add (decibels ("presence", "Presence", -12.0f, 12.0f, 0.0f));

    layout.add (percent ("squeal", "Squeal", 0.6f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "pitch", 1 }, "Squeal Pitch",
                                              juce::NormalisableRange<float> (200.0f, 1500.0f, 0.0f, 0.5f), 490.0f,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return juce::String (juce::roundToInt (v)) + " Hz"; })));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "distance", 1 }, "Distance",
                                              juce::NormalisableRange<float> (1.0f, 20.0f, 0.0f, 0.6f), 8.3f,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return juce::String (v * 0.343f, 1) + " m"; })));
    layout.add (percent ("drift", "Drift", 0.3f));
    layout.add (percent ("bloom", "Bloom", 0.35f));

    layout.add (decibels ("output", "Output", -36.0f, 12.0f, -6.0f));
    return layout;
}

MayoProcessor::MayoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pGateOn = get ("gateon"); pGateThresh = get ("gatethresh"); pGateAttack = get ("gateattack"); pGateFade = get ("gatefade");
    pInput = get ("input"); pSustain = get ("sustain"); pTone = get ("tone"); pFuzzVol = get ("fuzzvol");
    pAmpOn = get ("ampon"); pDrive = get ("drive"); pBass = get ("bass"); pMid = get ("mid");
    pTreble = get ("treble"); pPresence = get ("presence"); pSqueal = get ("squeal"); pPitch = get ("pitch");
    pDistance = get ("distance"); pDrift = get ("drift"); pBloom = get ("bloom"); pOutput = get ("output");
}

bool MayoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const auto ok = [] (const juce::AudioChannelSet& s)
    { return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo(); };
    return ok (in) && ok (out);
}

mayo::Params MayoProcessor::readParams() const
{
    mayo::Params p;
    p.gateOn = pGateOn->load() > 0.5f; p.gateThresholdDb = pGateThresh->load();
    p.gateAttackMs = pGateAttack->load(); p.gateFadeS = pGateFade->load();
    p.inputDb = pInput->load();
    p.sustain = pSustain->load(); p.tone = pTone->load(); p.fuzzVolumeDb = pFuzzVol->load();
    p.ampOn = pAmpOn->load() > 0.5f; p.drive = pDrive->load();
    p.bassDb = pBass->load(); p.midDb = pMid->load(); p.trebleDb = pTreble->load(); p.presenceDb = pPresence->load();
    p.squeal = pSqueal->load(); p.pitchHz = pPitch->load(); p.distanceMs = pDistance->load();
    p.drift = pDrift->load(); p.bloom = pBloom->load();
    p.outputDb = pOutput->load();
    return p;
}

void MayoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mono.setSize (1, samplesPerBlock);
    oversampling.reset();
    oversampling.initProcessing ((size_t) samplesPerBlock);
    chain.prepare (sampleRate * (1 << kOversampleStages));
    lastParams = readParams();
    chain.setParams (lastParams);
    setLatencySamples (juce::roundToInt (oversampling.getLatencyInSamples()));
}

void MayoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    if (mono.getNumSamples() < n)
        mono.setSize (1, n, false, false, true);

    // Guitar is mono: sum whatever comes in.
    mono.clear();
    for (int ch = 0; ch < numIn; ++ch)
        mono.addFrom (0, 0, buffer, ch, 0, n, 1.0f / (float) numIn);

    // Cheap enough to do every block (a few filter designs), and it keeps knob moves glitch-free.
    lastParams = readParams();
    chain.setParams (lastParams);

    juce::dsp::AudioBlock<float> block (mono.getArrayOfWritePointers(), 1, (size_t) n);
    auto up = oversampling.processSamplesUp (block);
    float* x = up.getChannelPointer (0);
    for (size_t i = 0; i < up.getNumSamples(); ++i)
        x[i] = chain.process (x[i]);
    oversampling.processSamplesDown (block);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, mono, 0, 0, n);

    squealMeter = chain.getSquealMeter();
    gateMeter = chain.getGateGain();
}

void MayoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void MayoProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MayoProcessor::createEditor() { return new MayoEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MayoProcessor(); }
