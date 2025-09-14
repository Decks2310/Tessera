//================================================================================
// File: FX_Modules/DistortionProcessor.cpp (NO INTERNAL OS)
//================================================================================

#include "DistortionProcessor.h"

DistortionProcessor::DistortionProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
{
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_";
    driveParamId = slotPrefix + "DISTORTION_DRIVE";
    levelParamId = slotPrefix + "DISTORTION_LEVEL";
    typeParamId = slotPrefix + "DISTORTION_TYPE";
    biasParamId = slotPrefix + "DISTORTION_BIAS";
    characterParamId = slotPrefix + "DISTORTION_CHARACTER";
}

void DistortionProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    auto numChannels = (juce::uint32)getTotalNumInputChannels();
    if (numChannels == 0) numChannels = 2;

    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, numChannels };

    preGain.prepare(spec);
    postGain.prepare(spec);
    preGain.setRampDurationSeconds(0.01);
    postGain.setRampDurationSeconds(0.01);

    inputDCBlocker.prepare(spec);
    outputDCBlocker.prepare(spec);
    if (sampleRate > 0)
    {
        auto dcBlockerCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f);
        *inputDCBlocker.state = *dcBlockerCoeffs;
        *outputDCBlocker.state = *dcBlockerCoeffs;
    }

    double smoothingTime = 0.05;
    smoothedBias.reset(sampleRate, smoothingTime);
    smoothedCharacter.reset(sampleRate, smoothingTime);

    inputFollower.prepare(spec);
    inputFollower.setAttackTime(5.0f);
    inputFollower.setReleaseTime(50.0f);

    reset();
}

void DistortionProcessor::releaseResources() { reset(); }

void DistortionProcessor::reset() {
    preGain.reset();
    postGain.reset();
    inputDCBlocker.reset();
    outputDCBlocker.reset();
    inputFollower.reset();
    smoothedBias.setCurrentAndTargetValue(0.0f);
    smoothedCharacter.setCurrentAndTargetValue(0.5f);
}

float DistortionProcessor::processTube(float x, float bias, float dynamicBias) {
    float effectiveBias = bias * 0.5f + (dynamicBias * 0.3f);
    float y = x + effectiveBias;
    if (y > 0)
        return std::tanh(y * 0.9f);
    else
        return std::tanh(y * 1.4f);
}

float DistortionProcessor::processOpAmp(float x, float character) {
    float soft = std::tanh(x * 1.5f);
    float hard = x / (std::abs(x) + 0.6f) * 0.8f;
    return juce::jmap(character, hard, soft);
}

float DistortionProcessor::processGermanium(float x, float stability) {
    float gateThreshold = juce::jmap(stability, 0.08f, 0.001f);
    if (std::abs(x) < gateThreshold) return x * 0.1f;
    float positiveDrive = 1.8f;
    float negativeDrive = juce::jmap(stability, 0.7f, 1.3f);
    if (x > 0)
        return (1.0f - std::exp(-x * positiveDrive)) * 0.85f;
    else
        return (-1.0f + std::exp(std::abs(x) * negativeDrive)) * 0.85f;
}

void DistortionProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    auto type = static_cast<Algo>(static_cast<int>(mainApvts.getRawParameterValue(typeParamId)->load()));
    preGain.setGainDecibels(mainApvts.getRawParameterValue(driveParamId)->load());
    postGain.setGainDecibels(mainApvts.getRawParameterValue(levelParamId)->load());
    smoothedBias.setTargetValue(mainApvts.getRawParameterValue(biasParamId)->load());
    smoothedCharacter.setTargetValue(mainApvts.getRawParameterValue(characterParamId)->load());

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    inputDCBlocker.process(context);
    preGain.process(context);

    int numSamples = (int)block.getNumSamples();
    int numChannels = (int)block.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        float currentBias = smoothedBias.getNextValue();
        float currentCharacter = smoothedCharacter.getNextValue();
        float currentDynamicBias = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float sample = block.getSample(ch, i);
            if (ch == 0)
                currentDynamicBias = inputFollower.process(sample);

            float processed = sample;
            switch (type)
            {
            case Algo::VintageTube: processed = processTube(sample, currentBias, currentDynamicBias); break;
            case Algo::OpAmp: processed = processOpAmp(sample, currentCharacter); break;
            case Algo::GermaniumFuzz: processed = processGermanium(sample, currentCharacter); break;
            }
            block.setSample(ch, i, processed);
        }
    }

    outputDCBlocker.process(context);
    postGain.process(context);
}
