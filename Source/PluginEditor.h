/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

enum FFTOrder
{
    order2048 = 11,
    order4096 = 12,
    order8192 = 13
};

template<typename BlockType>
struct FFTDataGenerator
{
    void produceFFTDataForRendering(const juce::AudioBuffer<float>& audioData, const float negativeInfinity)
    {
        const auto fftSize = getFFTSize();
        
        fftData.assign(fftData.size(), 0);
        auto* readIndex = audioData.getReadPointer(0);
        std::copy(readIndex, readIndex + fftSize, fftData.begin());
        
        // Apply windowing function.
        window->multiplyWithWindowingTable(fftData.data(), fftSize);
        
        // Apply FFT transform.
        forwardFFT->performFrequencyOnlyForwardTransform(fftData.data());
        
        int numBins = (int) fftSize / 2;
        
        // Normalize.
        for (int i = 0; i < numBins; i++)
        {
            fftData[i] /= (float) numBins;
        }
        
        // Convert to dB.
        for (int i = 0; i < numBins; i++)
        {
            fftData[i] = juce::Decibels::gainToDecibels(fftData[i], negativeInfinity);
        }
        
        fftDataFifo.push(fftData);
    }
    
    void changeOrder(FFTOrder newOrder)
    {
        order = newOrder;
        
        auto fftSize = getFFTSize();
        
        forwardFFT = std::make_unique<juce::dsp::FFT>(order);
        window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris);
        
        fftData.clear();
        fftData.resize(fftSize * 2, 0);
        
        fftDataFifo.prepare(fftData.size());
    }
    
    int getFFTSize() const { return 1 << order; }
    int getNumAvailableFFTDataBlocks() const { return fftDataFifo.getNumAvailableForReading(); }
    
    bool getFFTData(BlockType& fftData) { return fftDataFifo.pull(fftData); }
    
private:
    FFTOrder order;
    BlockType fftData;
    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    
    Fifo<BlockType> fftDataFifo;
};

template<typename PathType>
struct AnalyzerPathGenerator
{
    void generatePath(const std::vector<float>& renderData,
                      juce::Rectangle<float> fftBounds,
                      int fftSize,
                      float binWidth,
                      float negativeInfinity)
    {
        auto top = fftBounds.getY();
        auto bottom = fftBounds.getHeight();
        auto width = fftBounds.getWidth();
        
        int numBins = (int) fftSize / 2;
        
        PathType p;
        p.preallocateSpace(3 * (int) width);
        
        auto map = [bottom, top, negativeInfinity](float v)
        {
            return juce::jmap(v, negativeInfinity, 0.0f, bottom, top);
        };
        
        auto y = map(renderData[0]);
        
        jassert(!std::isnan(y) && !std::isinf(y));
        
        p.startNewSubPath(0, y);
        
        const int pathResolution = 2;
        
        for (int binNum = 1; binNum < numBins; binNum += pathResolution)
        {
            y = map(renderData[binNum]);
            
            jassert(!std::isnan(y) && !std::isinf(y));
            
            if (!std::isnan(y) && !std::isinf(y))
            {
                auto binFreq = binNum * binWidth;
                auto normBinX = juce::mapFromLog10(binFreq, 20.0f, 20000.0f);
                int binX = std::floor(normBinX * width);
                p.lineTo(binX, y);
            }
        }
        
        pathFifo.push(p);
    }
    
    int getNumPathsAvailable() const
    {
        return pathFifo.getNumAvailableForReading();
    }
    
    bool getPath(PathType& path)
    {
        return pathFifo.pull(path);
    }
    
private:
    Fifo<PathType> pathFifo;
};

struct LookAndFeel : juce::LookAndFeel_V4
{
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
};

struct RotarySliderWithLabels : juce::Slider
{
    RotarySliderWithLabels(juce::RangedAudioParameter& p, juce::String s) : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
                                        juce::Slider::TextEntryBoxPosition::NoTextBox),
    param(&p),
    suffix(s)
    {
        setLookAndFeel(&lnf);
    }
    
    ~RotarySliderWithLabels()
    {
        setLookAndFeel(nullptr);
    }
    
    struct Label
    {
        float pos;
        juce::String label;
    };
    
    juce::Array<Label> labels;
    
    void paint(juce::Graphics&) override;
    juce::Rectangle<int> getSliderBounds() const;
    int getTextHeight() const { return 14; }
    juce::String getDisplayString() const;
    
private:
    LookAndFeel lnf;
    
    juce::RangedAudioParameter* param;
    juce::String suffix;
};

struct ResponseCurveComponent :
juce::Component,
juce::AudioProcessorParameter::Listener,
juce::Timer
{
public:
    ResponseCurveComponent(SimpleEQAudioProcessor&);
    ~ResponseCurveComponent() override;
    
    void paint (juce::Graphics&) override;
    
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override { }
    
    void timerCallback() override;
    
    void resized() override;
    
private:
    SimpleEQAudioProcessor& audioProcessor;
    
    juce::Atomic<bool> parametersChanged { false };
    
    MonoChain monoChain;
    
    void updateChain();
    
    juce::Image background;
    
    juce::Rectangle<int> getRenderArea();
    
    juce::Rectangle<int> getAnalysisArea();
    
    SingleChannelSampleFifo<SimpleEQAudioProcessor::BlockType>* leftChannelFifo;
    
    juce::AudioBuffer<float> monoBuffer;
    
    FFTDataGenerator<std::vector<float>> leftChannelFFTDataGenerator;
    
    AnalyzerPathGenerator<juce::Path> pathProducer;
    
    juce::Path leftChannelFFTPath;
};

//==============================================================================
/**
*/
class SimpleEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    SimpleEQAudioProcessorEditor (SimpleEQAudioProcessor&);
    ~SimpleEQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SimpleEQAudioProcessor& audioProcessor;
    
    RotarySliderWithLabels peakFreqSlider, peakGainSlider, peakQualitySlider;
    RotarySliderWithLabels lowCutFreqSlider, lowCutSlopeSlider;
    RotarySliderWithLabels highCutFreqSlider, highCutSlopeSlider;
    
    ResponseCurveComponent responseCurveComponent;
    
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;
    
    Attachment peakFreqAttachment, peakGainAttachment, peakQualityAttachment;
    Attachment lowCutFreqAttachment, lowCutSlopeAttachment;
    Attachment highCutFreqAttachment, highCutSlopeAttachment;
    
    std::vector<juce::Component*> getComps();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQAudioProcessorEditor)
};
