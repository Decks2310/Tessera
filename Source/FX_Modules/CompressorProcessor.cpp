#include "CompressorProcessor.h"

CompressorProcessor::CompressorProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
{
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_";
    typeParamId = slotPrefix + "COMP_TYPE";
    thresholdParamId = slotPrefix + "COMP_THRESHOLD";
    ratioParamId = slotPrefix + "COMP_RATIO";
    attackParamId = slotPrefix + "COMP_ATTACK";
    releaseParamId = slotPrefix + "COMP_RELEASE";
    makeupParamId = slotPrefix + "COMP_MAKEUP";
}

void CompressorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumInputChannels() };
    compressor.prepare(spec);
    makeupGain.prepare(spec);
    reset();
}

void CompressorProcessor::releaseResources()
{
    reset();
}

void CompressorProcessor::reset()
{
    compressor.reset();
    makeupGain.reset();
}

void CompressorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // === FIX P1: Add standard boilerplate ===
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that didn't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    // ========================================

    auto type = static_cast<int>(mainApvts.getRawParameterValue(typeParamId)->load());

    compressor.setThreshold(mainApvts.getRawParameterValue(thresholdParamId)->load());
    compressor.setRatio(mainApvts.getRawParameterValue(ratioParamId)->load());
    compressor.setAttack(mainApvts.getRawParameterValue(attackParamId)->load());
    compressor.setRelease(mainApvts.getRawParameterValue(releaseParamId)->load());
    makeupGain.setGainDecibels(mainApvts.getRawParameterValue(makeupParamId)->load());

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    compressor.process(context);
    makeupGain.process(context);

    if (type == 1) // FET
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                channelData[i] = std::tanh(channelData[i] * 1.2f);
        }
    }
}