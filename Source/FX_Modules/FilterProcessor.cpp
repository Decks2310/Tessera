#include "FilterProcessor.h"

FilterProcessor::FilterProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
{
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_";
    cutoffParamId = slotPrefix + "FILTER_CUTOFF";
    resonanceParamId = slotPrefix + "FILTER_RESONANCE";
    driveParamId = slotPrefix + "FILTER_DRIVE";
    typeParamId = slotPrefix + "FILTER_TYPE";
    profileParamId = slotPrefix + "FILTER_PROFILE";
}

void FilterProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumInputChannels() };
    svfFilter.prepare(spec);
    ladderFilter.prepare(spec);
    reset();
}

void FilterProcessor::releaseResources() {
    reset();
}

void FilterProcessor::reset() {
    svfFilter.reset();
    ladderFilter.reset();
}

void FilterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    // === FIX P1: Add standard boilerplate ===
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that didn't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    // ========================================

    auto profile = static_cast<Profile>(static_cast<int>(mainApvts.getRawParameterValue(profileParamId)->load()));
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    float rawResonance = mainApvts.getRawParameterValue(resonanceParamId)->load();

    switch (profile)
    {
    case svfProfile:
        svfFilter.setCutoffFrequency(mainApvts.getRawParameterValue(cutoffParamId)->load());
        svfFilter.setResonance(rawResonance);
        svfFilter.setType(static_cast<juce::dsp::StateVariableTPTFilterType>(static_cast<int>(mainApvts.getRawParameterValue(typeParamId)->load())));
        svfFilter.process(context);
        break;

    case transistorLadder:
    {
        ladderFilter.setMode(juce::dsp::LadderFilterMode::LPF24);
        ladderFilter.setCutoffFrequencyHz(mainApvts.getRawParameterValue(cutoffParamId)->load());
        float ladderResonance = juce::jlimit(0.0f, 1.0f, rawResonance / 10.0f);
        ladderFilter.setResonance(ladderResonance);
        ladderFilter.setDrive(mainApvts.getRawParameterValue(driveParamId)->load());
        ladderFilter.process(context);
        break;
    }
    case diodeLadder:
    {
        ladderFilter.setMode(juce::dsp::LadderFilterMode::LPF12);
        ladderFilter.setCutoffFrequencyHz(mainApvts.getRawParameterValue(cutoffParamId)->load());
        float ladderResonanceDiode = juce::jlimit(0.0f, 1.0f, rawResonance / 10.0f);
        ladderFilter.setResonance(ladderResonanceDiode);
        ladderFilter.setDrive(mainApvts.getRawParameterValue(driveParamId)->load());
        ladderFilter.process(context);
        break;
    }
    case ota:
        svfFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        svfFilter.setCutoffFrequency(mainApvts.getRawParameterValue(cutoffParamId)->load());
        svfFilter.setResonance(rawResonance);
        svfFilter.process(context);
        break;
    }
}