//================================================================================
// File: FX_Modules/HelicalDelayProcessor.cpp
//================================================================================
#include "HelicalDelayProcessor.h"

HelicalDelayProcessor::HelicalDelayProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
{
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_";
    timeParamId = slotPrefix + "HELICAL_TIME";
    pitchParamId = slotPrefix + "HELICAL_PITCH";
    feedbackParamId = slotPrefix + "HELICAL_FEEDBACK";
    degradeParamId = slotPrefix + "HELICAL_DEGRADE";
    textureParamId = slotPrefix + "HELICAL_TEXTURE";
    mixParamId = slotPrefix + "HELICAL_MIX";
}

void HelicalDelayProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentSampleRate = sampleRate;
    auto numChannels = (juce::uint32)std::max(getTotalNumInputChannels(), getTotalNumOutputChannels());
    if (numChannels == 0) numChannels = 2;
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, numChannels };

    const int maxDelayInSamples = (int)(sampleRate * 2.0);
    delayBuffer.prepare(spec, maxDelayInSamples);

    degradeFilter.prepare(spec);
    degradeFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    textureLFO.prepare(spec);
    textureLFO.setFrequency(0.3f);
    textureLFO.setWaveform(DSPUtils::LFO::Waveform::Sine);

    readPositions.resize(numChannels, 0.0);

    double rampTimeSeconds = 0.03;
    smoothedTimeMs.reset(sampleRate, rampTimeSeconds);
    smoothedPitch.reset(sampleRate, rampTimeSeconds);
    smoothedFeedback.reset(sampleRate, rampTimeSeconds);
    smoothedDegrade.reset(sampleRate, rampTimeSeconds);
    smoothedTexture.reset(sampleRate, rampTimeSeconds);
    smoothedMix.reset(sampleRate, rampTimeSeconds);

    reset();
}

void HelicalDelayProcessor::releaseResources() {}

void HelicalDelayProcessor::reset() {
    delayBuffer.reset();
    degradeFilter.reset();
    textureLFO.reset();

    if (auto* p = mainApvts.getRawParameterValue(timeParamId))    smoothedTimeMs.setCurrentAndTargetValue(p->load());
    if (auto* p = mainApvts.getRawParameterValue(pitchParamId))   smoothedPitch.setCurrentAndTargetValue(p->load());
    if (auto* p = mainApvts.getRawParameterValue(feedbackParamId))smoothedFeedback.setCurrentAndTargetValue(p->load());
    if (auto* p = mainApvts.getRawParameterValue(degradeParamId)) smoothedDegrade.setCurrentAndTargetValue(p->load());
    if (auto* p = mainApvts.getRawParameterValue(textureParamId)) smoothedTexture.setCurrentAndTargetValue(p->load());
    if (auto* p = mainApvts.getRawParameterValue(mixParamId))     smoothedMix.setCurrentAndTargetValue(p->load());

    std::fill(readPositions.begin(), readPositions.end(), 0.0);
}

void HelicalDelayProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    smoothedTimeMs.setTargetValue(mainApvts.getRawParameterValue(timeParamId)->load());
    smoothedPitch.setTargetValue(mainApvts.getRawParameterValue(pitchParamId)->load());
    smoothedFeedback.setTargetValue(mainApvts.getRawParameterValue(feedbackParamId)->load());
    smoothedDegrade.setTargetValue(mainApvts.getRawParameterValue(degradeParamId)->load());
    smoothedTexture.setTargetValue(mainApvts.getRawParameterValue(textureParamId)->load());
    smoothedMix.setTargetValue(mainApvts.getRawParameterValue(mixParamId)->load());

    int writePosition = delayBuffer.getWritePosition();
    const int bufferSize = delayBuffer.getSize();

    auto textureMod = textureLFO.getNextStereoSample();

    for (int i = 0; i < numSamples; ++i)
    {
        float timeMs        = smoothedTimeMs.getNextValue();
        float pitchSemis    = smoothedPitch.getNextValue();
        float feedback      = smoothedFeedback.getNextValue();
        float degrade       = smoothedDegrade.getNextValue();
        float texture       = smoothedTexture.getNextValue();
        float mix           = smoothedMix.getNextValue();

        float rate = std::pow(2.0f, pitchSemis / 12.0f);
        float delaySamples = timeMs * (float)currentSampleRate / 1000.0f;

        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            float mod = (ch == 0) ? textureMod.first : textureMod.second;
            float modDelay = delaySamples * (1.0f + mod * texture * 0.05f);
            modDelay = juce::jlimit(1.0f, (float)bufferSize - 1.0f, modDelay);

            readPositions[ch] += (double)rate;
            if (readPositions[ch] >= bufferSize)
                readPositions[ch] -= bufferSize;

            double readHead = (double)writePosition - modDelay + readPositions[ch];
            while (readHead < 0) readHead += bufferSize;
            readHead = std::fmod(readHead, (double)bufferSize);

            float delayedSample = delayBuffer.read(ch, (float)readHead);

            float cutoff = juce::jmap(degrade, 0.0f, 1.0f, 18000.0f, 1000.0f);
            degradeFilter.setCutoffFrequency(cutoff);
            float filteredSample = degradeFilter.processSample(ch, delayedSample);

            float saturated = DSPUtils::fastTanh(filteredSample * 1.2f);
            float feedbackSample = saturated * feedback;

            float inputSample = buffer.getSample(ch, i);
            float toDelay = inputSample + feedbackSample;
            delayBuffer.writeSample(ch, toDelay);

            float out = (inputSample * (1.0f - mix)) + (saturated * mix);
            buffer.setSample(ch, i, out);
        }
        delayBuffer.advanceWritePosition();
    }
}
