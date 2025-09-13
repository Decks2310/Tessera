// File: FX_Modules/AdvancedCompressorProcessor.cpp
#include "AdvancedCompressorProcessor.h"

AdvancedCompressorProcessor::AdvancedCompressorProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
{
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_ADVCOMP_";
    topologyParamId = slotPrefix + "TOPOLOGY";
    detectorParamId = slotPrefix + "DETECTOR";
    thresholdParamId = slotPrefix + "THRESHOLD";
    ratioParamId = slotPrefix + "RATIO";
    attackParamId = slotPrefix + "ATTACK";
    releaseParamId = slotPrefix + "RELEASE";
    makeupParamId = slotPrefix + "MAKEUP";
}

void AdvancedCompressorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumInputChannels() };

    peakDetector.prepare(spec);
    peakDetector.setAttackTime(0.1f); // Fast ballistics for the detector itself
    peakDetector.setReleaseTime(5.0f);

    envelopeSmoother.prepare(spec);
    makeupGain.prepare(spec);
    colorationStage.prepare(spec);

    // Initialize RMS averages
    rmsAverages.resize(spec.numChannels, 0.0f);
    // Calculate alpha for RMS moving average (Blueprint 3.2.1)
    if (sampleRate > 0)
        rmsAlpha = std::exp(-1.0f / (float)(sampleRate * rmsWindowTimeMs / 1000.0f));

    reset();
}

void AdvancedCompressorProcessor::reset()
{
    peakDetector.reset();
    envelopeSmoother.reset();
    makeupGain.reset();
    std::fill(rmsAverages.begin(), rmsAverages.end(), 0.0f);
}

void AdvancedCompressorProcessor::releaseResources() {}

// RMS calculation using Exponential Moving Average (EMA)
float AdvancedCompressorProcessor::calculateRMS(int channel, float input)
{
    if (channel >= (int)rmsAverages.size()) return 0.0f;

    float squaredInput = input * input;
    // EMA: avg = alpha * avg + (1 - alpha) * input^2
    rmsAverages[channel] = rmsAlpha * rmsAverages[channel] + (1.0f - rmsAlpha) * squaredInput;
    return std::sqrt(rmsAverages[channel]);
}

// Gain calculation (Hard Knee implementation)
float AdvancedCompressorProcessor::calculateGainDb(float detectorDb, float thresholdDb, float ratio)
{
    if (detectorDb > thresholdDb)
    {
        return (thresholdDb - detectorDb) * (1.0f - (1.0f / ratio));
    }
    return 0.0f;
}

// Configures characteristics based on the selected topology (Blueprint 3.3)
void AdvancedCompressorProcessor::configureTopology(Topology topology, float attackMs, float releaseMs)
{
    switch (topology)
    {
    case Topology::VCA_Clean:
        // VCA: Fast, clean (Blueprint 3.3.1)
        envelopeSmoother.setAttackTime(attackMs);
        envelopeSmoother.setReleaseTime(releaseMs);
        colorationStage.functionToUse = [](float x) { return x; }; // Transparent
        break;
    case Topology::FET_Aggressive:
        // FET: Ultra-fast attack, aggressive coloration (Blueprint 3.3.2)
        envelopeSmoother.setAttackTime(juce::jmax(0.1f, attackMs * 0.5f)); // Faster attack
        envelopeSmoother.setReleaseTime(releaseMs);
        // FET coloration profile approximation
        colorationStage.functionToUse = [](float x) { return std::tanh(x * 1.5f); };
        break;
    case Topology::Opto_Smooth:
        // Opto: Slower attack, smoother release (Blueprint 3.3.3)
        envelopeSmoother.setAttackTime(juce::jmax(10.0f, attackMs * 1.5f)); // Inherently slower attack
        envelopeSmoother.setReleaseTime(releaseMs * 1.2f);
        // Opto/Tube coloration approximation
        colorationStage.functionToUse = [](float x) { return std::tanh(x * 0.8f); };
        break;
    }
}

void AdvancedCompressorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // P1 Boilerplate
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // 1. Get parameters (Safety check omitted for brevity)
    // Ensure parameters exist before accessing them
    if (!mainApvts.getRawParameterValue(thresholdParamId) || !mainApvts.getRawParameterValue(ratioParamId) ||
        !mainApvts.getRawParameterValue(attackParamId) || !mainApvts.getRawParameterValue(releaseParamId) ||
        !mainApvts.getRawParameterValue(makeupParamId) || !mainApvts.getRawParameterValue(topologyParamId) ||
        !mainApvts.getRawParameterValue(detectorParamId))
    {
        return;
    }

    float thresholdDb = mainApvts.getRawParameterValue(thresholdParamId)->load();
    float ratio = mainApvts.getRawParameterValue(ratioParamId)->load();
    float attackMs = mainApvts.getRawParameterValue(attackParamId)->load();
    float releaseMs = mainApvts.getRawParameterValue(releaseParamId)->load();
    float makeupDb = mainApvts.getRawParameterValue(makeupParamId)->load();
    auto topology = static_cast<Topology>(static_cast<int>(mainApvts.getRawParameterValue(topologyParamId)->load()));
    auto detectorMode = static_cast<DetectorMode>(static_cast<int>(mainApvts.getRawParameterValue(detectorParamId)->load()));

    // 2. Configure based on topology
    configureTopology(topology, attackMs, releaseMs);
    makeupGain.setGainDecibels(makeupDb);

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    // Process loop: Sample outer, Channel inner
    for (int i = 0; i < numSamples; ++i)
    {
        // Process channels independently
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float inputSample = buffer.getSample(ch, i);

            // --- DETECTOR STAGE (Blueprint 3.2.1) ---
            float detectorValue = 0.0f;
            if (detectorMode == DetectorMode::Peak)
            {
                // BallisticsFilter requires the channel index (ch)
                detectorValue = peakDetector.processSample(ch, std::abs(inputSample));
            }
            else // RMS
            {
                detectorValue = calculateRMS(ch, inputSample);
            }

            float detectorDb = juce::Decibels::gainToDecibels(detectorValue + 1e-9f);

            // --- GAIN COMPUTER ---
            float targetGainDb = calculateGainDb(detectorDb, thresholdDb, ratio);

            // --- ENVELOPE STAGE (Blueprint 3.2.2) ---
            // Smooth the gain changes. BallisticsFilter requires the channel index (ch).
            float smoothedGainDb = envelopeSmoother.processSample(ch, targetGainDb);
            float linearGain = juce::Decibels::decibelsToGain(smoothedGainDb);

            // --- APPLY GAIN & COLORATION ---
            float processedSample = inputSample * linearGain;

            // Apply coloration based on topology (WaveShaper takes 1 arg)
            processedSample = colorationStage.processSample(processedSample);

            // Apply makeup gain
            // FIX: dsp::Gain::processSample takes only 1 argument (the sample value).
            // The previous implementation incorrectly passed the channel index 'ch'.
            processedSample = makeupGain.processSample(processedSample);

            buffer.setSample(ch, i, processedSample);
        }
    }
}