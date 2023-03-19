/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================

void LookAndFeel::drawRotarySlider(juce::Graphics &g,
                                   int x,
                                   int y,
                                   int width,
                                   int height,
                                   float sliderPosProportional,
                                   float rotaryStartAngle,
                                   float rotaryEndAngle,
                                   juce::Slider &slider)
{
    auto bounds = juce::Rectangle<float>(x, y, width, height);
    
    // Draw slider body.
    
    g.setColour(juce::Colours::green);
    g.fillEllipse(bounds);
    
    g.setColour(juce::Colours::greenyellow);
    g.drawEllipse(bounds, 1.0f);
    
    auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider);
    
    if (rswl == nullptr)
    {
        return;
    }
    
    auto centre = bounds.getCentre();
    
    juce::Path p;
    
    // Draw slider needle.
    
    juce::Rectangle<float> r;
    
    r.setLeft(centre.getX() - 2);
    r.setRight(centre.getX() + 2);
    r.setTop(bounds.getY());
    r.setBottom(centre.getY() - rswl->getTextHeight() * 1.5);
    
    p.addRoundedRectangle(r, 2.0f);
    
    jassert(rotaryStartAngle < rotaryEndAngle);
    
    auto sliderAngleRad = juce::jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);
    
    p.applyTransform(juce::AffineTransform().rotated(sliderAngleRad, centre.getX(), centre.getY()));
    
    g.fillPath(p);
    
    // Draw slider value.
    
    g.setFont(rswl->getTextHeight());
    
    auto text = rswl->getDisplayString();
    auto textWidth = g.getCurrentFont().getStringWidth(text);
    
    r.setSize(textWidth + 4, rswl->getTextHeight() + 2);
    r.setCentre(centre);
    
    g.setColour(juce::Colours::black);
    g.fillRect(r);
    
    g.setColour(juce::Colours::white);
    g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
}

//==============================================================================

void RotarySliderWithLabels::paint(juce::Graphics &g)
{
    auto startAngle = juce::degreesToRadians(225.0f);
    auto endAngle = juce::degreesToRadians(135.0f) + juce::MathConstants<float>::twoPi;
    
    auto range = getRange();
    auto sliderBounds = getSliderBounds();
    
    // Draw slider.
    
    getLookAndFeel().drawRotarySlider(g,
                                      sliderBounds.getX(),
                                      sliderBounds.getY(),
                                      sliderBounds.getWidth(),
                                      sliderBounds.getHeight(),
                                      juce::jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
                                      startAngle,
                                      endAngle,
                                      *this);
    
    // Draw outer slider labels.
    
    auto centre = sliderBounds.getCentre();
    auto radius = sliderBounds.getWidth() / 2.0f;
    
    g.setColour(juce::Colours::greenyellow);
    g.setFont(getTextHeight());
    
    for (auto label : labels)
    {
        jassert(label.pos >= 0.0f);
        jassert(label.pos <= 1.0f);
        
        auto angleRad = juce::jmap(label.pos, 0.0f, 1.0f, startAngle, endAngle);
        auto labelCentre = centre.getPointOnCircumference(radius + getTextHeight() / 2.0f + 1.0f, angleRad);
        
        juce::Rectangle<float> r;
        
        r.setSize(g.getCurrentFont().getStringWidth(label.label), getTextHeight());
        r.setCentre(labelCentre);
        r.setY(r.getY() + getTextHeight());
        
        g.drawFittedText(label.label, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    auto bounds = getLocalBounds();
    
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    
    size -= getTextHeight() * 2;
    
    juce::Rectangle<int> r;
    r.setSize(size, size);
    r.setCentre(bounds.getCentreX(), bounds.getCentreY());
    r.setY(2);
    
    return r;
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
    {
        return choiceParam->getCurrentChoiceName();
    }
    else if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        bool addK = false;
        
        float val = getValue();
        
        if (val >= 1000.0f)
        {
            val /= 1000.0f;
            addK = true;
        }
        
        auto result = juce::String(val, addK ? 2 : 0);
        
        if (addK)
        {
            result << "k";
        }
        
        if (suffix.isNotEmpty())
        {
            result << " ";
            result << suffix;
        }
        
        return result;
    }
    else
    {
        jassertfalse;
    }
    
    return juce::String(getValue());
}

//==============================================================================

ResponseCurveComponent::ResponseCurveComponent(SimpleEQAudioProcessor& p) : audioProcessor(p)
{
    const auto& params = audioProcessor.getParameters();
    
    for (auto param : params)
    {
        param->addListener(this);
    }
    
    updateChain();
    
    startTimerHz(60);
}

ResponseCurveComponent::~ResponseCurveComponent()
{
    const auto& params = audioProcessor.getParameters();
    
    for (auto param : params)
    {
        param->removeListener(this);
    }
}

void ResponseCurveComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::black);
    
    auto responseArea = getLocalBounds();
    
    auto w = responseArea.getWidth();
    
    auto& lowCut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highCut = monoChain.get<ChainPositions::HighCut>();
    
    auto sampleRate = audioProcessor.getSampleRate();
    
    std::vector<double> magnitudes;
    magnitudes.resize(w);
    
    for (int i = 0; i < w; i++)
    {
        auto magnitude = 1.0;
        auto freq = juce::mapToLog10(double(i) / double(w), 20.0, 20000.0);
        
        if (!monoChain.isBypassed<ChainPositions::Peak>())
        {
            magnitude *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
                
        if (!lowCut.isBypassed<0>())
        {
            magnitude *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        
        if (!lowCut.isBypassed<1>())
        {
            magnitude *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        
        if (!lowCut.isBypassed<2>())
        {
            magnitude *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        
        if (!lowCut.isBypassed<3>())
        {
            magnitude *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        
        if (!highCut.isBypassed<0>())
        {
            magnitude *= highCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        
        if (!highCut.isBypassed<1>())
        {
            magnitude *= highCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        
        if (!highCut.isBypassed<2>())
        {
            magnitude *= highCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        
        if (!highCut.isBypassed<3>())
        {
            magnitude *= highCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        
        magnitudes[i] = juce::Decibels::gainToDecibels(magnitude);
    }
    
    juce::Path responseCurve;
    
    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    
    auto map = [outputMin, outputMax](double input)
    {
        return juce::jmap(input, -24.0, 24.0, outputMin, outputMax);
    };
    
    responseCurve.startNewSubPath(responseArea.getX(), map(magnitudes.front()));
    
    for (size_t i = 0; i < magnitudes.size(); i++)
    {
        responseCurve.lineTo(responseArea.getX() + i, map(magnitudes[i]));
    }
    
    g.setColour(juce::Colours::orange);
    g.drawRoundedRectangle(responseArea.toFloat(), 4.0f, 1.0f);
    
    g.setColour(juce::Colours::white);
    g.strokePath(responseCurve, juce::PathStrokeType(2.0f));
}

void ResponseCurveComponent::parameterValueChanged(int parameterIndex, float newValue)
{
    parametersChanged.set(true);
}

void ResponseCurveComponent::timerCallback()
{
    if (parametersChanged.compareAndSetBool(false, true))
    {
        updateChain();
        repaint();
    }
}

void ResponseCurveComponent::updateChain()
{
    auto chainSettings = getChainSettings(audioProcessor.apvts);
    auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
    auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
    auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());
    
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCutCoefficients(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutCoefficients(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);
}

//==============================================================================

SimpleEQAudioProcessorEditor::SimpleEQAudioProcessorEditor (SimpleEQAudioProcessor& p)
: AudioProcessorEditor (&p), audioProcessor (p), peakFreqSlider(*audioProcessor.apvts.getParameter("Peak Freq"), "Hz"),
peakGainSlider(*audioProcessor.apvts.getParameter("Peak Gain"), "dB"),
peakQualitySlider(*audioProcessor.apvts.getParameter("Peak Quality"), ""),
lowCutFreqSlider(*audioProcessor.apvts.getParameter("LowCut Freq"), "Hz"),
lowCutSlopeSlider(*audioProcessor.apvts.getParameter("LowCut Slope"), "dB/Oct"),
highCutFreqSlider(*audioProcessor.apvts.getParameter("HighCut Freq"), "Hz"),
highCutSlopeSlider(*audioProcessor.apvts.getParameter("HighCut Slope"), "dB/Oct"),
responseCurveComponent(p),
peakFreqAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider),
peakGainAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
peakQualityAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
lowCutFreqAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
lowCutSlopeAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
highCutFreqAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
highCutSlopeAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider)
{
    peakFreqSlider.labels.add({ 0.0f, "20 Hz" });
    peakFreqSlider.labels.add({ 1.0f, "20 kHz" });
    
    peakGainSlider.labels.add({ 0.0f, "-24 dB" });
    peakGainSlider.labels.add({ 1.0f, "24 dB" });
    
    peakQualitySlider.labels.add({ 0.0f, "0.1" });
    peakQualitySlider.labels.add({ 1.0f, "10" });
    
    lowCutFreqSlider.labels.add({ 0.0f, "20 Hz" });
    lowCutFreqSlider.labels.add({ 1.0f, "20 kHz" });
    
    lowCutSlopeSlider.labels.add({ 0.0f, "12 dB/Oct" });
    lowCutSlopeSlider.labels.add({ 1.0f, "48 dB/Oct" });
    
    highCutFreqSlider.labels.add({ 0.0f, "20 Hz" });
    highCutFreqSlider.labels.add({ 1.0f, "20 kHz" });
    
    highCutSlopeSlider.labels.add({ 0.0f, "12 dB/Oct" });
    highCutSlopeSlider.labels.add({ 1.0f, "48 dB/Oct" });
    
    for (auto* comp : getComps())
    {
        addAndMakeVisible(comp);
    }
    
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
        
    setSize(640, 480);
}

SimpleEQAudioProcessorEditor::~SimpleEQAudioProcessorEditor()
{
}

//==============================================================================
void SimpleEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::black);
}

void SimpleEQAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();
    auto hRatio = 25.0f / 100.0f;
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * hRatio);
    
    bounds.removeFromTop(5);
    responseArea.reduce(5, 5);
    
    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);
    auto peakFreqArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    auto peakGainArea = bounds.removeFromTop(bounds.getHeight() * 0.5);
    auto peakQualityArea = bounds;
    
    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
    lowCutSlopeSlider.setBounds(lowCutArea);
    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    highCutSlopeSlider.setBounds(highCutArea);
    peakFreqSlider.setBounds(peakFreqArea);
    peakGainSlider.setBounds(peakGainArea);
    peakQualitySlider.setBounds(peakQualityArea);
    responseCurveComponent.setBounds(responseArea);
}

std::vector<juce::Component*> SimpleEQAudioProcessorEditor::getComps()
{
    return
    {
        &peakFreqSlider,
        &peakGainSlider,
        &peakQualitySlider,
        &lowCutFreqSlider,
        &lowCutSlopeSlider,
        &highCutFreqSlider,
        &highCutSlopeSlider,
        &responseCurveComponent
    };
}
