#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class ReverbProcessor : public juce::AudioProcessor
{
public:
    ReverbProcessor(juce::AudioProcessorValueTreeState& mainApvts, int slotIndex);
    ~ReverbProcessor() override = default;

    const juce::String getName() const override { return "Reverb"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

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
    juce::dsp::Reverb reverb;

    juce::AudioProcessorValueTreeState& mainApvts;
    juce::String roomSizeParamId, dampingParamId, mixParamId, widthParamId;
};