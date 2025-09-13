#include "ReverbProcessor.h"

ReverbProcessor::ReverbProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
{
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_";
    roomSizeParamId = slotPrefix + "REVERB_ROOM_SIZE";
    dampingParamId = slotPrefix + "REVERB_DAMPING";
    mixParamId = slotPrefix + "REVERB_MIX";
    widthParamId = slotPrefix + "REVERB_WIDTH";
}

void ReverbProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumInputChannels() };
    reverb.prepare(spec);
    reset();
}

void ReverbProcessor::releaseResources()
{
    reset();
}

void ReverbProcessor::reset()
{
    reverb.reset();
}

void ReverbProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // === FIX P1: Add standard boilerplate ===
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that didn't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    // ========================================

    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = mainApvts.getRawParameterValue(roomSizeParamId)->load();
    reverbParams.damping = mainApvts.getRawParameterValue(dampingParamId)->load();
    reverbParams.wetLevel = mainApvts.getRawParameterValue(mixParamId)->load();
    reverbParams.dryLevel = 1.0f - reverbParams.wetLevel;
    reverbParams.width = mainApvts.getRawParameterValue(widthParamId)->load();
    reverb.setParameters(reverbParams);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);
}