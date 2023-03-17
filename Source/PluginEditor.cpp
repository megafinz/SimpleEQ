/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleEQAudioProcessorEditor::SimpleEQAudioProcessorEditor (SimpleEQAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
peakFreqAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider),
peakGainAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
peakQualityAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
lowCutFreqAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
lowCutSlopeAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
highCutFreqAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
highCutSlopeAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    
    for (auto* comp : getComps())
    {
        addAndMakeVisible(comp);
    }
    
    setSize (600, 400);
}

SimpleEQAudioProcessorEditor::~SimpleEQAudioProcessorEditor()
{
}

//==============================================================================
void SimpleEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::black);
    
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    
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

void SimpleEQAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();
    /* auto responseArea = */ bounds.removeFromTop(bounds.getHeight() * 0.33);
    
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
        &highCutSlopeSlider
    };
}
