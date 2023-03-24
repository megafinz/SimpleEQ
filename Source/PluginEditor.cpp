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

void LookAndFeel::drawToggleButton(juce::Graphics& g,
                                   juce::ToggleButton& toggleButton,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown)
{
    juce::Path powerButton;
    
    auto bounds = toggleButton.getLocalBounds();
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 6;
    auto r = bounds.withSizeKeepingCentre(size, size);
    
    float angle = 30.0f;
    
    size -= 6;
    
    powerButton.addCentredArc(r.toFloat().getCentreX(), r.toFloat().getCentreY(),
                              size * 0.5f, size * 0.5f,
                              0.0f,
                              juce::degreesToRadians(angle), juce::degreesToRadians(360 - angle),
                              true);
    
    powerButton.startNewSubPath(r.getCentreX(), r.getY());
    powerButton.lineTo(r.toFloat().getCentre());
    
    juce::PathStrokeType pst(2.0f, juce::PathStrokeType::JointStyle::curved);
    
    auto colour = toggleButton.getToggleState() ? juce::Colours::dimgrey : juce::Colours::greenyellow;
    
    g.setColour(colour);
    g.strokePath(powerButton, pst);
    
    g.drawEllipse(r.toFloat(), 2.0f);
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

void PathProducer::process(juce::Rectangle<float> fftBounds, double sampleRate)
{
    juce::AudioBuffer<float> tempIncomingBuffer;
    
    // Produce FFT data.
    
    while (sampleFifo->getNumCompleteBuffersAvailable() > 0)
    {
        if (sampleFifo->getAudioBuffer(tempIncomingBuffer))
        {
            auto size = tempIncomingBuffer.getNumSamples();
            
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, 0),
                                              monoBuffer.getReadPointer(0, size),
                                              monoBuffer.getNumSamples() - size);
            
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, monoBuffer.getNumSamples() - size),
                                              tempIncomingBuffer.getReadPointer(0, 0),
                                              size);
            
            fftDataGenerator.produceFFTDataForRendering(monoBuffer, -48.0f);
        }
    }
    
    // Produce paths to render from FFT data.
    
    const auto fftSize = fftDataGenerator.getFFTSize();
    const auto binWidth = sampleRate / (double) fftSize;
    
    while (fftDataGenerator.getNumAvailableFFTDataBlocks() > 0)
    {
        std::vector<float> fftData;
        
        if (fftDataGenerator.getFFTData(fftData))
        {
            pathProducer.generatePath(fftData, fftBounds.toFloat(), fftSize, binWidth, -48.0f);
        }
    }
    
    // Store most recent path.
    
    while (pathProducer.getNumPathsAvailable() > 0)
    {
        pathProducer.getPath(fftPath);
    }
}

//==============================================================================

ResponseCurveComponent::ResponseCurveComponent(SimpleEQAudioProcessor& p) :
audioProcessor(p),
leftChannelPathProducer(audioProcessor.leftChannelFifo),
rightChannelPathProducer(audioProcessor.rightChannelFifo)
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
    
    g.drawImage(background, getLocalBounds().toFloat());
    
    auto renderArea = getAnalysisArea();
    
    auto w = renderArea.getWidth();
    
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
             
        if (!monoChain.isBypassed<ChainPositions::LowCut>())
        {
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
        }
        
        if (!monoChain.isBypassed<ChainPositions::HighCut>())
        {
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
        }
        
        magnitudes[i] = juce::Decibels::gainToDecibels(magnitude);
    }
    
    juce::Path responseCurve;
    
    const double outputMin = renderArea.getBottom();
    const double outputMax = renderArea.getY();
    
    auto map = [outputMin, outputMax](double input)
    {
        return juce::jmap(input, -24.0, 24.0, outputMin, outputMax);
    };
    
    responseCurve.startNewSubPath(renderArea.getX(), map(magnitudes.front()));
    
    for (size_t i = 0; i < magnitudes.size(); i++)
    {
        responseCurve.lineTo(renderArea.getX() + i, map(magnitudes[i]));
    }
    
    auto leftChannelPath = leftChannelPathProducer.getPath();
    auto rightChannelPath = rightChannelPathProducer.getPath();
    
    leftChannelPath.applyTransform(juce::AffineTransform().translation(renderArea.getX(), renderArea.getY()));
    rightChannelPath.applyTransform(juce::AffineTransform().translation(renderArea.getX(), renderArea.getY()));
    
    g.setColour(juce::Colours::skyblue);
    g.strokePath(leftChannelPath, juce::PathStrokeType(1.0f));
    
    g.setColour(juce::Colours::lightyellow);
    g.strokePath(rightChannelPath, juce::PathStrokeType(1.0f));
    
    g.setColour(juce::Colours::orange);
    g.drawRoundedRectangle(getRenderArea().toFloat(), 4.0f, 1.0f);
    
    g.setColour(juce::Colours::white);
    g.strokePath(responseCurve, juce::PathStrokeType(2.0f));
}

void ResponseCurveComponent::parameterValueChanged(int parameterIndex, float newValue)
{
    parametersChanged.set(true);
}

void ResponseCurveComponent::timerCallback()
{
    auto fftBounds = getAnalysisArea().toFloat();
    auto sampleRate = audioProcessor.getSampleRate();
    
    leftChannelPathProducer.process(fftBounds, sampleRate);
    rightChannelPathProducer.process(fftBounds, sampleRate);
    
    if (parametersChanged.compareAndSetBool(false, true))
    {
        updateChain();
    }
    
    repaint();
}

void ResponseCurveComponent::updateChain()
{
    auto chainSettings = getChainSettings(audioProcessor.apvts);
    
    monoChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    monoChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    monoChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    
    auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
    auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
    auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());
    
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCutCoefficients(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutCoefficients(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);
}

void ResponseCurveComponent::resized()
{
    background = juce::Image(juce::Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    
    juce::Graphics g(background);
    
    g.setColour(juce::Colours::dimgrey);
    
    auto renderArea = getAnalysisArea();
    auto t = renderArea.getY();
    auto b = renderArea.getBottom();
    auto l = renderArea.getX();
    auto r = renderArea.getRight();
    auto w = renderArea.getWidth();
    
    // Draw frequency bands.
    
    juce::Array<float> freqs
    {
        20, 50, 100,
        200, 500, 1000,
        2000, 5000, 10000,
        20000
    };
    
    juce::Array<float> freqPositions;
    
    for (auto freq : freqs)
    {
        auto pos = l + juce::mapFromLog10(freq, 20.0f, 20000.0f) * w;
        
        freqPositions.add(pos);
        
        g.drawVerticalLine(pos, t, b);
    }
        
    // Draw gain bands.
    
    juce::Array<float> gains
    {
        -24, -12, 0, 12, 24
    };
    
    juce::Array<float> gainPositions;
    
    for (auto gain : gains)
    {
        auto pos = juce::jmap(gain, -24.0f, 24.0f, float(b), float(t));
        
        gainPositions.add(pos);
        
        g.setColour(gain == 0 ? juce::Colours::green : juce::Colours::dimgrey);
        g.drawHorizontalLine(pos, l, r);
    }
    
    // Draw frequency labels.
    
    const int fontHeight = 10;
    
    g.setFont(fontHeight);
    g.setColour(juce::Colours::white);
    
    for (int i = 0; i < freqs.size(); i++)
    {
        auto freq = freqs[i];
        auto pos = freqPositions[i];
        
        bool addK = false;
        juce::String label;
        
        if (freq >= 1000.f)
        {
            addK = true;
            freq /= 1000.0f;
        }
        
        label << freq << " ";
        
        if (addK)
        {
            label << "k";
        }
        
        label << "Hz";
        
        auto textWidth = g.getCurrentFont().getStringWidth(label);
        
        juce::Rectangle<int> textBounds;
        textBounds.setSize(textWidth, fontHeight);
        textBounds.setCentre(pos, 0);
        textBounds.setY(1);
        
        g.drawFittedText(label, textBounds, juce::Justification::centred, 1);
    }
    
    // Draw gain labels.
    
    for (int i = 0; i < gains.size(); i++)
    {
        auto gain = gains[i];
        auto pos = gainPositions[i];
        
        // Right.
        
        juce::String label;
        
        if (gain > 0)
        {
            label << "+";
        }
        
        label << gain << " dB";
        
        auto textWidth = g.getCurrentFont().getStringWidth(label);
        
        juce::Rectangle<int> textBounds;
        textBounds.setSize(textWidth, fontHeight);
        textBounds.setX(getWidth() - textWidth);
        textBounds.setCentre(textBounds.getCentreX(), pos);
        
        g.setColour(gain == 0 ? juce::Colours::green : juce::Colours::white);
        
        g.drawFittedText(label, textBounds, juce::Justification::centred, 1);
        
        // Left.
        
        label.clear();
        
        label << gain - 24.0f;
        
        textBounds.setX(1);
        textWidth = g.getCurrentFont().getStringWidth(label);
        textBounds.setSize(textWidth, fontHeight);
        
        g.setColour(juce::Colours::white);
        g.drawFittedText(label, textBounds, juce::Justification::centred, 1);
    }
}

juce::Rectangle<int> ResponseCurveComponent::getRenderArea()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop(16);
    bounds.removeFromBottom(2);
    bounds.removeFromLeft(35);
    bounds.removeFromRight(35);
    
    return bounds;
}

juce::Rectangle<int> ResponseCurveComponent::getAnalysisArea()
{
    auto bounds = getRenderArea();
    
    bounds.removeFromTop(4);
    bounds.removeFromBottom(4);
    
    return bounds;
}

//==============================================================================

SimpleEQAudioProcessorEditor::SimpleEQAudioProcessorEditor (SimpleEQAudioProcessor& p)
: AudioProcessorEditor (&p),
audioProcessor (p),
responseCurveComponent(p),
peakFreqSlider(*audioProcessor.apvts.getParameter("Peak Freq"), "Hz"),
peakGainSlider(*audioProcessor.apvts.getParameter("Peak Gain"), "dB"),
peakQualitySlider(*audioProcessor.apvts.getParameter("Peak Quality"), ""),
lowCutFreqSlider(*audioProcessor.apvts.getParameter("LowCut Freq"), "Hz"),
lowCutSlopeSlider(*audioProcessor.apvts.getParameter("LowCut Slope"), "dB/Oct"),
highCutFreqSlider(*audioProcessor.apvts.getParameter("HighCut Freq"), "Hz"),
highCutSlopeSlider(*audioProcessor.apvts.getParameter("HighCut Slope"), "dB/Oct"),
peakFreqAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider),
peakGainAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
peakQualityAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
lowCutFreqAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
lowCutSlopeAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
highCutFreqAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
highCutSlopeAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider),
peakBypassAttachment(audioProcessor.apvts, "Peak Bypassed", peakBypassButton),
lowCutBypassAttachment(audioProcessor.apvts, "LowCut Bypassed", lowCutBypassButton),
highCutBypassAttachment(audioProcessor.apvts, "HighCut Bypassed", highCutBypassButton),
analyzerEnabledAttachment(audioProcessor.apvts, "Analyzer Enabled", analyzerEnabledButton)
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
    
    peakBypassButton.setLookAndFeel(&lnf);
    lowCutBypassButton.setLookAndFeel(&lnf);
    highCutBypassButton.setLookAndFeel(&lnf);
    
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
        
    setSize(640, 480);
}

SimpleEQAudioProcessorEditor::~SimpleEQAudioProcessorEditor()
{
    peakBypassButton.setLookAndFeel(nullptr);
    lowCutBypassButton.setLookAndFeel(nullptr);
    highCutBypassButton.setLookAndFeel(nullptr);
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
    auto lowCutBypassButtonArea = lowCutArea.removeFromTop(25);
    auto lowCutFreqArea = lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5);
    auto lowCutSlopeArea = lowCutArea;
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);
    auto highCutBypassButtonArea = highCutArea.removeFromTop(25);
    auto highCutFreqArea = highCutArea.removeFromTop(highCutArea.getHeight() * 0.5);
    auto highCutSlopeArea = highCutArea;
    auto peakBypassedButtonArea = bounds.removeFromTop(25);
    auto peakFreqArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    auto peakGainArea = bounds.removeFromTop(bounds.getHeight() * 0.5);
    auto peakQualityArea = bounds;
    
    lowCutBypassButton.setBounds(lowCutBypassButtonArea);
    lowCutFreqSlider.setBounds(lowCutFreqArea);
    lowCutSlopeSlider.setBounds(lowCutSlopeArea);
    
    highCutBypassButton.setBounds(highCutBypassButtonArea);
    highCutFreqSlider.setBounds(highCutFreqArea);
    highCutSlopeSlider.setBounds(highCutSlopeArea);
    
    peakBypassButton.setBounds(peakBypassedButtonArea);
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
        &responseCurveComponent,
        &lowCutBypassButton,
        &highCutBypassButton,
        &peakBypassButton,
        &analyzerEnabledButton
    };
}
