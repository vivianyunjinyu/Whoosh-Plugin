#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//== pastel-blue look, swap these hex values for your exact Vocoduh palette ====
class BlueLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BlueLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId,            juce::Colour (0xffEAF3FB));
        setColour (juce::ComboBox::textColourId,                  juce::Colour (0xff4A6D8C));
        setColour (juce::ComboBox::outlineColourId,               juce::Colour (0xffBAD8EE));
        setColour (juce::ComboBox::arrowColourId,                 juce::Colour (0xff7FA8C9));
        setColour (juce::PopupMenu::backgroundColourId,           juce::Colour (0xffF5FAFF));
        setColour (juce::PopupMenu::highlightedBackgroundColourId,juce::Colour (0xffBAD8EE));
        setColour (juce::PopupMenu::textColourId,                 juce::Colour (0xff4A6D8C));
        setColour (juce::Label::textColourId,                     juce::Colour (0xff4A6D8C));
    }
};

//== the draggable two-breakpoint curve editor + live scope ===================
class CurveDisplay : public juce::Component,
                     private juce::Timer
{
public:
    CurveDisplay (WhooshAudioProcessor& p) : proc (p), apvts (p.parameters)
    {
        floorVal    = apvts.getRawParameterValue ("floor");
        recoveryVal = apvts.getRawParameterValue ("recovery");
        curveVal    = apvts.getRawParameterValue ("curve");
        startTimerHz (30);
    }

    ~CurveDisplay() override { stopTimer(); }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent&)   override { dragging = Drag::none; }

private:
    void timerCallback() override { repaint(); }

    float phaseToX (float ph) const { return pad + ph * ((float) getWidth()  - 2.0f * pad); }
    float ampToY   (float a)  const { return pad + (1.0f - a) * ((float) getHeight() - 2.0f * pad); }
    float xToPhase (float x)  const { return juce::jlimit (0.0f, 1.0f, (x - pad) / ((float) getWidth()  - 2.0f * pad)); }
    float yToAmp   (float y)  const { return juce::jlimit (0.0f, 1.0f, 1.0f - (y - pad) / ((float) getHeight() - 2.0f * pad)); }

    float curveGain (float phase, float floorLevel, float recovery, int type) const
    {
        const float p = (type == 0 ? 3.0f : (type == 1 ? 0.33f : 1.0f)); // exp / log / linear
        if (phase < recovery)
        {
            const float t = phase / recovery;
            return floorLevel + (1.0f - floorLevel) * std::pow (t, p);
        }
        return 1.0f;
    }

    void drawHandle (juce::Graphics& g, float x, float y, juce::Colour fill, juce::Colour edge)
    {
        g.setColour (edge); g.fillEllipse (x - handleR,        y - handleR,        handleR * 2.0f,        handleR * 2.0f);
        g.setColour (fill); g.fillEllipse (x - handleR + 2.0f, y - handleR + 2.0f, (handleR - 2.0f) * 2.0f, (handleR - 2.0f) * 2.0f);
    }

    WhooshAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float>* floorVal    = nullptr;
    std::atomic<float>* recoveryVal = nullptr;
    std::atomic<float>* curveVal    = nullptr;

    enum class Drag { none, pointA, pointB };
    Drag dragging = Drag::none;

    static constexpr float pad     = 20.0f;
    static constexpr float handleR = 8.0f;
    static constexpr float grabR   = 16.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveDisplay)
};

//== the main editor ==========================================================
class WhooshAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit WhooshAudioProcessorEditor (WhooshAudioProcessor&);
    ~WhooshAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WhooshAudioProcessor& processorRef;
    BlueLookAndFeel lnf;

    CurveDisplay curveDisplay;
    juce::ComboBox rateBox, curveBox;
    juce::Label rateLabel, curveLabel;
    juce::Label titleLabel, subtitleLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateAttach, curveAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WhooshAudioProcessorEditor)
};
