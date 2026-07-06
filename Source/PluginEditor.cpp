#include "PluginProcessor.h"
#include "PluginEditor.h"

//== CurveDisplay =============================================================
void CurveDisplay::paint (juce::Graphics& g)
{
    const float floorLevel = floorVal->load();
    const float recovery   = juce::jmax (0.02f, recoveryVal->load());
    const int   type       = (int) curveVal->load();

    auto b = getLocalBounds().toFloat().reduced (8.0f);

    // soft dark shadow so the panel still lifts off the body
    juce::Path panelPath;
    panelPath.addRoundedRectangle (b, 14.0f);
    juce::DropShadow (juce::Colour (0xaa05070f), 16, { 0, 5 }).drawForPath (g, panelPath);

    // inset "void" panel: darker than the body so glowing elements read as light
    g.setColour (juce::Colour (0xff121525));
    g.fillRoundedRectangle (b, 14.0f);

    // grid (dim slate lines)
    g.setColour (juce::Colour (0xff2C3254));
    for (int i = 1; i < 4; ++i)
    {
        const float y = ampToY (i / 4.0f);
        g.drawLine (phaseToX (0.0f), y, phaseToX (1.0f), y, 1.0f);
        const float x = phaseToX (i / 4.0f);
        g.drawLine (x, ampToY (0.0f), x, ampToY (1.0f), 1.0f);
    }

    // tiny pastel star specks in the corners
    {
        auto star = [&] (float sx, float sy, float r, juce::Colour c)
        {
            g.setColour (c);
            g.fillEllipse (sx - r, sy - r, r * 2.0f, r * 2.0f);
        };
        star (b.getRight() - 40.0f, b.getY() + 18.0f, 1.5f, juce::Colour (0xff8FD8EA));
        star (b.getRight() - 64.0f, b.getY() + 36.0f, 1.0f, juce::Colour (0xffF48FB8));
        star (b.getRight() - 24.0f, b.getY() + 50.0f, 1.2f, juce::Colour (0xffC9B7F5));
        star (b.getX() + 28.0f,     b.getY() + 30.0f, 1.2f, juce::Colour (0xffC9B7F5));
        star (b.getX() + 52.0f,     b.getY() + 16.0f, 1.0f, juce::Colour (0xffF48FB8));
    }

    // --- live output waveform (one cycle, phase-aligned), clipped to panel ---
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (panelPath);

        const int   n     = WhooshAudioProcessor::scopeSize;
        const float left  = phaseToX (0.0f), right = phaseToX (1.0f);
        const float midY  = (ampToY (1.0f) + ampToY (0.0f)) * 0.5f;
        const float halfH = (ampToY (0.0f) - ampToY (1.0f)) * 0.5f * 0.9f;

        juce::Path wave;
        for (int i = 0; i < n; ++i)
        {
            const float v = proc.scopeBuffer[(size_t) i];
            const float x = left + (right - left) * (float) i / (float) (n - 1);
            if (i == 0) wave.startNewSubPath (x, midY - v * halfH);
            else        wave.lineTo          (x, midY - v * halfH);
        }
        for (int i = n - 1; i >= 0; --i)
        {
            const float v = proc.scopeBuffer[(size_t) i];
            const float x = left + (right - left) * (float) i / (float) (n - 1);
            wave.lineTo (x, midY + v * halfH);
        }
        wave.closeSubPath();
        g.setColour (juce::Colour (0x446FD8E8));   // faint holographic cyan
        g.fillPath (wave);
    }

    // build the curve
    juce::Path curve;
    const int steps = 160;
    curve.startNewSubPath (phaseToX (0.0f), ampToY (curveGain (0.0f, floorLevel, recovery, type)));
    for (int i = 1; i <= steps; ++i)
    {
        const float ph = (float) i / (float) steps;
        curve.lineTo (phaseToX (ph), ampToY (curveGain (ph, floorLevel, recovery, type)));
    }

    // fill under the curve: translucent pink up top fading to faint purple below
    juce::Path fill = curve;
    fill.lineTo (phaseToX (1.0f), ampToY (0.0f));
    fill.lineTo (phaseToX (0.0f), ampToY (0.0f));
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x48F48FB8), b.getX(), ampToY (1.0f),
                                             juce::Colour (0x1A7C5CBF), b.getX(), ampToY (0.0f), false));
    g.fillPath (fill);

    // neon glow: two wide low-alpha strokes underneath, then the crisp line
    g.setColour (juce::Colour (0x30F48FB8));
    g.strokePath (curve, juce::PathStrokeType (9.0f));
    g.setColour (juce::Colour (0x55F48FB8));
    g.strokePath (curve, juce::PathStrokeType (5.5f));
    g.setColour (juce::Colour (0xffF48FB8));
    g.strokePath (curve, juce::PathStrokeType (2.5f));

    // handles: dark bead + glowing pastel ring (pink = point A, aqua = point B)
    drawHandle (g, phaseToX (0.0f),    ampToY (floorLevel), juce::Colour (0xffF48FB8));
    drawHandle (g, phaseToX (recovery), ampToY (1.0f),      juce::Colour (0xff8FD8EA));

    // border
    g.setColour (juce::Colour (0xff454C7A));
    g.drawRoundedRectangle (b, 14.0f, 1.0f);
}

void CurveDisplay::mouseDown (const juce::MouseEvent& e)
{
    const juce::Point<float> a { phaseToX (0.0f),                ampToY (floorVal->load()) };
    const juce::Point<float> b { phaseToX (recoveryVal->load()), ampToY (1.0f) };

    if (e.position.getDistanceFrom (a) < grabR)      dragging = Drag::pointA;
    else if (e.position.getDistanceFrom (b) < grabR) dragging = Drag::pointB;
    else                                             dragging = Drag::none;
}

void CurveDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == Drag::pointA)
    {
        const float newFloor = yToAmp (e.position.y);
        apvts.getParameter ("floor")->setValueNotifyingHost (newFloor);
    }
    else if (dragging == Drag::pointB)
    {
        const float newRec = juce::jlimit (0.02f, 1.0f, xToPhase (e.position.x));
        apvts.getParameter ("recovery")->setValueNotifyingHost (newRec);
    }
    repaint();
}

//== Editor ===================================================================
WhooshAudioProcessorEditor::WhooshAudioProcessorEditor (WhooshAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), curveDisplay (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (curveDisplay);

    rateBox.addItemList ({ "1/1", "1/2", "1/4", "1/8", "1/16" }, 1);
    curveBox.addItemList ({ "Exponential", "Logarithmic", "Linear" }, 1);
    addAndMakeVisible (rateBox);
    addAndMakeVisible (curveBox);

    rateAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.parameters, "rate",  rateBox);
    curveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.parameters, "curve", curveBox);

    rateLabel.setText ("Rate", juce::dontSendNotification);
    curveLabel.setText ("Curve", juce::dontSendNotification);
    rateLabel.setJustificationType (juce::Justification::centredLeft);
    curveLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (rateLabel);
    addAndMakeVisible (curveLabel);

    titleLabel.setText ("Whoosh", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (32.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffF1F4FF)); // near-white on navy
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("vivian creations, made with a 10-bit lookup table and love :)",
                           juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (15.0f, juce::Font::italic));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffF48FB8)); // pastel pink
    subtitleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (subtitleLabel);

    setSize (720, 400);   // horizontal rectangle
}

WhooshAudioProcessorEditor::~WhooshAudioProcessorEditor()
{
    setLookAndFeel (nullptr);   // always detach before the LookAndFeel dies
}

void WhooshAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff232842), 0.0f, 0.0f,
                                             juce::Colour (0xff181C30), 0.0f, (float) getHeight(), false));
    g.fillAll();
}

void WhooshAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (16);

    titleLabel   .setBounds (r.removeFromTop (40));
    subtitleLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (10);                              // gap under header

    auto bottom = r.removeFromBottom (56);
    curveDisplay.setBounds (r.withTrimmedBottom (12));

    bottom.removeFromTop (8);
    auto left  = bottom.removeFromLeft (bottom.getWidth() / 2).reduced (4, 0);
    auto right = bottom.reduced (4, 0);

    rateLabel .setBounds (left .removeFromTop (20));
    rateBox   .setBounds (left);
    curveLabel.setBounds (right.removeFromTop (20));
    curveBox  .setBounds (right);
}
