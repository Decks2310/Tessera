//================================================================================
// File: FX_Modules/PhysicalResonatorProcessor.cpp
//================================================================================
#include "PhysicalResonatorProcessor.h"

//================================================================================
// FIX (LNK2001): ModalResonator Implementation (Placeholders)
//================================================================================
void ModalResonator::prepare(const juce::dsp::ProcessSpec& spec) {
    // Placeholder implementation
    juce::ignoreUnused(spec);
    // Initialize modal synthesis engine here
}
void ModalResonator::reset() {
    // Placeholder implementation
    // Reset modal synthesis engine state here
}
void ModalResonator::process(const juce::dsp::AudioBlock<float>& excitationBlock, juce::dsp::AudioBlock<float>& outputBlock,
    float tuneHz, float structure, float brightness, float damping, float position) {
    // Placeholder implementation
    juce::ignoreUnused(excitationBlock, tuneHz, structure, brightness, damping, position);

    // Ensure silence if not processing
    if (outputBlock.getNumSamples() > 0 && outputBlock.getNumChannels() > 0)
    {
        outputBlock.clear();
    }
    // Implement the actual modal synthesis processing here.
}

// =============================================================================
// ExcitationGenerator Implementation
// =============================================================================

ExcitationGenerator::ExcitationGenerator()
{
    noiseGen.setType(DSPUtils::NoiseGenerator::NoiseType::White);
}

void ExcitationGenerator::prepare(const juce::dsp::ProcessSpec& spec) {
    sampleRate = spec.sampleRate;
    transientDetector.prepare(spec);
    spectralAnalyzer.prepare(spec);

    // Configure color filter (Stereo)
    colorFilter.prepare(spec);
    // Using Bandpass filtering often yields more focused excitation
    colorFilter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);

    burstEnvelope.setSampleRate(sampleRate);

    // FIX: Initialize smoothers (50ms)
    smoothedExciteType.reset(sampleRate, 0.05);
    smoothedSensitivity.reset(sampleRate, 0.05);

    reset();
}

void ExcitationGenerator::reset() {
    transientDetector.reset();
    spectralAnalyzer.reset();
    colorFilter.reset();
    burstEnvelope.reset();
    // FIX: Reset smoothers
    smoothedExciteType.setCurrentAndTargetValue(0.5f);
    smoothedSensitivity.setCurrentAndTargetValue(0.5f);
}

// FIX: Updated process loop for time-aligned analysis and smoothed parameters.
void ExcitationGenerator::process(const juce::dsp::AudioBlock<float>& inputBlock, juce::dsp::AudioBlock<float>& outputExcitationBlock, const ExcitationParams& params) {
    // 1. Set Noise Type
    noiseGen.setType(params.noiseType == 1 ? DSPUtils::NoiseGenerator::NoiseType::Pink : DSPUtils::NoiseGenerator::NoiseType::White);

    // 2. Update ADSR Parameters (Block-wise update is fine)
    juce::ADSR::Parameters adsrParams;
    adsrParams.attack = params.attack;
    adsrParams.decay = params.decay;
    adsrParams.sustain = params.sustain;
    adsrParams.release = params.release;
    burstEnvelope.setParameters(adsrParams);

    // FIX: Update Smoother Targets
    smoothedExciteType.setTargetValue(params.exciteType);
    smoothedSensitivity.setTargetValue(params.sensitivity);

    // Rely on the inputBlock dimensions.
    int numSamples = (int)inputBlock.getNumSamples();
    int numChannels = (int)inputBlock.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        // --- Analysis (Per-Sample) ---

        // Calculate mono sum for analysis
        float monoSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            monoSample += inputBlock.getSample(ch, i);
        }
        if (numChannels > 0)
            monoSample /= (float)numChannels;

        // Analyze input features sample-by-sample using the refactored interface
        transientDetector.processSample(monoSample);
        spectralAnalyzer.processSample(monoSample);

        // Get smoothed parameters for this sample
        float currentExciteType = smoothedExciteType.getNextValue();
        float currentSensitivity = smoothedSensitivity.getNextValue();

        // Use spectral centroid to adaptively color the noise burst
        float centroid = spectralAnalyzer.getSpectralCentroid();
        float cutoff = juce::jmap(centroid, 500.0f, 10000.0f);
        // Update filter cutoff (TPT filters handle smooth updates internally)
        colorFilter.setCutoffFrequency(cutoff);

        // Define trigger threshold based on smoothed sensitivity
        float triggerThreshold = juce::jmap(currentSensitivity, 0.8f, 0.2f);
        float transientLevel = transientDetector.getTransientValue();


        // Triggering logic (Blueprint 3.2)
        if (transientLevel > triggerThreshold && !burstEnvelope.isActive())
            burstEnvelope.noteOn();

        // Handle noteOff for sustained sounds (using original params.sustain for duration check)
        if (params.sustain > 0.001f && transientLevel < (triggerThreshold * 0.5f) && burstEnvelope.isActive())
        {
            burstEnvelope.noteOff();
        }

        float envelopeValue = burstEnvelope.getNextSample();

        // --- Synthesis (Per-Sample, Per-Channel) ---
        for (int ch = 0; ch < numChannels; ++ch)
        {
            // Source A: Continuous Input (using smoothed sensitivity)
            float continuous = inputBlock.getSample(ch, i) * currentSensitivity;

            // Source B: Impulsive Noise Burst
            float noise = noiseGen.getNextSample();
            // Apply filtering to the noise burst (Stereo filter)
            float filteredNoise = colorFilter.processSample(ch, noise);
            float impulsive = filteredNoise * envelopeValue;

            // Crossfade (using smoothed exciteType)
            float excitation = (continuous * (1.0f - currentExciteType)) + (impulsive * currentExciteType);

            // Ensure output block matches input block channel count (Safety assertion)
            jassert(ch < (int)outputExcitationBlock.getNumChannels());
            outputExcitationBlock.setSample(ch, i, excitation);
        }
    }
}

// =============================================================================
// PhysicalResonatorProcessor Implementation
// =============================================================================
PhysicalResonatorProcessor::PhysicalResonatorProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
{
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_PHYSRES_";
    modelParamId = slotPrefix + "MODEL";
    tuneParamId = slotPrefix + "TUNE";
    structureParamId = slotPrefix + "STRUCTURE";
    brightnessParamId = slotPrefix + "BRIGHTNESS";
    dampingParamId = slotPrefix + "DAMPING";
    positionParamId = slotPrefix + "POSITION";
    exciteTypeParamId = slotPrefix + "EXCITE_TYPE";
    sensitivityParamId = slotPrefix + "SENSITIVITY";
    mixParamId = slotPrefix + "MIX";
    noiseTypeParamId = slotPrefix + "NOISE_TYPE";
    attackParamId = slotPrefix + "ATTACK";
    decayParamId = slotPrefix + "DECAY";
    sustainParamId = slotPrefix + "SUSTAIN";
    releaseParamId = slotPrefix + "RELEASE";
}

void PhysicalResonatorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    // Configure spec based on input channels
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumInputChannels() };

    // Initialize components
    excitationGenerator.prepare(spec);

    // Prepare all resonator instances
    modalResonator.prepare(spec);
    sympatheticResonator.prepare(spec);
    stringResonator.prepare(spec);

    // Ensure buffers are correctly sized based on input channels
    excitationBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);
    wetOutputBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);

    // Initialize smoothers
    double smoothingTime = 0.05; // 50ms smoothing
    smoothedTune.reset(sampleRate, smoothingTime);
    smoothedStructure.reset(sampleRate, smoothingTime);
    smoothedBrightness.reset(sampleRate, smoothingTime);
    smoothedDamping.reset(sampleRate, smoothingTime);
    smoothedPosition.reset(sampleRate, smoothingTime);
    // FIX: Removed ExciteType and Sensitivity smoothers (now in ExcitationGenerator)
    smoothedMix.reset(sampleRate, smoothingTime);

    reset();
}

void PhysicalResonatorProcessor::releaseResources() { reset(); }
void PhysicalResonatorProcessor::reset() {
    excitationGenerator.reset();
    modalResonator.reset();
    sympatheticResonator.reset();
    stringResonator.reset();
}

void PhysicalResonatorProcessor::updateResonatorCore(int newModelIndex) {
    // FIX (C4100): The parameter 'newModelIndex' is unreferenced in the current placeholder implementation.
    juce::ignoreUnused(newModelIndex);

    // Future implementation for dynamic switching goes here.
}

void PhysicalResonatorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    // [Boilerplate and setup - Identical to input]
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    int numSamples = buffer.getNumSamples();
    int numChannels = totalNumInputChannels;
    if (numChannels == 0) return;


    // 1. Update Model if necessary
    int modelIndex = static_cast<int>(mainApvts.getRawParameterValue(modelParamId)->load());
    updateResonatorCore(modelIndex);

    if (activeResonator == nullptr) return;

    // 2. Update Smoothers (Targets set once per block)
    smoothedTune.setTargetValue(mainApvts.getRawParameterValue(tuneParamId)->load());
    smoothedStructure.setTargetValue(mainApvts.getRawParameterValue(structureParamId)->load());
    smoothedBrightness.setTargetValue(mainApvts.getRawParameterValue(brightnessParamId)->load());
    smoothedDamping.setTargetValue(mainApvts.getRawParameterValue(dampingParamId)->load());
    smoothedPosition.setTargetValue(mainApvts.getRawParameterValue(positionParamId)->load());
    // FIX: Removed ExciteType and Sensitivity smoother updates
    smoothedMix.setTargetValue(mainApvts.getRawParameterValue(mixParamId)->load());

    // [Buffer Management and AudioBlock definitions]
    if (excitationBuffer.getNumSamples() < numSamples || excitationBuffer.getNumChannels() < numChannels)
    {
        excitationBuffer.setSize(numChannels, numSamples, false, true, true);
        wetOutputBuffer.setSize(numChannels, numSamples, false, true, true);
    }
    juce::dsp::AudioBlock<float> dryBlock(buffer);
    juce::dsp::AudioBlock<float> excitationBlock(excitationBuffer);
    juce::dsp::AudioBlock<float> wetBlock(wetOutputBuffer);
    auto activeDryBlock = dryBlock.getSubBlock(0, (size_t)numSamples).getSubsetChannelBlock(0, (size_t)numChannels);
    auto activeExcitationBlock = excitationBlock.getSubBlock(0, (size_t)numSamples);
    auto activeWetBlock = wetBlock.getSubBlock(0, (size_t)numSamples);
    activeExcitationBlock.clear();
    activeWetBlock.clear();

    // 3. STAGE 1: Generate Excitation

    // NEW: Gather parameters for the Excitation Generator (Read directly from APVTS)
    // The generator now handles the smoothing internally.
    ExcitationGenerator::ExcitationParams exciteParams;
    exciteParams.exciteType = mainApvts.getRawParameterValue(exciteTypeParamId)->load();
    exciteParams.sensitivity = mainApvts.getRawParameterValue(sensitivityParamId)->load();
    exciteParams.noiseType = static_cast<int>(mainApvts.getRawParameterValue(noiseTypeParamId)->load());
    exciteParams.attack = mainApvts.getRawParameterValue(attackParamId)->load();
    exciteParams.decay = mainApvts.getRawParameterValue(decayParamId)->load();
    exciteParams.sustain = mainApvts.getRawParameterValue(sustainParamId)->load();
    exciteParams.release = mainApvts.getRawParameterValue(releaseParamId)->load();

    // Generate excitation
    excitationGenerator.process(activeDryBlock, activeExcitationBlock, exciteParams);

    // 4. STAGE 2: Process Resonator Core
    for (int i = 0; i < numSamples; ++i)
    {
        float tune = smoothedTune.getNextValue();
        float structure = smoothedStructure.getNextValue();
        float brightness = smoothedBrightness.getNextValue();
        float damping = smoothedDamping.getNextValue();
        float position = smoothedPosition.getNextValue();
        auto excitationSampleBlock = activeExcitationBlock.getSubBlock((size_t)i, 1);
        auto wetOutputSampleBlock = activeWetBlock.getSubBlock((size_t)i, 1);
        activeResonator->process(excitationSampleBlock, wetOutputSampleBlock, tune, structure, brightness, damping, position);
    }

    // 5. STAGE 3: Final Mix
    for (int i = 0; i < numSamples; ++i)
    {
        float mix = smoothedMix.getNextValue();
        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        {
            float dry = (ch < numChannels) ? buffer.getSample(ch, i) : 0.0f;
            float wet = (ch < numChannels) ? activeWetBlock.getSample(ch, i) : 0.0f;
            buffer.setSample(ch, i, (dry * (1.0f - mix)) + (wet * mix));
        }
    }
}