#include "ModulationProcessor.h"

ModulationProcessor::ModulationProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
{
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_";
    modeParamId = slotPrefix + "MODULATION_MODE";
    rateParamId = slotPrefix + "MODULATION_RATE";
    depthParamId = slotPrefix + "MODULATION_DEPTH";
    feedbackParamId = slotPrefix + "MODULATION_FEEDBACK";
    mixParamId = slotPrefix + "MODULATION_MIX";
}

void ModulationProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumInputChannels() };
    chorus.prepare(spec);
    phaser.prepare(spec);
    reset();
}

void ModulationProcessor::releaseResources()
{
    reset();
}

void ModulationProcessor::reset()
{
    chorus.reset();
    phaser.reset();
}

void ModulationProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // === FIX P1: Add standard boilerplate ===
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that didn't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    // ========================================

    auto mode = static_cast<ModType>(static_cast<int>(mainApvts.getRawParameterValue(modeParamId)->load()));
    float rate = mainApvts.getRawParameterValue(rateParamId)->load();
    float depth = mainApvts.getRawParameterValue(depthParamId)->load();
    float feedback = mainApvts.getRawParameterValue(feedbackParamId)->load();
    float mix = mainApvts.getRawParameterValue(mixParamId)->load();

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    if (mode == Phaser)
    {
        phaser.setRate(rate);
        phaser.setDepth(depth);
        phaser.setFeedback(feedback);
        phaser.setMix(mix);
        phaser.process(context);
    }
    else
    {
        chorus.setRate(rate);
        chorus.setDepth(depth);
        chorus.setFeedback(feedback);
        chorus.setMix(mode == Vibrato ? 1.0f : mix);
        chorus.setCentreDelay(mode == Flanger ? 2.0f : 10.0f);
        chorus.process(context);
    }
}