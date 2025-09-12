//================================================================================
// File: FX_Modules/DistortionProcessor.cpp (CORRECTED)
//================================================================================

#include "DistortionProcessor.h"

// FIX: Removed incorrect initialization of ProcessorDuplicator from the initializer list.
// Initialization must rely on prepareToPlay where the actual sample rate is known.
DistortionProcessor::DistortionProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
    // REMOVED:
    // inputDCBlocker(juce::dsp::IIR::Coefficients<float>::makeHighPass(44100.0, 20.0f)),
    // outputDCBlocker(juce::dsp::IIR::Coefficients<float>::makeHighPass(44100.0, 20.0f))
{
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_";
    driveParamId = slotPrefix + "DISTORTION_DRIVE";
    levelParamId = slotPrefix + "DISTORTION_LEVEL";
    typeParamId = slotPrefix + "DISTORTION_TYPE";
    biasParamId = slotPrefix + "DISTORTION_BIAS";
    characterParamId = slotPrefix + "DISTORTION_CHARACTER";
}

// The rest of the DistortionProcessor.cpp file remains the same as your provided version,
// as the prepareToPlay implementation was already handling initialization correctly.

void DistortionProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    auto numChannels = (juce::uint32)getTotalNumInputChannels();
    if (numChannels == 0) numChannels = 2;

    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, numChannels };

    preGain.prepare(spec);
    postGain.prepare(spec);
    preGain.setRampDurationSeconds(0.01);
    postGain.setRampDurationSeconds(0.01);

    // === FIX: Initialize the ProcessorDuplicator for DC Blocking (High-pass at 20Hz) ===
    inputDCBlocker.prepare(spec);
    outputDCBlocker.prepare(spec);

    if (sampleRate > 0)
    {
        // Calculate coefficients (returns a ReferenceCountedPointer)
        auto dcBlockerCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f);
        // Assign to the shared state of the ProcessorDuplicator
        *inputDCBlocker.state = *dcBlockerCoeffs;
        *outputDCBlocker.state = *dcBlockerCoeffs;
    }
    // =====================================================================================

    double smoothingTime = 0.05;
    smoothedBias.reset(sampleRate, smoothingTime);
    smoothedCharacter.reset(sampleRate, smoothingTime);

    inputFollower.prepare(spec);
    inputFollower.setAttackTime(5.0f);
    inputFollower.setReleaseTime(50.0f);

    localOversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(numChannels), 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
    localOversampler->initProcessing(static_cast<size_t>(samplesPerBlock));

    reset();
}

void DistortionProcessor::releaseResources() {
    reset();
}

void DistortionProcessor::reset() {
    preGain.reset();
    postGain.reset();
    // === FIX: Reset the ProcessorDuplicators ===
    inputDCBlocker.reset();
    outputDCBlocker.reset();
    // ===========================================
    inputFollower.reset();
    if (localOversampler)
        localOversampler->reset();
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

    // === FIX P1: Add standard boilerplate ===
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that didn't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    // ========================================

    auto type = static_cast<Algo>(static_cast<int>(mainApvts.getRawParameterValue(typeParamId)->load()));
    preGain.setGainDecibels(mainApvts.getRawParameterValue(driveParamId)->load());
    postGain.setGainDecibels(mainApvts.getRawParameterValue(levelParamId)->load());
    smoothedBias.setTargetValue(mainApvts.getRawParameterValue(biasParamId)->load());
    smoothedCharacter.setTargetValue(mainApvts.getRawParameterValue(characterParamId)->load());

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    inputDCBlocker.process(context);
    preGain.process(context);

    auto oversampledBlock = localOversampler->processSamplesUp(block);

    float currentBias = 0.0f;
    float currentCharacter = 0.0f;

    // === FIX (C4267): Explicitly cast dimensions to int and use int for loop indices ===
    int numOversampledSamples = (int)oversampledBlock.getNumSamples();
    int numChannels = (int)oversampledBlock.getNumChannels();

    for (int i = 0; i < numOversampledSamples; ++i)
    {
        currentBias = smoothedBias.getNextValue();
        currentCharacter = smoothedCharacter.getNextValue();
        float currentDynamicBias = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            // Arguments are now int, resolving warnings
            float inputSample = oversampledBlock.getSample(channel, i);

            if (channel == 0)
                currentDynamicBias = inputFollower.process(inputSample);

            float outputSample = inputSample;
            switch (type)
            {
            case Algo::VintageTube:
                outputSample = processTube(inputSample, currentBias, currentDynamicBias);
                break;
            case Algo::OpAmp:
                outputSample = processOpAmp(inputSample, currentCharacter);
                break;
            case Algo::GermaniumFuzz:
                outputSample = processGermanium(inputSample, currentCharacter);
                break;
            }
            // Arguments are now int, resolving warnings
            oversampledBlock.setSample(channel, i, outputSample);
        }
    }
    // =====================================================================================

    localOversampler->processSamplesDown(block);
    outputDCBlocker.process(context);
    postGain.process(context);
}