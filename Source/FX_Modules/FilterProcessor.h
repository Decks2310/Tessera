#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class FilterProcessor : public juce::AudioProcessor
{
public:
    FilterProcessor(juce::AudioProcessorValueTreeState& mainApvts, int slotIndex);
    ~FilterProcessor() override = default;

    const juce::String getName() const override { return "Filter"; }
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

    enum Profile { svfProfile, transistorLadder, diodeLadder, ota };

private:
    juce::dsp::StateVariableTPTFilter<float> svfFilter;
    juce::dsp::LadderFilter<float> ladderFilter;

    juce::AudioProcessorValueTreeState& mainApvts;
    juce::String cutoffParamId, resonanceParamId, driveParamId, typeParamId, profileParamId;
};