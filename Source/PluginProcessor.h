// MFR-Mayo - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "MayoDSP.h"

class MayoProcessor : public juce::AudioProcessor
{
public:
    MayoProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MFR-Mayo"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.5; }   // longest gate fade, plus the whistle's ring

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> squealMeter { 0.0f };
    std::atomic<float> gateMeter { 0.0f };

private:
    static constexpr int kOversampleStages = 2;   // 4x
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    mayo::Params readParams() const;

    juce::dsp::Oversampling<float> oversampling { 1, kOversampleStages,
                                                  juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    mayo::Chain chain;
    mayo::Params lastParams;
    juce::AudioBuffer<float> mono;

    std::atomic<float>* pGateOn; std::atomic<float>* pGateThresh; std::atomic<float>* pGateAttack; std::atomic<float>* pGateFade;
    std::atomic<float>* pInput; std::atomic<float>* pSustain; std::atomic<float>* pTone; std::atomic<float>* pFuzzVol;
    std::atomic<float>* pAmpOn; std::atomic<float>* pDrive; std::atomic<float>* pBass; std::atomic<float>* pMid;
    std::atomic<float>* pTreble; std::atomic<float>* pPresence; std::atomic<float>* pSqueal; std::atomic<float>* pPitch;
    std::atomic<float>* pDistance; std::atomic<float>* pDrift; std::atomic<float>* pBloom; std::atomic<float>* pOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MayoProcessor)
};
