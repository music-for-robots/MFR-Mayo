// MFR-Mayo - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace colours
{
const juce::Colour bg      { 0xff1c1a17 };
const juce::Colour panel   { 0xff27241f };
const juce::Colour edge    { 0xff3d3830 };
const juce::Colour text    { 0xffe6dfcf };
const juce::Colour dimText { 0xff8f877a };
const juce::Colour mayo    { 0xfff2dd9a };   // accent
const juce::Colour squeal  { 0xffff8fb1 };   // feedback meter
const juce::Colour cellOff { 0xff332f29 };
} // namespace colours

class MayoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MayoLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
};

// Rotary slider with Ctrl/Shift fine-drag.
class FineSlider : public juce::Slider
{
public:
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    bool fineDrag = false;
    juce::Point<float> lastPos;
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title);
    void resized() override;

private:
    FineSlider slider;
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class MayoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MayoEditor (MayoProcessor&);
    ~MayoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void addKnob (std::vector<std::unique_ptr<Knob>>&, const char* id, const char* title);
    void resetToDefaults();

    MayoProcessor& proc;
    MayoLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> gateKnobs, fuzzKnobs, ampKnobs, squealKnobs, levelKnobs;
    juce::ToggleButton gateToggle { "" }, ampToggle { "AMP + CAB" };
    juce::AudioProcessorValueTreeState::ButtonAttachment gateAttachment, ampAttachment;
    juce::TextButton initButton { "INIT" };

    juce::Rectangle<int> gatePanel, fuzzPanel, ampPanel, squealPanel, levelPanel, meterArea, gateMeterArea;
    float meterLevel = 0.0f, gateLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MayoEditor)
};
