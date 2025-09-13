//================================================================================
// File: FX_Modules/PhysicalResonatorProcessor.cpp
//================================================================================
#include "PhysicalResonatorProcessor.h"

// ===================== ExcitationGenerator Implementation =====================
ExcitationGenerator::ExcitationGenerator() {
    noiseGen.setType(DSPUtils::NoiseGenerator::NoiseType::White);
    // Initialization of colorFilter is now handled in prepare().
}

void ExcitationGenerator::prepare(const juce::dsp::ProcessSpec& spec) {
    sampleRate = spec.sampleRate;
    transientDetector.prepare(spec);
    spectralAnalyzer.prepare(spec);
    colorFilter.prepare(spec);

    // FIX: Initialize colorFilter using JUCE 8 API (setParameters).
    juce::dsp::StateVariableTPTFilterParameters params;
    params.type = juce::dsp::StateVariableTPTFilterType::lowpass;
    params.cutoffFrequency = 5000.0f;
    params.Q = 0.707f; // Standard Butterworth Q
    colorFilter.setParameters(params);

    burstEnvelope.setSampleRate(sampleRate);

    // Initialize ADSR defaults
    juce::ADSR::Parameters adsrParams;
    adsrParams.attack = 0.001f;
    adsrParams.decay = 0.05f;
    adsrParams.sustain = 0.0f;
    adsrParams.release = 0.01f;
    burstEnvelope.setParameters(adsrParams);

    smoothedExciteType.reset(sampleRate, 0.05);
    smoothedSensitivity.reset(sampleRate, 0.05);
    reset();
}

void ExcitationGenerator::reset() {
    transientDetector.reset();
    spectralAnalyzer.reset();
    colorFilter.reset();
    burstEnvelope.reset();
    smoothedExciteType.setCurrentAndTargetValue(0.5f);
    smoothedSensitivity.setCurrentAndTargetValue(0.5f);
}

// FIX: Enhanced excitation generator using a triggered ADSR for distinct impulses.
void ExcitationGenerator::process(const juce::dsp::AudioBlock<float>& inputBlock, juce::dsp::AudioBlock<float>& outputExcitationBlock, const ExcitationParams& params) {
    noiseGen.setType(params.noiseType == 1 ? DSPUtils::NoiseGenerator::NoiseType::Pink : DSPUtils::NoiseGenerator::NoiseType::White);

    smoothedExciteType.setTargetValue(params.exciteType);
    smoothedSensitivity.setTargetValue(params.sensitivity);

    // Update ADSR parameters
    juce::ADSR::Parameters adsrParams;
    adsrParams.attack = params.attack > 0 ? params.attack : 0.001f;
    adsrParams.decay = params.decay > 0 ? params.decay : 0.05f;
    adsrParams.sustain = params.sustain >= 0 ? params.sustain : 0.0f;
    adsrParams.release = params.release > 0 ? params.release : 0.01f;
    burstEnvelope.setParameters(adsrParams);


    int numSamples = (int)inputBlock.getNumSamples();
    int numChannels = (int)inputBlock.getNumChannels();

    // FIX: Update colorFilter using JUCE 8 API.
    auto filterParams = colorFilter.getParameters();
    filterParams.cutoffFrequency = 5000.0f;
    filterParams.Q = 0.707f; // Corresponds to the previous resonance setting of 0.7f
    colorFilter.setParameters(filterParams);

    const float triggerThreshold = 0.3f;

    for (int i = 0; i < numSamples; ++i) {
        float currentExciteType = smoothedExciteType.getNextValue();
        float currentSensitivity = smoothedSensitivity.getNextValue();

        // 1. Transient Detection (Mono analysis)
        float monoSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            monoSample += inputBlock.getSample(ch, i);
        if (numChannels > 0) monoSample /= (float)numChannels;

        transientDetector.processSample(monoSample);
        float transientLevel = transientDetector.getTransientValue();

        // 2. Envelope Generation (Triggering logic)
        float scaledTransient = transientLevel * currentSensitivity * 2.0f;

        if (!burstEnvelope.isActive() && scaledTransient > triggerThreshold) {
            burstEnvelope.noteOn();
        }

        float env = burstEnvelope.getNextSample();

        // 3. Excitation Signal Synthesis
        for (int ch = 0; ch < numChannels; ++ch) {
            // Source A: Continuous (Input Audio)
            float continuous = inputBlock.getSample(ch, i) * currentSensitivity;

            // Source B: Impulsive (Noise Burst)
            float noise = noiseGen.getNextSample();
            // colorFilter was prepared with the multi-channel spec, so we use the channel index 'ch'.
            float filteredNoise = colorFilter.processSample(ch, noise);
            float impulsive = filteredNoise * env;

            // Crossfade (Excite Type: 0.0 = Continuous, 1.0 = Impulsive)
            float excitation = (continuous * (1.0f - currentExciteType)) + (impulsive * currentExciteType);

            outputExcitationBlock.setSample(ch, i, excitation * 1.5f);
        }
    }
}


// ===================== ModalResonator Implementation =====================
// (ModalResonator implementation is unchanged as it correctly uses StateVariableTPTFilter)
void ModalResonator::prepare(const juce::dsp::ProcessSpec& spec) {
    sampleRate = spec.sampleRate;
    initializeMaterialTables();

    if (channelFilters.size() != spec.numChannels)
        channelFilters.resize(spec.numChannels);

    // Prepare filters as MONO.
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    for (auto& bank : channelFilters) {
        for (auto& f : bank) {
            f.prepare(monoSpec);
            // FIX: Initialize TPT filter using JUCE 8 API.
            juce::dsp::StateVariableTPTFilterParameters params;
            params.type = juce::dsp::StateVariableTPTFilterType::bandpass;
            // Initialize to safe defaults
            params.cutoffFrequency = 1000.0f;
            params.Q = 1.0f;
            f.setParameters(params);
        }
    }
    reset();
}

void ModalResonator::reset() {
    for (auto& bank : channelFilters)
        for (auto& f : bank) f.reset();
}

void ModalResonator::initializeMaterialTables() {
    if (tablesInitialized) return;

    // (Data remains the same as provided in the prompt...)
    // Wood (Marimba-like)
    float woodRatios[] = { 1, 4, 9, 16, 25, 36, 49, 64, 81, 100, 121, 144, 169, 196, 225, 256, 289, 324, 361, 400, 441, 484, 529, 576 };
    float woodGains[] = { 1, 0.6f, 0.3f, 0.2f, 0.15f, 0.1f, 0.08f, 0.06f, 0.05f, 0.04f, 0.035f, 0.03f, 0.028f, 0.025f, 0.02f, 0.018f, 0.016f, 0.014f, 0.012f, 0.01f, 0.009f, 0.008f, 0.007f, 0.006f };
    float woodQs[] = { 50, 70, 90, 110, 130, 150, 170, 190, 210, 230, 250, 270, 290, 310, 330, 350, 370, 390, 410, 430, 450, 470, 490, 510 };

    // Metal (Vibraphone-like)
    float metalRatios[] = { 1, 3.95f, 8.8f, 15.5f, 24.0f, 34.2f, 46.0f, 59.8f, 75.7f, 93.7f, 113.8f, 136.0f, 160.3f, 186.7f, 215.2f, 245.8f, 278.5f, 313.3f, 340.2f, 380.0f, 415.5f, 450.1f, 490.2f, 530.8f };
    float metalGains[] = { 0.9f, 1.0f, 0.8f, 0.7f, 0.6f, 0.5f, 0.45f, 0.40f, 0.38f, 0.36f, 0.34f, 0.32f, 0.30f, 0.28f, 0.26f, 0.24f, 0.22f, 0.20f, 0.18f, 0.16f, 0.15f, 0.14f, 0.13f, 0.12f };
    float metalQs[] = { 400, 600, 800, 1000, 1200, 1400, 1600, 1800, 1700, 1600, 1500, 1400, 1300, 1200, 1100, 1000, 950, 900, 850, 800, 780, 760, 740, 720 };

    // Glass/Bell (Highly inharmonic)
    float glassRatios[] = { 1, 1.5f, 2.0f, 2.76f, 3.00f, 3.50f, 4.2f, 4.9f, 5.7f, 6.6f, 7.6f, 8.7f, 9.9f, 11.2f, 12.6f, 14.1f, 15.7f, 17.4f, 19.2f, 21.1f, 23.1f, 25.2f, 27.4f, 29.7f };
    float glassGains[] = { 0.8f, 1.0f, 0.9f, 0.75f, 0.85f, 0.6f, 0.55f, 0.50f, 0.45f, 0.40f, 0.36f, 0.33f, 0.30f, 0.27f, 0.24f, 0.22f, 0.20f, 0.18f, 0.16f, 0.15f, 0.14f, 0.13f, 0.12f, 0.11f };
    float glassQs[] = { 1000, 1500, 2000, 2500, 3000, 3500, 3400, 3300, 3200, 3100, 3000, 2900, 2800, 2700, 2600, 2500, 2400, 2300, 2200, 2100, 2050, 2000, 1950, 1900 };

    for (int i = 0; i < NUM_MODES; ++i) {
        woodData.ratios[i] = woodRatios[i]; woodData.gains[i] = woodGains[i]; woodData.qs[i] = woodQs[i];
        metalData.ratios[i] = metalRatios[i]; metalData.gains[i] = metalGains[i]; metalData.qs[i] = metalQs[i];
        glassData.ratios[i] = glassRatios[i]; glassData.gains[i] = glassGains[i]; glassData.qs[i] = glassQs[i];
    }
    tablesInitialized = true;
}

void ModalResonator::computeModeParams(float tuneHz, float structure, float brightness, float damping, float position) {
    // Structure: 0.0 Wood -> 0.5 Metal -> 1.0 Glass
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    const float split = 0.5f;

    float tA = juce::jlimit(0.0f, 1.0f, structure / split);
    float tB = juce::jlimit(0.0f, 1.0f, (structure - split) / (1.0f - split));

    for (int m = 0; m < NUM_MODES; ++m) {
        float r, g, q;

        if (structure < split) { // Wood to Metal
            r = lerp(woodData.ratios[m], metalData.ratios[m], tA);
            g = lerp(woodData.gains[m], metalData.gains[m], tA);
            q = lerp(woodData.qs[m], metalData.qs[m], tA);
        }
        else { // Metal to Glass
            r = lerp(metalData.ratios[m], glassData.ratios[m], tB);
            g = lerp(metalData.gains[m], glassData.gains[m], tB);
            q = lerp(metalData.qs[m], glassData.qs[m], tB);
        }

        // Apply Modifiers

        // Brightness
        float brightAtten = std::pow(juce::jmap(brightness, 0.1f, 1.0f), (float)m * 0.1f);

        // Damping: Scales Q (0.0 = short decay, 1.0 = long decay).
        // FIX: Adjusted mapping to be intuitive (0=short, 1=long).
        float dampingScale = std::pow(10.0f, juce::jmap(damping, 0.0f, 1.0f, -1.5f, 1.0f));

        // Position
        float pos = position * 2.0f;
        if (pos > 1.0f) pos = 2.0f - pos; // 0 (edge) to 1 (center)

        float posShape = 1.0f;
        if (pos > 0.8f) { // Near center, attenuate even harmonics
            if ((m + 1) % 2 == 0)
                posShape = 1.0f - (pos - 0.8f) * 5.0f;
        }

        // Final Calculations and Safety Clamping
        modeFreqs[m] = tuneHz * r;
        modeGains[m] = g * brightAtten * posShape;
        modeQs[m] = q * dampingScale;

        modeFreqs[m] = juce::jlimit(20.0f, (float)sampleRate * 0.49f, modeFreqs[m]);
        // Ensure Q is >= 0.5 for stability and safe division.
        modeQs[m] = juce::jlimit(0.5f, 10000.0f, modeQs[m]);
    }
}

void ModalResonator::process(const juce::dsp::AudioBlock<float>& excitationBlock,
    juce::dsp::AudioBlock<float>& outputBlock,
    float tune, float structure, float brightness, float damping, float position) {

    if (channelFilters.empty()) return;

    // Map normalized tune to Hz
    float tuneHz = tuneToHz(tune);
    computeModeParams(tuneHz, structure, brightness, damping, position);

    int numChannels = (int)outputBlock.getNumChannels();
    int numSamples = (int)outputBlock.getNumSamples();

    // Update filter coefficients
    // FIX: Update using JUCE 8 API (setParameters). This resolves the 'setQ' error (C2039).
    for (int ch = 0; ch < numChannels; ++ch) {
        if ((size_t)ch >= channelFilters.size()) continue;
        auto& bank = channelFilters[(size_t)ch];
        for (int m = 0; m < NUM_MODES; ++m) {
            auto params = bank[m].getParameters();
            params.cutoffFrequency = modeFreqs[m];
            // In JUCE 8 StateVariableTPTFilterParameters, the parameter is 'Q'.
            params.Q = modeQs[m];
            bank[m].setParameters(params);
        }
    }

    // Process Loop
    for (int i = 0; i < numSamples; ++i) {
        for (int ch = 0; ch < numChannels; ++ch) {
            if ((size_t)ch >= channelFilters.size()) continue;

            float in = excitationBlock.getSample(ch, i);
            float acc = 0.0f;
            auto& bank = channelFilters[(size_t)ch];

            // Sum the output of all modes
            for (int m = 0; m < NUM_MODES; ++m) {
                // CRITICAL FIX: Q-Normalization (Blueprint Section 2.2.3).
                // TPT Bandpass gain is proportional to Q. We normalize the input to prevent overload.
                float normalizedInput = in / modeQs[m];

                // Process the input through the filter.
                // JUCE 8 TPT filters require the channel index. Since filters are prepared as MONO, we pass 0.
                float filtered = bank[m].processSample(0, normalizedInput);
                acc += filtered * modeGains[m];
            }

            // Global scaling factor.
            const float globalGain = 0.8f;
            outputBlock.setSample(ch, i, acc * globalGain);
        }
    }
}

// ===================== SympatheticStringResonator Implementation =====================
// (SympatheticStringResonator implementation is unchanged as it correctly uses FirstOrderTPTFilter)
void SympatheticStringResonator::prepare(const juce::dsp::ProcessSpec& spec) {
    sampleRate = spec.sampleRate;
    maxDelaySamples = (int)(sampleRate / 20.0) + 100;

    channelDelays.resize(spec.numChannels);
    channelFilters.resize(spec.numChannels);
    feedbackGains.resize(spec.numChannels);

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    for (size_t ch = 0; ch < spec.numChannels; ++ch) {
        for (int s = 0; s < NUM_STRINGS; ++s) {
            channelDelays[ch][s].setMaximumDelayInSamples(maxDelaySamples);
            channelDelays[ch][s].prepare(spec);
            channelFilters[ch][s].prepare(monoSpec);

            // FIX: Initialize FirstOrderTPTFilter using JUCE 8 API.
            juce::dsp::FirstOrderTPTFilterParameters params;
            params.type = juce::dsp::FirstOrderTPTFilterType::lowpass;
            params.cutoffFrequency = 20000.0f; // Default high cutoff
            channelFilters[ch][s].setParameters(params);

            feedbackGains[ch][s] = 0.0f;
        }
    }

    // FIX: Initialize smoothers. 50ms helps stabilize abrupt changes (e.g., Structure parameter).
    for (int s = 0; s < NUM_STRINGS; ++s) {
        smoothedDelayTimes[s].reset(sampleRate, 0.05);
    }
    reset();
}

void SympatheticStringResonator::reset() {
    for (auto& bank : channelDelays)
        for (auto& d : bank) d.reset();
    for (auto& bank : channelFilters)
        for (auto& f : bank) f.reset();
    for (auto& bank : feedbackGains)
        std::fill(bank.begin(), bank.end(), 0.0f);

    // FIX: Reset smoothers to a safe value
    for (int s = 0; s < NUM_STRINGS; ++s) {
        smoothedDelayTimes[s].setCurrentAndTargetValue((float)maxDelaySamples / 4.0f);
    }
}

void SympatheticStringResonator::updateTunings(float structure) {
    // (Implementation remains the same)
    if (structure < 0.2f) { // Unison/Octaves
        currentRatios = { 1.0f, 2.0f, 0.5f, 4.0f, 1.01f, 0.99f };
    }
    else if (structure < 0.4f) { // Fifths
        currentRatios = { 1.0f, 1.5f, 2.0f, 3.0f, 0.5f, 0.75f };
    }
    else if (structure < 0.6f) { // Major Triad
        currentRatios = { 1.0f, 1.25f, 1.5f, 2.0f, 2.5f, 3.0f };
    }
    else if (structure < 0.8f) { // Minor Triad
        currentRatios = { 1.0f, 1.2f, 1.5f, 2.0f, 2.4f, 3.0f };
    }
    else { // Suspended 4th
        currentRatios = { 1.0f, 1.333f, 1.5f, 2.0f, 2.666f, 3.0f };
    }
}

void SympatheticStringResonator::process(const juce::dsp::AudioBlock<float>& excitationBlock, juce::dsp::AudioBlock<float>& outputBlock,
    float tune, float structure, float brightness, float damping, float position) {

    float tuneHz = tuneToHz(tune);
    updateTunings(structure);

    // Damping maps to feedback gain. Capped slightly below 1.0 for stability.
    float feedbackGain = std::pow(damping, 0.3f) * 0.998f;

    // Brightness maps to LPF cutoff
    float brightnessCutoff = juce::jmap(brightness, 500.0f, (float)sampleRate * 0.45f);

    int numChannels = (int)outputBlock.getNumChannels();
    int numSamples = (int)outputBlock.getNumSamples();

    // Update parameters: Set Targets for delay smoothers
    for (int s = 0; s < NUM_STRINGS; ++s) {
        float freq = tuneHz * currentRatios[s];
        freq = juce::jlimit(20.0f, (float)sampleRate * 0.49f, freq);
        float targetDelaySamples = (float)sampleRate / freq;

        // Tuning compensation: Subtract estimated filter delay (approx 0.5 samples for 1st order TPT)
        float delayTimeSamples = targetDelaySamples - 0.5f;
        delayTimeSamples = juce::jlimit(1.0f, (float)maxDelaySamples - 5.0f, delayTimeSamples);

        smoothedDelayTimes[s].setTargetValue(delayTimeSamples);
    }

    // Update filter cutoffs
    for (int ch = 0; ch < numChannels; ++ch) {
        if ((size_t)ch >= channelFilters.size()) continue;
        for (int s = 0; s < NUM_STRINGS; ++s) {
            // FIX: Update FirstOrderTPTFilter using JUCE 8 API.
            auto params = channelFilters[ch][s].getParameters();
            params.cutoffFrequency = brightnessCutoff;
            channelFilters[ch][s].setParameters(params);
        }
    }

    // Process Loop (Comb Filter implementation)
    for (int i = 0; i < numSamples; ++i) {

        // FIX: Get the next smoothed delay times and apply them BEFORE processing the sample.
        for (int s = 0; s < NUM_STRINGS; ++s) {
            float currentDelay = smoothedDelayTimes[s].getNextValue();
            for (int ch = 0; ch < numChannels; ++ch) {
                if ((size_t)ch >= channelDelays.size()) continue;
                channelDelays[ch][s].setDelay(currentDelay);
            }
        }

        for (int ch = 0; ch < numChannels; ++ch) {
            if ((size_t)ch >= channelDelays.size()) continue;

            float in = excitationBlock.getSample(ch, i);
            float acc = 0.0f;

            for (int s = 0; s < NUM_STRINGS; ++s) {
                // 1. Read from delay line
                float delayedSample = channelDelays[ch][s].popSample(ch);

                // 2. Apply damping (LPF) (MONO processing, channel 0)
                float dampedSample = channelFilters[ch][s].processSample(0, delayedSample);

                // 3. Calculate new input (Excitation + Feedback)
                float feedbackIn = feedbackGains[ch][s];

                // FIX: Introduce non-linearity (soft clipping) to tame resonances.
                float feedbackClipped = juce::dsp::FastMathApproximations::tanh(feedbackIn * 1.5f);

                float newSample = in + feedbackClipped;

                // 4. Write to delay line
                channelDelays[ch][s].pushSample(ch, newSample);

                // 5. Update feedback storage (use the unclipped damped sample for feedback path)
                feedbackGains[ch][s] = dampedSample * feedbackGain;

                // Output
                acc += dampedSample;
            }

            // Global scaling
            outputBlock.setSample(ch, i, acc * 0.3f);
        }
    }
}


// ===================== StringResonator (Karplus-Strong) Implementation =====================
void StringResonator::prepare(const juce::dsp::ProcessSpec& spec) {
    sampleRate = spec.sampleRate;
    maxDelaySamples = (int)(sampleRate / 20.0) + 100;

    channelDelays.resize(spec.numChannels);
    channelDampingFilters.resize(spec.numChannels);
    channelDispersionFilters.resize(spec.numChannels);
    feedback.resize(spec.numChannels, 0.0f);

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    for (size_t ch = 0; ch < spec.numChannels; ++ch) {
        channelDelays[ch].setMaximumDelayInSamples(maxDelaySamples);
        channelDelays[ch].prepare(spec);

        // Initialize Damping Filter (FirstOrderTPTFilter)
        channelDampingFilters[ch].prepare(monoSpec);
        // FIX: Initialize using JUCE 8 API.
        juce::dsp::FirstOrderTPTFilterParameters dampingParams;
        dampingParams.type = juce::dsp::FirstOrderTPTFilterType::lowpass;
        dampingParams.cutoffFrequency = 20000.0f;
        channelDampingFilters[ch].setParameters(dampingParams);

        // Initialize Dispersion Filter (IIR::Filter)
        channelDispersionFilters[ch].prepare(monoSpec);
        // FIX: Initialize using IIR::Coefficients::makeAllPass().
        // This replaces the initialization logic for the non-existent AllpassTPTFilter.
        *channelDispersionFilters[ch].getCoefficients() = *juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, 1000.0f, 0.707f);
    }

    // FIX: Initialize smoother.
    smoothedDelayTime.reset(sampleRate, 0.05);
    reset();
}

void StringResonator::reset() {
    for (auto& d : channelDelays) d.reset();
    for (auto& f : channelDampingFilters) f.reset();
    for (auto& f : channelDispersionFilters) f.reset();
    std::fill(feedback.begin(), feedback.end(), 0.0f);
    // FIX: Reset smoother
    smoothedDelayTime.setCurrentAndTargetValue((float)maxDelaySamples / 4.0f);
}

void StringResonator::process(const juce::dsp::AudioBlock<float>& excitationBlock, juce::dsp::AudioBlock<float>& outputBlock,
    float tune, float structure, float brightness, float damping, float position) {

    float tuneHz = tuneToHz(tune);
    tuneHz = juce::jlimit(20.0f, (float)sampleRate * 0.49f, tuneHz);
    float targetDelaySamples = (float)sampleRate / tuneHz;

    // Damping (Feedback Gain)
    float feedbackGain = std::pow(damping, 0.5f) * 0.998f;

    // Brightness (Damping Filter Cutoff)
    float brightnessCutoff = juce::jmap(brightness, 1000.0f, (float)sampleRate * 0.45f);

    // Structure (Inharmonicity/Dispersion) - maps to all-pass filter Q.
    float dispersionQ = juce::jmap(structure, 0.01f, 2.0f);

    // FIX (NO SOUND): Tuning compensation. Subtract estimated filter delay (LPF+APF+Interpolation).
    float filterDelayEstimate = 2.5f;
    float delayTimeSamples = targetDelaySamples - filterDelayEstimate;
    delayTimeSamples = juce::jlimit(1.0f, (float)maxDelaySamples - 5.0f, delayTimeSamples);

    // FIX: Set target for the smoother.
    smoothedDelayTime.setTargetValue(delayTimeSamples);


    int numChannels = (int)outputBlock.getNumChannels();
    int numSamples = (int)outputBlock.getNumSamples();

    // Update parameters
    for (int ch = 0; ch < numChannels; ++ch) {
        if ((size_t)ch >= channelDelays.size()) continue;

        // FIX: Update Damping Filter using JUCE 8 API.
        auto dampingParams = channelDampingFilters[ch].getParameters();
        dampingParams.cutoffFrequency = brightnessCutoff;
        channelDampingFilters[ch].setParameters(dampingParams);

        // FIX: Update Dispersion Filter (IIR::Filter).
        // Coefficients must be updated when parameters change (Frequency or Q).
        *channelDispersionFilters[ch].getCoefficients() = *juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, tuneHz, dispersionQ);
    }

    // Process Loop (Extended Karplus-Strong)
    for (int i = 0; i < numSamples; ++i) {

        // FIX: Get the next smoothed delay time and apply it BEFORE processing.
        float currentDelay = smoothedDelayTime.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch) {
            if ((size_t)ch >= channelDelays.size()) continue;
            channelDelays[ch].setDelay(currentDelay);
        }

        for (int ch = 0; ch < numChannels; ++ch) {
            if ((size_t)ch >= channelDelays.size()) continue;

            float in = excitationBlock.getSample(ch, i);

            // 1. Read from delay line
            float delayedSample = channelDelays[ch].popSample(ch);

            // 2. Apply filters in the feedback loop
            // Damping (Brightness). TPT filters prepared as MONO use channel 0.
            float dampedSample = channelDampingFilters[ch].processSample(0, delayedSample);

            // Dispersion (Inharmonicity). IIR filters prepared as MONO use the overload without the channel index.
            float dispersedSample = channelDispersionFilters[ch].processSample(dampedSample);

            // 3. Calculate new input (Excitation + Feedback)
            float feedbackIn = feedback[ch];

            // FIX: Introduce non-linearity (soft clipping) for stability.
            float feedbackClipped = juce::dsp::FastMathApproximations::tanh(feedbackIn * 1.2f);

            float newSample = in + feedbackClipped;

            // 4. Write to delay line
            channelDelays[ch].pushSample(ch, newSample);

            // 5. Update feedback storage
            feedback[ch] = dispersedSample * feedbackGain;

            // Output (Position ignored for simplicity here)
            outputBlock.setSample(ch, i, dispersedSample * 0.8f);
        }
    }
}



// ===================== PhysicalResonatorProcessor Implementation =====================
// (The main processor implementation remains unchanged)
PhysicalResonatorProcessor::PhysicalResonatorProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)), mainApvts(apvts) {

    // Define Parameter IDs
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
    // ADSR IDs (now actively used)
    attackParamId = slotPrefix + "ATTACK";
    decayParamId = slotPrefix + "DECAY";
    sustainParamId = slotPrefix + "SUSTAIN";
    releaseParamId = slotPrefix + "RELEASE";
}

void PhysicalResonatorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::uint32 numChannels = (juce::uint32)std::max(getTotalNumInputChannels(), getTotalNumOutputChannels());
    if (numChannels == 0) numChannels = 2;

    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, numChannels };

    // Prepare all components
    excitationGenerator.prepare(spec);
    modalResonator.prepare(spec);
    sympatheticResonator.prepare(spec);
    stringResonator.prepare(spec);
    safetyLimiter.prepare(spec);

    // Configure limiter
    safetyLimiter.setThreshold(0.95f);
    safetyLimiter.setRelease(50.0f);

    // Setup buffers
    excitationBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);
    wetOutputBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);

    // Initialize smoothed values
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

void PhysicalResonatorProcessor::releaseResources() {
    reset();
}

void PhysicalResonatorProcessor::reset() {
    // CRITICAL: Added ScopedNoDenormals here as resetting filters can sometimes cause denormals.
    juce::ScopedNoDenormals noDenormals;
    excitationGenerator.reset();
    modalResonator.reset();
    sympatheticResonator.reset();
    stringResonator.reset();
    safetyLimiter.reset();
    instabilityFlag = false;
}

void PhysicalResonatorProcessor::updateResonatorCore(int newModelIndex) {
    if (newModelIndex == currentModelIndex && activeResonator != nullptr) return;

    // Reset the previous one to silence tails when switching
    if (activeResonator) {
        activeResonator->reset();
    }

    currentModelIndex = newModelIndex;

    switch (newModelIndex) {
    case 0: activeResonator = &modalResonator; break;
    case 1: activeResonator = &sympatheticResonator; break;
    case 2: activeResonator = &stringResonator; break;
    default: activeResonator = &modalResonator; break;
    }
}

bool PhysicalResonatorProcessor::checkAndHandleInstability(float sampleValue) {
    if (std::isnan(sampleValue) || std::isinf(sampleValue) || std::abs(sampleValue) > 100.0f) {
        if (!instabilityFlag) {
            // DBG("PhysicalResonator: Numerical instability detected. Resetting.");
            instabilityFlag = true;
            reset(); // Reset all DSP components
        }
        return true;
    }
    return false;
}


void PhysicalResonatorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    // CRITICAL: Ensure denormals are disabled for performance, especially with high-Q filters and feedback.
    juce::ScopedNoDenormals noDenormals;
    auto totalIn = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();

    for (auto i = totalIn; i < totalOut; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    int numSamples = buffer.getNumSamples();
    int numChannels = std::max(totalIn, totalOut);

    if (numChannels == 0) return;

    instabilityFlag = false;

    // 1. Update Model Selection
    int modelIndex = (int)mainApvts.getRawParameterValue(modelParamId)->load();
    updateResonatorCore(modelIndex);

    if (!activeResonator) return;

    // 2. Update Smoothed Parameter Targets
    auto getParam = [&](const juce::String& id) {
        if (auto* p = mainApvts.getRawParameterValue(id)) return p->load();
        return 0.0f;
        };

    smoothedTune.setTargetValue(getParam(tuneParamId));
    smoothedStructure.setTargetValue(getParam(structureParamId));
    smoothedBrightness.setTargetValue(getParam(brightnessParamId));
    smoothedDamping.setTargetValue(getParam(dampingParamId));
    smoothedPosition.setTargetValue(getParam(positionParamId));
    smoothedMix.setTargetValue(getParam(mixParamId));

    // Ensure buffers are adequate size
    if (excitationBuffer.getNumSamples() < numSamples || excitationBuffer.getNumChannels() < numChannels) {
        excitationBuffer.setSize(numChannels, numSamples, false, true, true);
        wetOutputBuffer.setSize(numChannels, numSamples, false, true, true);
    }

    // Setup AudioBlocks
    juce::dsp::AudioBlock<float> dryBlock(buffer);
    juce::dsp::AudioBlock<float> excitationBlock(excitationBuffer);
    juce::dsp::AudioBlock<float> wetBlock(wetOutputBuffer);

    // Create sub-blocks
    auto activeDry = dryBlock.getSubBlock(0, (size_t)numSamples);
    if (totalIn > 0) {
        activeDry = activeDry.getSubsetChannelBlock(0, (size_t)totalIn);
    }

    auto activeExcite = excitationBlock.getSubBlock(0, (size_t)numSamples).getSubsetChannelBlock(0, (size_t)numChannels);
    auto activeWet = wetBlock.getSubBlock(0, (size_t)numSamples).getSubsetChannelBlock(0, (size_t)numChannels);

    activeExcite.clear();
    activeWet.clear();

    // 3. Generate Excitation Signal
    ExcitationGenerator::ExcitationParams ep;
    ep.exciteType = getParam(exciteTypeParamId);
    ep.sensitivity = getParam(sensitivityParamId);
    ep.noiseType = (int)getParam(noiseTypeParamId);
    // Pass ADSR parameters
    ep.attack = getParam(attackParamId);
    ep.decay = getParam(decayParamId);
    ep.sustain = getParam(sustainParamId);
    ep.release = getParam(releaseParamId);


    if (totalIn > 0) {
        excitationGenerator.process(activeDry, activeExcite, ep);
    }

    // 4. Process through Resonator Core (Sample-by-sample for smooth parameter updates)
    // This structure is maintained, but the resonators are now efficient enough to handle it.
    for (int i = 0; i < numSamples; ++i) {
        float tune = smoothedTune.getNextValue();
        float structure = smoothedStructure.getNextValue();
        float brightness = smoothedBrightness.getNextValue();
        float damping = smoothedDamping.getNextValue();
        float position = smoothedPosition.getNextValue();

        // Create single-sample blocks
        auto exciteSample = activeExcite.getSubBlock((size_t)i, 1);
        auto wetSample = activeWet.getSubBlock((size_t)i, 1);

        // Process the sample
        activeResonator->process(exciteSample, wetSample, tune, structure, brightness, damping, position);
    }

    // 5. Safety Check and Limiting
    for (int ch = 0; ch < numChannels; ++ch) {
        for (int i = 0; i < numSamples; ++i) {
            if (checkAndHandleInstability(activeWet.getSample(ch, i))) {
                activeWet.clear();
                break;
            }
        }
        if (instabilityFlag) break;
    }

    if (!instabilityFlag) {
        safetyLimiter.process(juce::dsp::ProcessContextReplacing<float>(activeWet));
    }

    // 6. Wet/Dry Mixing (Equal Power)
    for (int i = 0; i < numSamples; ++i) {
        float mix = smoothedMix.getNextValue();
        float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
        float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);

        for (int ch = 0; ch < totalOut; ++ch) {
            float dry = (ch < totalIn) ? buffer.getSample(ch, i) : 0.0f;
            float wet = (ch < numChannels && ch < (int)activeWet.getNumChannels()) ? activeWet.getSample(ch, i) : 0.0f;

            float out = dry * dryGain + wet * wetGain;

            buffer.setSample(ch, i, out);
        }
    }
}