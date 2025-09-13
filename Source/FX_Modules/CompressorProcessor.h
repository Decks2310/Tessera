#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class CompressorProcessor : public juce::AudioProcessor
{
public:
    CompressorProcessor(juce::AudioProcessorValueTreeState& mainApvts, int slotIndex);
    ~CompressorProcessor() override = default;

    const juce::String getName() const override { return "Compressor"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Gain<float> makeupGain;

    juce::AudioProcessorValueTreeState& mainApvts;
    juce::String typeParamId, thresholdParamId, ratioParamId, attackParamId, releaseParamId, makeupParamId;
};