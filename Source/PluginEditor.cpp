// MFR-Mayo - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"

namespace
{
juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace

//==============================================================================
MayoLookAndFeel::MayoLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextButton::textColourOffId, colours::text);
}

void MayoLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? colours::cellOff.brighter (0.3f) : highlighted ? colours::cellOff.brighter (0.12f) : colours::cellOff);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void MayoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, startAngle, endAngle, true);
    g.setColour (colours::edge);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, startAngle, angle, true);
    g.setColour (colours::mayo);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = radius - 8.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff57514a), centre.x, centre.y - knobR,
                                             juce::Colour (0xff1a1815), centre.x, centre.y + knobR, false));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2, knobR * 2).withCentre (centre));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (juce::Rectangle<float> (knobR * 2, knobR * 2).withCentre (centre), 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -knobR + 3.0f, 3.0f, knobR * 0.5f, 1.5f);
    g.setColour (colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void MayoLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? colours::mayo : (highlighted ? colours::cellOff.brighter (0.12f) : colours::cellOff));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? colours::bg : colours::dimText);
    g.setFont (font (12.0f, true));
    const auto state = on ? juce::String ("ON") : b.getProperties().getWithDefault ("offText", "OFF").toString();
    g.drawText (b.getButtonText().isEmpty() ? state : b.getButtonText() + "  " + state, r, juce::Justification::centred);
}

//==============================================================================
void FineSlider::mouseDown (const juce::MouseEvent& e)
{
    fineDrag = e.mods.isCtrlDown() || e.mods.isShiftDown();
    lastPos = e.position;
    Slider::mouseDown (e);
}

void FineSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (! fineDrag || ! isEnabled())
    {
        Slider::mouseDrag (e);
        return;
    }
    // Linear fine mode: 2000 px of drag covers the whole range (8x finer than normal).
    const float delta = (e.position.x - lastPos.x) + (lastPos.y - e.position.y);
    lastPos = e.position;
    const double pos = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + delta / 2000.0);
    setValue (proportionOfLengthToValue (pos), juce::sendNotificationSync);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& title)
    : attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    if (auto* p = state.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));   // also Alt-click
    addAndMakeVisible (slider);

    label.setText (title.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (12.0f, true));
    label.setColour (juce::Label::textColourId, colours::dimText);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
MayoEditor::MayoEditor (MayoProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      gateAttachment (p.apvts, "gateon", gateToggle), ampAttachment (p.apvts, "ampon", ampToggle)
{
    setLookAndFeel (&lnf);
    ampToggle.getProperties().set ("offText", "BYPASSED");

    addKnob (gateKnobs, "gatethresh", "Threshold");
    addKnob (gateKnobs, "gateattack", "Attack");
    addKnob (gateKnobs, "gatefade", "Fade");

    addKnob (fuzzKnobs, "sustain", "Sustain");
    addKnob (fuzzKnobs, "tone", "Tone");
    addKnob (fuzzKnobs, "fuzzvol", "Volume");

    addKnob (ampKnobs, "drive", "Drive");
    addKnob (ampKnobs, "bass", "Bass");
    addKnob (ampKnobs, "mid", "Middle");
    addKnob (ampKnobs, "treble", "Treble");
    addKnob (ampKnobs, "presence", "Presence");

    addKnob (squealKnobs, "squeal", "Squeal");
    addKnob (squealKnobs, "pitch", "Pitch");
    addKnob (squealKnobs, "distance", "Distance");
    addKnob (squealKnobs, "drift", "Drift");
    addKnob (squealKnobs, "bloom", "Bloom");

    addKnob (levelKnobs, "input", "Input");
    addKnob (levelKnobs, "output", "Output");

    addAndMakeVisible (gateToggle);
    addAndMakeVisible (ampToggle);

    initButton.setTooltip ("Reset every control to its default");
    initButton.onClick = [this] { resetToDefaults(); };
    addAndMakeVisible (initButton);

    setSize (1100, 410);
    startTimerHz (30);
}

MayoEditor::~MayoEditor() { setLookAndFeel (nullptr); }

void MayoEditor::addKnob (std::vector<std::unique_ptr<Knob>>& group, const char* id, const char* title)
{
    addAndMakeVisible (*group.emplace_back (std::make_unique<Knob> (proc.apvts, id, title)));
}

void MayoEditor::resetToDefaults()
{
    // As host gestures, so the DAW records the change like any other knob move.
    for (auto* param : proc.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
}

void MayoEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colours::bg.brighter (0.06f), 0, 0, colours::bg, 0, (float) getHeight(), false));
    g.fillAll();

    g.setColour (colours::mayo);
    g.setFont (font (30.0f, true));
    g.drawText ("MFR-MAYO", 24, 12, 300, 36, juce::Justification::centredLeft);
    g.setColour (colours::dimText);
    g.setFont (font (12.0f));
    g.drawText ("FUZZ  |  AMP  |  MICROPHONIC SQUEAL", 200, 14, 400, 18, juce::Justification::centredLeft);
    g.setFont (font (10.0f));
    g.drawText ("v" JucePlugin_VersionString "  |  musicforrobots.com", 200, 30, 300, 14, juce::Justification::centredLeft);

    const std::pair<juce::Rectangle<int>, const char*> panels[] = {
        { gatePanel, "GATE" }, { fuzzPanel, "FUZZ" }, { ampPanel, "AMP" }, { squealPanel, "SQUEAL" }, { levelPanel, "LEVELS" }
    };
    for (auto& [r, name] : panels)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (colours::edge);
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.0f);
        g.setColour (colours::mayo.withAlpha (0.8f));
        g.setFont (font (12.0f, true));
        g.drawText (name, r.getX() + 12, r.getY() + 6, 120, 16, juce::Justification::centredLeft);
    }

    // Squeal meter: how hard the pickup is ringing.
    g.setColour (colours::cellOff);
    g.fillRoundedRectangle (meterArea.toFloat(), 3.0f);
    const float level = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (meterLevel, -80.0f) + 80.0f) / 70.0f);
    g.setColour (colours::squeal.withAlpha (0.4f + 0.6f * level));
    g.fillRoundedRectangle (meterArea.toFloat().withWidth (meterArea.getWidth() * level), 3.0f);
    g.setColour (colours::dimText);
    g.setFont (font (10.0f, true));
    g.drawText ("PICKUP RING", meterArea.translated (0, -14).withHeight (12), juce::Justification::centredLeft);

    // Gate meter: how far open the gate is (0-100%), like GGate's.
    g.setColour (colours::cellOff);
    g.fillRoundedRectangle (gateMeterArea.toFloat(), 3.0f);
    g.setColour (colours::mayo.withAlpha (0.85f));
    g.fillRoundedRectangle (gateMeterArea.toFloat().withWidth (gateMeterArea.getWidth() * juce::jlimit (0.0f, 1.0f, gateLevel)), 3.0f);
    g.setColour (colours::dimText);
    g.setFont (font (9.0f, true));
    g.drawText ("0%", gateMeterArea.translated (0, 12).withHeight (11), juce::Justification::centredLeft);
    g.drawText ("100%", gateMeterArea.translated (0, 12).withHeight (11), juce::Justification::centredRight);

    g.setColour (colours::dimText);
    g.setFont (font (10.0f));
    g.drawText ("Double-click or Alt-click resets a knob. Ctrl/Shift-drag for fine control. "
                "The squeal blooms when you stop playing: mute the strings and let it ring.",
                24, getHeight() - 26, getWidth() - 48, 14, juce::Justification::centredLeft);
}

void MayoEditor::resized()
{
    const int kw = 86, kh = 108;
    auto layoutRow = [&] (std::vector<std::unique_ptr<Knob>>& knobs, juce::Rectangle<int> area)
    {
        for (auto& k : knobs)
            k->setBounds (area.removeFromLeft (kw).withHeight (kh));
    };

    initButton.setBounds (getWidth() - 96, 18, 80, 26);

    gatePanel   = { 16, 58, 3 * kw + 24, 150 };
    fuzzPanel   = { gatePanel.getRight() + 10, 58, 3 * kw + 24, 150 };
    ampPanel    = { fuzzPanel.getRight() + 10, 58, getWidth() - fuzzPanel.getRight() - 26, 150 };
    squealPanel = { 16, 218, 5 * kw + 24 + 150, 150 };
    levelPanel  = { squealPanel.getRight() + 10, 218, getWidth() - squealPanel.getRight() - 26, 150 };

    layoutRow (gateKnobs, gatePanel.reduced (12, 0).withTrimmedTop (32));
    gateToggle.setBounds (gatePanel.getRight() - 100, gatePanel.getY() + 6, 88, 22);
    gateMeterArea = { gatePanel.getX() + 62, gatePanel.getY() + 9, gatePanel.getWidth() - 62 - 112, 8 };
    layoutRow (fuzzKnobs, fuzzPanel.reduced (12, 0).withTrimmedTop (32));
    layoutRow (ampKnobs, ampPanel.reduced (12, 0).withTrimmedTop (32).withTrimmedLeft ((ampPanel.getWidth() - 24 - 5 * kw) / 2));
    ampToggle.setBounds (ampPanel.getRight() - 172, ampPanel.getY() + 6, 160, 22);

    layoutRow (squealKnobs, squealPanel.reduced (12, 0).withTrimmedTop (32));
    meterArea = { squealPanel.getX() + 12 + 5 * kw + 10, squealPanel.getY() + 72, 124, 14 };

    layoutRow (levelKnobs, levelPanel.reduced (12, 0).withTrimmedTop (32).withTrimmedLeft ((levelPanel.getWidth() - 24 - 2 * kw) / 2));
}

void MayoEditor::timerCallback()
{
    const float target = proc.squealMeter.load();
    meterLevel = target > meterLevel ? target : meterLevel * 0.85f + target * 0.15f;
    gateLevel = proc.gateMeter.load();
    repaint (meterArea.expanded (2));
    repaint (gateMeterArea.expanded (2, 14));
}
