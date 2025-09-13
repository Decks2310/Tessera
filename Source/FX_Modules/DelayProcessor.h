#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class DelayProcessor : public juce::AudioProcessor
{
public:
    DelayProcessor(juce::AudioProcessorValueTreeState& mainApvts, int slotIndex);
    ~DelayProcessor() override = default;

    const juce::String getName() const override { return "Delay"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;
    juce::dsp::StateVariableTPTFilter<float> feedbackFilter;
    // float lastFeedbackOutput[2] = { 0.0f, 0.0f }; // REMOVE THIS LINE

    juce::AudioProcessorValueTreeState& mainApvts;
    juce::String typeParamId, timeParamId, feedbackParamId, mixParamId, dampingParamId;
};