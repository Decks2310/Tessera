#include "MorphoCompProcessor.h"
// REMOVED: Internal SignalAnalyzer implementation.

MorphoCompProcessor::MorphoCompProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
{
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_";
    amountParamId = slotPrefix + "MORPHO_AMOUNT";
    responseParamId = slotPrefix + "MORPHO_RESPONSE";
    modeParamId = slotPrefix + "MORPHO_MODE";
    morphXParamId = slotPrefix + "MORPHO_X";
    morphYParamId = slotPrefix + "MORPHO_Y";
    mixParamId = slotPrefix + "MORPHO_MIX";
}

void MorphoCompProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumInputChannels() };

    // UPDATED: Prepare the new analyzers
    spectralAnalyzer.prepare(spec);
    transientDetector.prepare(spec);

    compressor.prepare(spec);
    saturator.prepare(spec);

    // Initial smoothing time (will be updated dynamically in processBlock)
    morphXSmoother.reset(sampleRate, 0.1);
    morphYSmoother.reset(sampleRate, 0.1);

    // NEW: Report latency introduced by the analyzers.
    // The latency is determined by the hop size of the analyzers (e.g., 256 samples).
    setLatencySamples(transientDetector.getLatencyInSamples());

    reset();
}

void MorphoCompProcessor::releaseResources() {
    reset();
}

void MorphoCompProcessor::reset() {
    // UPDATED: Reset the new analyzers
    spectralAnalyzer.reset();
    transientDetector.reset();

    compressor.reset();
    saturator.reset();
    morphXSmoother.setCurrentAndTargetValue(0.5f);
    morphYSmoother.setCurrentAndTargetValue(0.5f);
}

template <typename T>
T bilinearInterp(T c00, T c10, T c01, T c11, float tx, float ty)
{
    T a = c00 * (1.0f - tx) + c10 * tx;
    T b = c01 * (1.0f - tx) + c11 * tx;
    return a * (1.0f - ty) + b * ty;
}

void MorphoCompProcessor::updateCompressorAndSaturation(float amount, float response, float morphX, float morphY)
{
    float attackFactor = bilinearInterp(Topologies::VCA.attackFactor, Topologies::FET.attackFactor,
        Topologies::Opto.attackFactor, Topologies::VariMu.attackFactor, morphX, morphY);
    float releaseFactor = bilinearInterp(Topologies::VCA.releaseFactor, Topologies::FET.releaseFactor,
        Topologies::Opto.releaseFactor, Topologies::VariMu.releaseFactor, morphX, morphY);
    float ratioFactor = bilinearInterp(Topologies::VCA.ratioFactor, Topologies::FET.ratioFactor,
        Topologies::Opto.ratioFactor, Topologies::VariMu.ratioFactor, morphX, morphY);
    float saturationDrive = bilinearInterp(Topologies::VCA.saturationDrive, Topologies::FET.saturationDrive,
        Topologies::Opto.saturationDrive, Topologies::VariMu.saturationDrive, morphX, morphY);

    float baseThreshold = juce::jmap(amount, 0.0f, -40.0f);
    float baseRatio = juce::jmap(amount, 1.5f, 8.0f);

    float baseAttack = std::pow(10.0f, juce::jmap(response, 2.0f, 0.0f));
    float baseRelease = std::pow(10.0f, juce::jmap(response, 3.0f, 1.5f));

    compressor.setThreshold(baseThreshold);
    compressor.setRatio(juce::jlimit(1.0f, 20.0f, baseRatio * ratioFactor));
    compressor.setAttack(juce::jlimit(0.1f, 500.0f, baseAttack * attackFactor));
    compressor.setRelease(juce::jlimit(5.0f, 2000.0f, baseRelease * releaseFactor));

    currentSaturationDrive = 1.0f + saturationDrive;

    if (morphX > 0.5f && morphY < 0.5f) saturator.functionToUse = Topologies::FET.saturationFunc;
    else if (morphX < 0.5f && morphY > 0.5f) saturator.functionToUse = Topologies::Opto.saturationFunc;
    else if (morphX > 0.5f && morphY > 0.5f) saturator.functionToUse = Topologies::VariMu.saturationFunc;
    else saturator.functionToUse = Topologies::VCA.saturationFunc;
}

void MorphoCompProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    // P1 Boilerplate
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    int numSamples = buffer.getNumSamples();
    int numChannels = totalNumInputChannels;

    // 1. Run Analysis (Sample-by-sample)
    // We must run the analysis sample-by-sample to update the analyzer states correctly over time.
    for (int i = 0; i < numSamples; ++i)
    {
        // Calculate mono sum
        float monoSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            monoSample += buffer.getSample(ch, i);
        }
        if (numChannels > 0)
            monoSample /= (float)numChannels;

        // Process analysis using the refactored sample-based interface
        spectralAnalyzer.processSample(monoSample);
        transientDetector.processSample(monoSample);
    }


    // 2. Get Parameters and Analysis Results
    float amount = mainApvts.getRawParameterValue(amountParamId)->load();
    float response = mainApvts.getRawParameterValue(responseParamId)->load();
    bool autoMorph = mainApvts.getRawParameterValue(modeParamId)->load() > 0.5f; // 0=Auto, 1=Manual - Corrected logic
    float mix = mainApvts.getRawParameterValue(mixParamId)->load();

    float targetX, targetY;
    if (autoMorph)
    {
        // Get analysis results. Since we ran them through the whole block,
        // these return the state at the END of the block (which is fine for setting the target).
        targetX = transientDetector.getTransientValue();
        // Invert centroid (Low centroid = High Y, more 'Opto/VariMu')
        targetY = 1.0f - spectralAnalyzer.getSpectralCentroid();
    }
    else
    {
        targetX = mainApvts.getRawParameterValue(morphXParamId)->load();
        targetY = mainApvts.getRawParameterValue(morphYParamId)->load();
    }

    // 3. Configure Smoothing
    // FIX: Dynamic Smoothing Time
    // Auto mode needs moderate smoothing (200ms), Manual mode needs fast smoothing (50ms).
    double smoothingTime = autoMorph ? 0.2 : 0.05;

    // Update the smoother's ramp length dynamically.
    if (getSampleRate() > 0)
    {
        morphXSmoother.reset(getSampleRate(), smoothingTime);
        morphYSmoother.reset(getSampleRate(), smoothingTime);
    }

    morphXSmoother.setTargetValue(targetX);
    morphYSmoother.setTargetValue(targetY);

    // 4. Synchronize Smoothers and Update Parameters
    // FIX: Correct Smoother Synchronization (Required for block processing)
    // Advance the smoother once to get the starting value for the block.
    float currentX = morphXSmoother.getNextValue();
    float currentY = morphYSmoother.getNextValue();

    // Skip the remaining samples in the block to keep the smoother synchronized.
    if (numSamples > 1)
    {
        morphXSmoother.skip(numSamples - 1);
        morphYSmoother.skip(numSamples - 1);
    }

    // Update compressor parameters based on the smoothed position
    updateCompressorAndSaturation(amount, response, currentX, currentY);

    // 5. Audio Processing (Block-wise)
    juce::dsp::AudioBlock<float> block(buffer);

    juce::AudioBuffer<float> dryBuffer;
    if (mix < 1.0f) dryBuffer.makeCopyOf(buffer);

    juce::dsp::ProcessContextReplacing<float> context(block);
    compressor.process(context);

    if (currentSaturationDrive > 1.01f)
    {
        block.multiplyBy(currentSaturationDrive);
        saturator.process(context);
        block.multiplyBy(1.0f / currentSaturationDrive);
    }

    if (mix < 1.0f)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            if (ch < dryBuffer.getNumChannels())
            {
                buffer.addFrom(ch, 0, dryBuffer, ch, 0, numSamples, 1.0f - mix);
                buffer.applyGain(ch, 0, numSamples, mix);
            }
        }
    }
}
