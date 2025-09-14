//================================================================================
// File: FX_Modules/PhysicalResonatorProcessor.cpp (ORIGINAL + unused param fixes)
//================================================================================

#include "PhysicalResonatorProcessor.h"

// ===================== InternalExciter Implementation =====================

InternalExciter::InternalExciter()
{
    // Configure the envelope for a sharp, percussive burst.
    envParams.attack = 0.001f;
    envParams.sustain = 0.0f;
    envParams.decay = 0.05f;
    envParams.release = 0.01f;
    envelope.setParameters(envParams);
}

void InternalExciter::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    envelope.setSampleRate(sampleRate);
    colorFilter.prepare(spec);
    reset();
}

void InternalExciter::reset()
{
    envelope.reset();
    colorFilter.reset();
}

void InternalExciter::trigger()
{
    envelope.noteOn();
}

void InternalExciter::process(juce::dsp::AudioBlock<float>& outputBlock, float brightness, int noiseType)
{
    noiseGen.setType(noiseType == 1 ? DSPUtils::NoiseGenerator::NoiseType::Pink : DSPUtils::NoiseGenerator::NoiseType::White);

    // 'Brightness' controls the character of the strike (resonant band-pass).
    float cutoff = juce::jmap(brightness, 1000.0f, 12000.0f);
    float resonance = juce::jmap(brightness, 0.5f, 5.0f);

    colorFilter.setCutoffFrequency(cutoff);
    colorFilter.setResonance(resonance);
    colorFilter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);

    int numSamples = (int)outputBlock.getNumSamples();
    int numChannels = (int)outputBlock.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        float env = envelope.getNextSample();
        if (env < 1e-6f)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                outputBlock.setSample(ch, i, 0.0f);
            continue;
        }

        float noise = noiseGen.getNextSample();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            // StateVariableTPTFilter requires channel index in JUCE 8.
            float filteredNoise = colorFilter.processSample(ch, noise);
            outputBlock.setSample(ch, i, filteredNoise * env);
        }
    }
}

// ===================== ExcitationManager Implementation =====================

void ExcitationManager::prepare(const juce::dsp::ProcessSpec& spec)
{
    internalExciter.prepare(spec);
    transientDetector.prepare(spec);
    rmsDetector.prepare(spec);
    rmsDetector.setAttackTime(10.0f);
    rmsDetector.setReleaseTime(100.0f);
    reset();
}

void ExcitationManager::reset()
{
    internalExciter.reset();
    transientDetector.reset();
    rmsDetector.reset();
}

// Implements the smart normalization logic.
void ExcitationManager::process(const juce::dsp::AudioBlock<float>& inputBlock,
    juce::dsp::AudioBlock<float>& outputExcitationBlock,
    float brightness, float sensitivity, int noiseType)
{
    int numSamples = (int)inputBlock.getNumSamples();
    int numChannels = (int)inputBlock.getNumChannels();

    // Analyze the input block to determine if it's active ("patched").
    float totalRms = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            // BallisticsFilter needs channel index.
            totalRms += rmsDetector.processSample(ch, inputBlock.getSample(ch, i));
        }
    }

    bool inputIsActive = (totalRms / (float)(numSamples * numChannels)) > kInputThreshold;

    if (inputIsActive)
    {
        // If input is active, use the external audio as the excitation source.
        outputExcitationBlock.copyFrom(inputBlock);
        outputExcitationBlock.multiplyBy(sensitivity);
    }
    else
    {
        // If input is inactive, check for triggers (transients/gates on the input) to fire the internal exciter.
        for (int i = 0; i < numSamples; ++i)
        {
            float monoSample = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                monoSample += inputBlock.getSample(ch, i);

            if (numChannels > 0) monoSample /= (float)numChannels;

            transientDetector.processSample(monoSample);
            if (transientDetector.getTransientValue() > 0.8f) // High threshold for sharp triggers
            {
                internalExciter.trigger();
            }
        }
        // Process the internal exciter into the output buffer.
        internalExciter.process(outputExcitationBlock, brightness, noiseType);
    }
}

// ===================== ModalResonator Implementation =====================

void ModalResonator::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    initializeMaterialTables();

    if (channelFilters.size() != spec.numChannels)
        channelFilters.resize(spec.numChannels);

    // Filters operate independently per channel, prepare with mono spec.
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    for (auto& bank : channelFilters)
    {
        for (auto& f : bank)
        {
            f.prepare(monoSpec);
        }
    }
}

void ModalResonator::reset()
{
    for (auto& bank : channelFilters)
        for (auto& f : bank) f.reset();
}

// Initialize material tables with musically plausible ratios.
void ModalResonator::initializeMaterialTables()
{
    if (tablesInitialized) return;

    // Wood (Harmonic series: 1, 2, 3, 4...)
    for (int i = 0; i < NUM_MODES; ++i)
    {
        float n = (float)i + 1.0f;
        woodData.ratios[i] = n;
        woodData.gains[i] = 1.0f / (n * n + 10.0f);
        woodData.qs[i] = 50.0f + n * 20.0f;
    }

    // Metal (Vibraphone-like, slightly stretched harmonics)
    for (int i = 0; i < NUM_MODES; ++i)
    {
        float n = (float)i + 1.0f;
        const float stretchFactor = 0.01f;
        metalData.ratios[i] = n * (1.0f + stretchFactor * n);
        metalData.gains[i] = 1.0f / (n * 1.5f + 10.0f);
        metalData.qs[i] = 400.0f + n * 50.0f;
    }

    // Glass/Bell (Inharmonic, approximation of Chladni plate modes)
    for (int i = 0; i < NUM_MODES; ++i)
    {
        float n = (float)i + 1.0f;
        // Scaled down to keep frequencies manageable
        glassData.ratios[i] = (std::pow(n + 0.5f, 2.0f) * 0.25f) + 0.1f * n;
        glassData.gains[i] = 1.0f / (n * n * 0.5f + 20.0f);
        glassData.qs[i] = 1000.0f - n * 20.0f;
    }
    tablesInitialized = true;
}

void ModalResonator::computeModeParams(float tuneHz, float structure, float brightness, float damping, float position)
{
    // Structure: 0.0 Wood -> 0.5 Metal -> 1.0 Glass.
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    const float split = 0.5f;

    for (int m = 0; m < NUM_MODES; ++m)
    {
        float r, g, q;

        // 1. Calculate base material properties (Structure)
        if (structure < split)
        {
            float t = structure / split;
            t = t * t; // Non-linear curve for distinct material zones.
            r = lerp(woodData.ratios[m], metalData.ratios[m], t);
            g = lerp(woodData.gains[m], metalData.gains[m], t);
            q = lerp(woodData.qs[m], metalData.qs[m], t);
        }
        else
        {
            float t = (structure - split) / (1.0f - split);
            t = t * t;
            r = lerp(metalData.ratios[m], glassData.ratios[m], t);
            g = lerp(metalData.gains[m], glassData.gains[m], t);
            q = lerp(metalData.qs[m], glassData.qs[m], t);
        }

        // 2. Apply Brightness (Spectral Tilt)
        float brightAtten = std::pow(juce::jmap(brightness, 0.1f, 1.0f), (float)m * 0.05f);

        // 3. Apply Damping
        // Scales Q. Exponential mapping for wider control range.
        float dampingScale = std::pow(10.0f, juce::jmap(damping, 0.0f, 1.0f, 1.0f, -2.0f));

        // Frequency-dependent damping (coupled with Brightness).
        // Low brightness causes high frequencies to damp faster (higher damping factor).
        float freqDependentDamping = 1.0f + (1.0f - brightness) * ((float)m / (float)NUM_MODES) * 5.0f;
        dampingScale /= freqDependentDamping;


        // 4. Apply Position (Harmonic Cancellation)
        float pos = position * 2.0f;
        if (pos > 1.0f) pos = 2.0f - pos; // Mirrored response (0.5 is center)

        float posShape = 1.0f;
        // Attenuate even harmonics near the center (pos close to 1.0).
        if (pos > 0.8f && ((m + 1) % 2 == 0))
        {
            posShape = 1.0f - (pos - 0.8f) * 5.0f;
        }

        // Final assignments
        modeFreqs[m] = tuneHz * r;
        modeGains[m] = g * brightAtten * posShape;
        modeQs[m] = q * dampingScale;

        // Safety clamping
        modeFreqs[m] = juce::jlimit(20.0f, (float)sampleRate * 0.49f, modeFreqs[m]);
        modeQs[m] = juce::jlimit(10.0f, 20000.0f, modeQs[m]);
    }
}

void ModalResonator::process(const juce::dsp::AudioBlock<float>& excitationBlock,
    juce::dsp::AudioBlock<float>& outputBlock,
    float tune, float structure, float brightness, float damping, float position)
{
    if (channelFilters.empty()) return;

    float tuneHz = tuneToHz(tune);
    computeModeParams(tuneHz, structure, brightness, damping, position);

    int numChannels = (int)outputBlock.getNumChannels();
    int numSamples = (int)outputBlock.getNumSamples();

    // Update filter coefficients
    for (int ch = 0; ch < numChannels; ++ch)
    {
        if ((size_t)ch >= channelFilters.size()) continue;
        auto& bank = channelFilters[(size_t)ch];

        for (int m = 0; m < NUM_MODES; ++m)
        {
            *bank[m].coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, modeFreqs[m], modeQs[m]);
        }
    }

    // Process audio
    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if ((size_t)ch >= channelFilters.size()) continue;

            float in = excitationBlock.getSample(ch, i);
            float acc = 0.0f;
            auto& bank = channelFilters[(size_t)ch];

            // Parallel filter bank processing
            for (int m = 0; m < NUM_MODES; ++m)
            {
                // IIR::Filter::processSample does not take channel index when prepared with monoSpec.
                float filtered = bank[m].processSample(in);
                acc += filtered * modeGains[m];
            }

            const float globalGain = 0.1f; // Gain staging for 60 partials
            outputBlock.setSample(ch, i, acc * globalGain);
        }
    }
}

// ===================== SympatheticStringResonator Implementation =====================

void SympatheticStringResonator::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxDelaySamples = (int)(sampleRate / 20.0) + 100; // Max delay for 20Hz fundamental

    channelDelays.resize(spec.numChannels);
    channelFilters.resize(spec.numChannels);
    feedbackGains.resize(spec.numChannels);
    summedFeedbackState.resize(spec.numChannels, 0.0f);

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    for (size_t ch = 0; ch < spec.numChannels; ++ch)
    {
        for (int s = 0; s < NUM_STRINGS; ++s)
        {
            channelDelays[ch][s].setMaximumDelayInSamples(maxDelaySamples);
            channelDelays[ch][s].prepare(spec);
            channelFilters[ch][s].prepare(monoSpec);
        }
    }
    reset();
}

void SympatheticStringResonator::reset()
{
    for (auto& bank : channelDelays) for (auto& d : bank) d.reset();
    for (auto& bank : channelFilters) for (auto& f : bank) f.reset();
    for (auto& bank : feedbackGains) std::fill(bank.begin(), bank.end(), 0.0f);
    std::fill(summedFeedbackState.begin(), summedFeedbackState.end(), 0.0f);
}

void SympatheticStringResonator::updateTunings(float structure)
{
    // Smoothly interpolate between musical interval sets.
    const std::array<float, 6> unison = { 1.0f, 2.0f, 0.5f, 4.0f, 1.01f, 0.99f };
    const std::array<float, 6> fifths = { 1.0f, 1.5f, 2.0f, 3.0f, 0.5f, 0.75f };
    const std::array<float, 6> major = { 1.0f, 1.25f, 1.5f, 2.0f, 2.5f, 3.0f };
    const std::array<float, 6> minor = { 1.0f, 1.189f, 1.5f, 2.0f, 2.378f, 3.0f }; // Approx minor third

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    if (structure < 0.33f)
    {
        float t = structure / 0.33f;
        for (int i = 0; i < NUM_STRINGS; ++i) currentRatios[i] = lerp(unison[i], fifths[i], t);
    }
    else if (structure < 0.66f)
    {
        float t = (structure - 0.33f) / 0.33f;
        for (int i = 0; i < NUM_STRINGS; ++i) currentRatios[i] = lerp(fifths[i], major[i], t);
    }
    else
    {
        float t = (structure - 0.66f) / 0.34f;
        for (int i = 0; i < NUM_STRINGS; ++i) currentRatios[i] = lerp(major[i], minor[i], t);
    }
}

void SympatheticStringResonator::process(const juce::dsp::AudioBlock<float>& excitationBlock,
    juce::dsp::AudioBlock<float>& outputBlock,
    float tune, float structure, float brightness, float damping, float position)
{
    juce::ignoreUnused(position); // unused param warning fix
    float tuneHz = tuneToHz(tune);
    updateTunings(structure);

    float feedbackGain = std::pow(damping, 0.3f) * 0.998f;
    float brightnessCutoff = juce::jmap(brightness, 500.0f, (float)sampleRate * 0.45f);

    int numChannels = (int)outputBlock.getNumChannels();
    int numSamples = (int)outputBlock.getNumSamples();

    // Update Delays and Filters
    for (int ch = 0; ch < numChannels; ++ch)
    {
        if ((size_t)ch >= channelDelays.size()) continue;

        for (int s = 0; s < NUM_STRINGS; ++s)
        {
            float freq = tuneHz * currentRatios[s];
            float delayTimeSamples = (float)sampleRate / juce::jlimit(20.0f, (float)sampleRate * 0.45f, freq);
            channelDelays[ch][s].setDelay(delayTimeSamples);
            channelFilters[ch][s].setCutoffFrequency(brightnessCutoff);
        }
    }

    // Process Audio with Coupling (All-to-all mixing)
    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if ((size_t)ch >= channelDelays.size()) continue;

            float in = excitationBlock.getSample(ch, i);
            float acc = 0.0f;
            float currentSummedFeedback = 0.0f;

            // Process each string
            for (int s = 0; s < NUM_STRINGS; ++s)
            {
                float delayedSample = channelDelays[ch][s].popSample(ch);

                // Apply damping filter. FirstOrderTPTFilter prepared with monoSpec uses channel 0.
                float dampedSample = channelFilters[ch][s].processSample(0, delayedSample);

                // Calculate input to the delay line: Excitation + Feedback + Coupling term.
                const float couplingFactor = 0.1f;
                // summedFeedbackState[ch] holds the value from the previous sample iteration (i-1).
                float newSample = in + feedbackGains[ch][s] + summedFeedbackState[ch] * couplingFactor;

                channelDelays[ch][s].pushSample(ch, newSample);

                // Update feedback gain storage for the next sample
                feedbackGains[ch][s] = dampedSample * feedbackGain;

                currentSummedFeedback += feedbackGains[ch][s];
                acc += dampedSample;
            }

            // Update the state for the next iteration (i+1)
            summedFeedbackState[ch] = currentSummedFeedback;

            // Write final output (scaled down)
            outputBlock.setSample(ch, i, acc * 0.25f);
        }
    }
}

// ===================== StringResonator (Karplus-Strong) Implementation =====================

void StringResonator::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxDelaySamples = (int)(sampleRate / 20.0) + 100;

    channelDelays.resize(spec.numChannels);
    channelDampingFilters.resize(spec.numChannels);
    channelDispersionFilters1.resize(spec.numChannels);
    channelDispersionFilters2.resize(spec.numChannels);
    feedback.resize(spec.numChannels, 0.0f);

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    for (size_t ch = 0; ch < spec.numChannels; ++ch)
    {
        channelDelays[ch].setMaximumDelayInSamples(maxDelaySamples);
        channelDelays[ch].prepare(spec);
        channelDampingFilters[ch].prepare(monoSpec);
        channelDispersionFilters1[ch].prepare(monoSpec);
        channelDispersionFilters2[ch].prepare(monoSpec);
    }
    reset();
}

void StringResonator::reset()
{
    for (auto& d : channelDelays) d.reset();
    for (auto& f : channelDampingFilters) f.reset();
    for (auto& f : channelDispersionFilters1) f.reset();
    for (auto& f : channelDispersionFilters2) f.reset();
    std::fill(feedback.begin(), feedback.end(), 0.0f);
}

void StringResonator::process(const juce::dsp::AudioBlock<float>& excitationBlock,
    juce::dsp::AudioBlock<float>& outputBlock,
    float tune, float structure, float brightness, float damping, float position)
{
    juce::ignoreUnused(position); // unused param warning fix
    float tuneHz = tuneToHz(tune);
    float baseDelayTimeSamples = (float)sampleRate / juce::jlimit(20.0f, (float)sampleRate * 0.45f, tuneHz);

    float feedbackGain = std::pow(damping, 0.4f) * 0.999f;
    float brightnessCutoff = juce::jmap(brightness, 800.0f, (float)sampleRate * 0.48f);
    float dispersionAmount = juce::jmap(structure, 0.0f, 0.5f); // Structure controls inharmonicity

    int numChannels = (int)outputBlock.getNumChannels();
    int numSamples = (int)outputBlock.getNumSamples();

    // Configure Filters and Delay Compensation
    for (int ch = 0; ch < numChannels; ++ch)
    {
        if ((size_t)ch >= channelDelays.size()) continue;

        // 1. Configure absorption filter (one-pole LPF)
        channelDampingFilters[ch].setCutoffFrequency(brightnessCutoff);

        // 2. Configure dispersion filters (two cascaded all-pass filters)
        float f1 = juce::jmap(dispersionAmount, 0.25f, 0.5f) * (float)sampleRate;
        float f2 = juce::jmap(dispersionAmount, 0.1f, 0.25f) * (float)sampleRate;

        *channelDispersionFilters1[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, f1);
        *channelDispersionFilters2[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, f2);

        // 3. Delay Compensation (Crucial for pitch stability)
        // Improved approximation for the phase delay of the filters.

        // Approximation for the one-pole LPF phase delay
        float dampingPhaseDelay = 0.0f;
        if (brightnessCutoff < (float)sampleRate * 0.5f)
        {
            // Phase delay estimation based on cutoff frequency relative to Nyquist
            dampingPhaseDelay = std::atan(brightnessCutoff / ((float)sampleRate * 0.5f)) / juce::MathConstants<float>::pi;
        }

        // Heuristic approximation for the all-pass filters
        float dispersionPhaseDelay = dispersionAmount * 4.0f;

        float totalPhaseDelay = dampingPhaseDelay + dispersionPhaseDelay;

        // Apply compensated delay time
        float compensatedDelay = baseDelayTimeSamples - totalPhaseDelay;
        channelDelays[ch].setDelay(juce::jmax(1.0f, compensatedDelay));
    }

    // Process Audio (Extended Karplus-Strong Loop)
    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if ((size_t)ch >= channelDelays.size()) continue;

            float in = excitationBlock.getSample(ch, i);
            float delayedSample = channelDelays[ch].popSample(ch);

            // Feedback loop processing (The "String")

            // 1. Absorption (Damping). TPT filters prepared with monoSpec use channel 0.
            float dampedSample = channelDampingFilters[ch].processSample(0, delayedSample);

            // 2. Dispersion (Inharmonicity). IIR filters (monoSpec) do not use channel index.
            float dispersedSample1 = channelDispersionFilters1[ch].processSample(dampedSample);
            float dispersedSample2 = channelDispersionFilters2[ch].processSample(dispersedSample1);

            // Calculate input to the delay line (Excitation + Feedback)
            float newSample = in + feedback[ch];
            channelDelays[ch].pushSample(ch, newSample);

            // Update feedback storage
            feedback[ch] = dispersedSample2 * feedbackGain;

            // Output the result
            outputBlock.setSample(ch, i, dispersedSample2 * 0.8f);
        }
    }
}

// ===================== PhysicalResonatorProcessor Implementation =====================

PhysicalResonatorProcessor::PhysicalResonatorProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
{
    // Initialize parameter IDs based on the plugin's naming scheme
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_PHYSRES_";
    modelParamId = slotPrefix + "MODEL";
    tuneParamId = slotPrefix + "TUNE";
    structureParamId = slotPrefix + "STRUCTURE";
    brightnessParamId = slotPrefix + "BRIGHTNESS";
    dampingParamId = slotPrefix + "DAMPING";
    positionParamId = slotPrefix + "POSITION";
    sensitivityParamId = slotPrefix + "SENSITIVITY";
    mixParamId = slotPrefix + "MIX";
    noiseTypeParamId = slotPrefix + "NOISE_TYPE";
}

void PhysicalResonatorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::uint32 numChannels = (juce::uint32)std::max(getTotalNumInputChannels(), getTotalNumOutputChannels());
    if (numChannels == 0) numChannels = 2;

    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, numChannels };

    excitationManager.prepare(spec);
    modalResonator.prepare(spec);
    sympatheticResonator.prepare(spec);
    stringResonator.prepare(spec);

    safetyLimiter.prepare(spec);
    safetyLimiter.setThreshold(-0.5f); // dB
    safetyLimiter.setRelease(50.0f);

    excitationBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);
    wetOutputBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);

    double smoothingTime = 0.02; // 20ms
    smoothedTune.reset(sampleRate, smoothingTime);
    smoothedStructure.reset(sampleRate, smoothingTime);
    smoothedBrightness.reset(sampleRate, smoothingTime);
    smoothedDamping.reset(sampleRate, smoothingTime);
    smoothedPosition.reset(sampleRate, smoothingTime);
    smoothedMix.reset(sampleRate, smoothingTime);

    updateResonatorCore((int)mainApvts.getRawParameterValue(modelParamId)->load());
    reset();
}

void PhysicalResonatorProcessor::releaseResources()
{
    reset();
}

void PhysicalResonatorProcessor::reset()
{
    excitationManager.reset();
    modalResonator.reset();
    sympatheticResonator.reset();
    stringResonator.reset();
    safetyLimiter.reset();
    instabilityFlag = false;
}

void PhysicalResonatorProcessor::updateResonatorCore(int newModelIndex)
{
    if (newModelIndex == currentModelIndex && activeResonator != nullptr) return;

    if (activeResonator) activeResonator->reset();
    currentModelIndex = newModelIndex;

    switch (newModelIndex)
    {
    case 0: activeResonator = &modalResonator; break;
    case 1: activeResonator = &sympatheticResonator; break;
    case 2: activeResonator = &stringResonator; break;
    default: activeResonator = &modalResonator; break;
    }
}

bool PhysicalResonatorProcessor::checkAndHandleInstability(float sampleValue)
{
    // Check for NaN, Inf, or excessively large values which indicate DSP instability.
    if (std::isnan(sampleValue) || std::isinf(sampleValue) || std::abs(sampleValue) > 50.0f)
    {
        if (!instabilityFlag)
        {
            instabilityFlag = true;
            // Reset resonators to silence immediately if instability occurs.
            reset();
        }
        return true;
    }
    return false;
}

void PhysicalResonatorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalIn = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();

    for (auto i = totalIn; i < totalOut; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    int numSamples = buffer.getNumSamples();
    int numChannels = std::max(totalIn, totalOut);

    if (numChannels == 0) return;

    instabilityFlag = false;

    // Handle Model Switching
    int modelIndex = (int)mainApvts.getRawParameterValue(modelParamId)->load();
    updateResonatorCore(modelIndex);

    if (!activeResonator) return;

    // Parameter Handling
    auto getParam = [&](const juce::String& id) {
        if (auto* p = mainApvts.getRawParameterValue(id)) return p->load();
        return 0.0f;
        };

    // Update smoothed parameter targets
    smoothedTune.setTargetValue(getParam(tuneParamId));
    smoothedStructure.setTargetValue(getParam(structureParamId));
    smoothedBrightness.setTargetValue(getParam(brightnessParamId));
    smoothedDamping.setTargetValue(getParam(dampingParamId));
    smoothedPosition.setTargetValue(getParam(positionParamId));
    smoothedMix.setTargetValue(getParam(mixParamId));

    // Ensure buffers are correctly sized
    if (excitationBuffer.getNumSamples() < numSamples || excitationBuffer.getNumChannels() < numChannels)
    {
        excitationBuffer.setSize(numChannels, numSamples, false, true, true);
        wetOutputBuffer.setSize(numChannels, numSamples, false, true, true);
    }

    // Create AudioBlocks
    juce::dsp::AudioBlock<float> mainBlock(buffer);
    juce::dsp::AudioBlock<float> excitationBlock(excitationBuffer);
    juce::dsp::AudioBlock<float> wetBlock(wetOutputBuffer);

    // 1. Generate Excitation Signal
    excitationManager.process(mainBlock, excitationBlock,
        getParam(brightnessParamId), // Brightness affects internal exciter
        getParam(sensitivityParamId),
        (int)getParam(noiseTypeParamId));

    // 2. Process through Resonator (Sample by sample for smoothing)
    for (int i = 0; i < numSamples; ++i)
    {
        float tune = smoothedTune.getNextValue();
        float structure = smoothedStructure.getNextValue();
        float brightness = smoothedBrightness.getNextValue();
        float damping = smoothedDamping.getNextValue();
        float position = smoothedPosition.getNextValue();

        // Get single-sample sub-blocks
        auto exciteSample = excitationBlock.getSubBlock((size_t)i, 1);
        auto wetSample = wetBlock.getSubBlock((size_t)i, 1);

        activeResonator->process(exciteSample, wetSample, tune, structure, brightness, damping, position);
    }

    // 3. Safety Checks and Limiting
    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            if (checkAndHandleInstability(wetBlock.getSample(ch, i)))
            {
                wetBlock.clear();
                break;
            }
        }
        if (instabilityFlag) break;
    }

    if (!instabilityFlag)
    {
        safetyLimiter.process(juce::dsp::ProcessContextReplacing<float>(wetBlock));
    }

    // 4. Dry/Wet Mix (Equal Power Crossfade)
    for (int i = 0; i < numSamples; ++i)
    {
        float mix = smoothedMix.getNextValue();
        float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
        float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);

        for (int ch = 0; ch < totalOut; ++ch)
        {
            float dry = (ch < totalIn) ? buffer.getSample(ch, i) : 0.0f;
            float wet = (ch < (int)wetBlock.getNumChannels()) ? wetBlock.getSample(ch, i) : 0.0f;

            float out = dry * dryGain + wet * wetGain;
            buffer.setSample(ch, i, out);
        }
    }
}
